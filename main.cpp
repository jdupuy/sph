////////////////////////////////////////////////////////////////////////////////
// \author   Jonathan Dupuy
// \brief    An SPH solver, running on the GPU via OpenGL4.2 and GLSL420.
//
////////////////////////////////////////////////////////////////////////////////

// enable gui
//#define _ANT_ENABLE

// GL libraries
#include "glew.hpp"
#include "GL/freeglut.h"

#ifdef _ANT_ENABLE
#	include "AntTweakBar.h"
#endif // _ANT_ENABLE

// Custom libraries
#include "Algebra.hpp"      // Basic algebra library
#include "Transform.hpp"    // Basic transformations
#include "Framework.hpp"    // utility classes/functions

// Standard librabries
#include <cmath>
#include <iostream>
#include <sstream>
#include <vector>
#include <stdexcept>


////////////////////////////////////////////////////////////////////////////////
// Global variables
//
////////////////////////////////////////////////////////////////////////////////

// Constants
const float  PI = 3.14159265f;
const GLuint MAX_PARTICLE_COUNT = 256u*1024u; // maximum number of particless
const Vector3 SIMULATION_DOMAIN = Vector3(50.0f,50.0f,50.0f); // centimeters
const Vector3 SIM_BOUNDS_MIN    = -0.5f*SIMULATION_DOMAIN;
const float MIN_SMOOTHING_LENGTH = 1.25f;                   // centimeters
const Vector3 BUCKET_3D_MAX   = (SIMULATION_DOMAIN/MIN_SMOOTHING_LENGTH).Ceil()
                              + Vector3(2,2,2); // border cells
const GLuint  BUCKET_1D_MAX   = BUCKET_3D_MAX[0]
                              * BUCKET_3D_MAX[1]
                              * BUCKET_3D_MAX[2];

enum // OpenGLNames
{
	// buffers
	BUFFER_POS_DENSITIES_PING = 0,
	BUFFER_POS_DENSITIES_PONG,
	BUFFER_VELOCITIES_PING,
	BUFFER_VELOCITIES_PONG,
	BUFFER_HEAD,
	BUFFER_LIST,
	BUFFER_CUBE_VERTICES,
	BUFFER_CUBE_INDEXES,
	BUFFER_COUNT,

	// vertex arrays
	VERTEX_ARRAY_BUCKET = 0,
	VERTEX_ARRAY_CUBE,
	VERTEX_ARRAY_POS_DENSITY_PING,
	VERTEX_ARRAY_POS_DENSITY_PONG,
	VERTEX_ARRAY_FLUID_RENDER_PING,
	VERTEX_ARRAY_FLUID_RENDER_PONG,
	VERTEX_ARRAY_COUNT,

	// textures
	TEXTURE_HEAD = 0,
	TEXTURE_LIST,
	TEXTURE_POS_DENSITIES_PING,
	TEXTURE_POS_DENSITIES_PONG,
	TEXTURE_VELOCITIES_PING,
	TEXTURE_VELOCITIES_PONG,
	TEXTURE_COUNT,

	// transform feedbacks
	TRANSFORM_FEEDBACK_HEAD = 0,
	TRANSFORM_FEEDBACK_LIST,
	TRANSFORM_FEEDBACK_DENSITY_PING,
	TRANSFORM_FEEDBACK_DENSITY_PONG,
	TRANSFORM_FEEDBACK_PARTICLE_PING,
	TRANSFORM_FEEDBACK_PARTICLE_PONG,
	TRANSFORM_FEEDBACK_COUNT,

	// programs
	PROGRAM_DENSITY = 0,
	PROGRAM_BUCKET,
	PROGRAM_GRID,
	PROGRAM_FORCE,
	PROGRAM_FLUID_RENDER,
	PROGRAM_CUBE_RENDER,
	PROGRAM_BUCKET_RENDER,
	PROGRAM_COUNT
};

// OpenGL objects
GLuint *buffers      = NULL;
GLuint *vertexArrays = NULL;
GLuint *textures     = NULL;
GLuint *programs     = NULL;
GLuint *transformFeedbacks = NULL;

// SPH variables
GLfloat smoothingLength = MIN_SMOOTHING_LENGTH*2.0f;  // centimeters
GLfloat particleMass    = 1.0f;            // grams
GLuint particleCount    = MAX_PARTICLE_COUNT / 32;           // number of particles
GLuint cellCount        = 0;    // number of cells
Vector3 gravityVector   = Vector3(0,-1,0); // gravity direction
GLfloat deltaT          = 0.0065f;
GLint sphPingPong       = 0;
GLfloat restDensity     = 10.0f;
GLfloat k               = 1.5f;
GLfloat mu              = 1.0f;
bool renderBucket       = false;


// Tools
Affine invCameraWorld       = Affine::Translation(Vector3(0,0,-100));
Projection cameraProjection = Projection::Perspective(PI*0.35f,
                                                      1.0f,
                                                      1.0f,
                                                      400.0f);

bool mouseLeft  = false;
bool mouseRight = false;

#ifdef _ANT_ENABLE
double secondsPerFrame = 0.0; // spf
#endif

////////////////////////////////////////////////////////////////////////////////
// Functions
//
////////////////////////////////////////////////////////////////////////////////


// get the size of the 3d bucket
Vector3 get_bucket_3d_size()
{
	return (SIMULATION_DOMAIN/smoothingLength).Ceil() + Vector3(2.0f,2.0f,2.0f);
}


// get the size of the 1d bucket
GLuint get_bucket_1d_size()
{
	Vector3 bucket3d = get_bucket_3d_size();
	return bucket3d[0]*bucket3d[1]*bucket3d[2];
}


// compute grid params and send to programs
void set_grid_params()
{
	// compute coeffs
	Vector3 bucket3d = get_bucket_3d_size();
	Vector3 bucket1dCoeffs(1,
	                       bucket3d[0],
	                       bucket3d[0]*bucket3d[1]);

	// set global variables
	cellCount = get_bucket_1d_size();

	// set 3d
	glProgramUniform2i(programs[PROGRAM_BUCKET_RENDER],
	                   glGetUniformLocation(programs[PROGRAM_BUCKET_RENDER],
	                                        "uBucket3dSize"),
	                   bucket3d[0], bucket3d[1]);

	// set 1d
	glProgramUniform3fv(programs[PROGRAM_DENSITY],
	                    glGetUniformLocation(programs[PROGRAM_DENSITY],
	                                         "uBucket1dCoeffs"),
	                    1,
	                    reinterpret_cast<GLfloat*>(&bucket1dCoeffs));
	glProgramUniform3fv(programs[PROGRAM_GRID],
	                    glGetUniformLocation(programs[PROGRAM_GRID],
	                                         "uBucket1dCoeffs"),
	                    1,
	                    reinterpret_cast<GLfloat*>(&bucket1dCoeffs));
	glProgramUniform3fv(programs[PROGRAM_FORCE],
	                    glGetUniformLocation(programs[PROGRAM_FORCE],
	                                         "uBucket1dCoeffs"),
	                    1,
	                    reinterpret_cast<GLfloat*>(&bucket1dCoeffs));

	// set cell size
	glProgramUniform1f(programs[PROGRAM_DENSITY],
	                   glGetUniformLocation(programs[PROGRAM_DENSITY],
	                                        "uBucketCellSize"),
	                   smoothingLength);
	glProgramUniform1f(programs[PROGRAM_GRID],
	                   glGetUniformLocation(programs[PROGRAM_GRID],
	                                        "uBucketCellSize"),
	                   smoothingLength);
	glProgramUniform1f(programs[PROGRAM_BUCKET_RENDER],
	                   glGetUniformLocation(programs[PROGRAM_BUCKET_RENDER],
	                                        "uBucketCellSize"),
	                    smoothingLength);
	glProgramUniform1f(programs[PROGRAM_FORCE],
	                   glGetUniformLocation(programs[PROGRAM_FORCE],
	                                        "uBucketCellSize"),
	                    smoothingLength);

}


// precompute sph force constant components and send to programs
void set_sph_constants()
{
	GLfloat h2        = smoothingLength*smoothingLength;
	GLfloat h6        = pow(smoothingLength,6.0f); // mind precision !!
	GLfloat h9        = pow(smoothingLength,9.0f);
	GLfloat poly6     = ( 315.0f/h9)/(64.0f*PI);
	GLfloat gradPoly6 = (-945.0f/h9)/(32.0f*PI);
	GLfloat gradSpiky = (-45.0f/h6)/PI;
	GLfloat grad2Viscosity = -gradSpiky; /* = 45.0f/(PI*h6); */
	const Vector3 SIM_MIN  = SIM_BOUNDS_MIN
	                       - Vector3(smoothingLength,
	                                 smoothingLength,
	                                 smoothingLength);

//	std::cout << "h6: " << h6 << std::endl;
//	std::cout << "h9: " << h9 << std::endl;
//	std::cout << "poly6: " << poly6 << std::endl;
//	std::cout << "gradPoly6: " << gradPoly6 << std::endl;
//	std::cout << "gradSpiky: " << gradSpiky << std::endl;
//	std::cout << "grad2Viscosity: " << grad2Viscosity << std::endl;

	// set masses
	glProgramUniform1f(programs[PROGRAM_FORCE],
	                   glGetUniformLocation(programs[PROGRAM_FORCE],
	                                        "uParticleMass"),
	                   particleMass);

	// set uniforms: h
	glProgramUniform1f(programs[PROGRAM_FORCE],
	                   glGetUniformLocation(programs[PROGRAM_FORCE],
	                                        "uSmoothingLength"),
	                   smoothingLength);

	// set uniforms: h2
	glProgramUniform1f(programs[PROGRAM_DENSITY],
	                   glGetUniformLocation(programs[PROGRAM_DENSITY],
	                                        "uSmoothingLengthSquared"),
	                   h2);
	glProgramUniform1f(programs[PROGRAM_FORCE],
	                   glGetUniformLocation(programs[PROGRAM_FORCE],
	                                        "uSmoothingLengthSquared"),
	                   h2);

	// set uniforms: Poly6
	glProgramUniform1f(programs[PROGRAM_DENSITY],
	                   glGetUniformLocation(programs[PROGRAM_DENSITY],
	                                        "uDensityConstants"),
	                   poly6 * particleMass);

	// gradPoly6
	glProgramUniform1f(programs[PROGRAM_FORCE],
	                   glGetUniformLocation(programs[PROGRAM_FORCE],
	                                        "uDensityConstants"),
	                   gradPoly6 * particleMass);

	// spiky
	glProgramUniform1f(programs[PROGRAM_FORCE],
	                   glGetUniformLocation(programs[PROGRAM_FORCE],
	                                        "uPressureConstants"),
	                   -gradSpiky  * particleMass * 0.5f);

	// viscosity
	glProgramUniform1f(programs[PROGRAM_FORCE],
	                   glGetUniformLocation(programs[PROGRAM_FORCE],
	                                        "uViscosityConstants"),
	                   grad2Viscosity * particleMass * mu);

	// rest density
	glProgramUniform1f(programs[PROGRAM_FORCE],
	                   glGetUniformLocation(programs[PROGRAM_FORCE],
	                                        "uRestDensity"),
	                   restDensity);

	// k constant
	glProgramUniform1f(programs[PROGRAM_FORCE],
	                   glGetUniformLocation(programs[PROGRAM_FORCE],
	                                        "uK"),
	                   k);

	// set min bounds of simulation
	glProgramUniform3fv(programs[PROGRAM_DENSITY],
	                    glGetUniformLocation(programs[PROGRAM_DENSITY],
	                                         "uBucketBoundsMin"),
	                    1,
	                    reinterpret_cast<GLfloat *>(
	                    const_cast<Vector3 *>(&SIM_MIN)));
	glProgramUniform3fv(programs[PROGRAM_BUCKET],
	                    glGetUniformLocation(programs[PROGRAM_BUCKET],
	                                         "uBucketBoundsMin"),
	                    1,
	                    reinterpret_cast<GLfloat *>(
	                    const_cast<Vector3 *>(&SIM_MIN)));
	glProgramUniform3fv(programs[PROGRAM_FORCE],
	                    glGetUniformLocation(programs[PROGRAM_FORCE],
	                                         "uBucketBoundsMin"),
	                    1,
	                    reinterpret_cast<GLfloat *>(
	                    const_cast<Vector3 *>(&SIM_MIN)));
	glProgramUniform3fv(programs[PROGRAM_GRID],
	                    glGetUniformLocation(programs[PROGRAM_GRID],
	                                         "uBucketBoundsMin"),
	                    1,
	                    reinterpret_cast<GLfloat *>(
	                    const_cast<Vector3 *>(&SIM_MIN)));
	glProgramUniform3f(programs[PROGRAM_BUCKET_RENDER],
	                   glGetUniformLocation(programs[PROGRAM_BUCKET_RENDER],
	                                        "uBucketBoundsMin"),
	                   SIM_MIN[0]+smoothingLength*0.5f,
	                   SIM_MIN[1]+smoothingLength*0.5f,
	                   SIM_MIN[2]+smoothingLength*0.5f);

	// min bounds of domain
	glProgramUniform3fv(programs[PROGRAM_FORCE],
	                    glGetUniformLocation(programs[PROGRAM_FORCE],
	                                         "uSimBoundsMin"),
	                    1,
	                    &SIM_BOUNDS_MIN[0]);
	glProgramUniform3f(programs[PROGRAM_FORCE],
	                   glGetUniformLocation(programs[PROGRAM_FORCE],
	                                        "uSimBoundsMax"),
	                    SIM_BOUNDS_MIN[0] + SIMULATION_DOMAIN[0],
	                    SIM_BOUNDS_MIN[1] + SIMULATION_DOMAIN[1],
	                    SIM_BOUNDS_MIN[2] + SIMULATION_DOMAIN[2] );

	// build grid
	set_grid_params();
}


// set delta 
void set_delta()
{
	glProgramUniform1f(programs[PROGRAM_FORCE],
	                   glGetUniformLocation(programs[PROGRAM_FORCE],
	                                        "uTicks"),
	                   deltaT);
}


// set direction of the gravity acceleration
void set_gravity_vector()
{
	glProgramUniform3fv(programs[PROGRAM_FORCE],
	                    glGetUniformLocation(programs[PROGRAM_FORCE],
	                                         "uGravityDir"),
	                    1,
	                    &gravityVector[0]);
}


// initialize cells (rasterizer must be disabled)
void init_sph_cells()
{
	glUseProgram(programs[PROGRAM_BUCKET]);
	glBindVertexArray(vertexArrays[VERTEX_ARRAY_BUCKET]);

	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK,
	                        transformFeedbacks[TRANSFORM_FEEDBACK_HEAD]);
	glBeginTransformFeedback(GL_POINTS);
		glDrawArrays(GL_POINTS, 0, cellCount/4 + cellCount%4);
	glEndTransformFeedback();

	glFinish(); // apparently, this is mandatory on AMD11.12

	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK,
	                        transformFeedbacks[TRANSFORM_FEEDBACK_LIST]);
	glBeginTransformFeedback(GL_POINTS);
		glDrawArrays(GL_POINTS, 0, particleCount/4);
	glEndTransformFeedback();

	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
}


// set runtime constants (called only once)
void set_runtime_constant_uniforms()
{

	// set cube
	glProgramUniform4f(programs[PROGRAM_CUBE_RENDER],
	                   glGetUniformLocation(programs[PROGRAM_CUBE_RENDER],
	                                        "uCubeSize"),
	                   SIMULATION_DOMAIN[0],
	                   SIMULATION_DOMAIN[1],
	                   SIMULATION_DOMAIN[2],
	                   1.0f);

	// set images
	glProgramUniform1i(programs[PROGRAM_GRID],
	                   glGetUniformLocation(programs[PROGRAM_GRID],
	                                        "imgHead"),
	                   TEXTURE_HEAD);
	glProgramUniform1i(programs[PROGRAM_GRID],
	                   glGetUniformLocation(programs[PROGRAM_GRID],
	                                        "imgList"),
	                   TEXTURE_LIST);
	glProgramUniform1i(programs[PROGRAM_DENSITY],
	                   glGetUniformLocation(programs[PROGRAM_DENSITY],
	                                        "imgHead"),
	                   TEXTURE_HEAD);
	glProgramUniform1i(programs[PROGRAM_DENSITY],
	                   glGetUniformLocation(programs[PROGRAM_DENSITY],
	                                        "imgList"),
	                   TEXTURE_LIST);
	glProgramUniform1i(programs[PROGRAM_FORCE],
	                   glGetUniformLocation(programs[PROGRAM_FORCE],
	                                        "imgHead"),
	                   TEXTURE_HEAD);
	glProgramUniform1i(programs[PROGRAM_FORCE],
	                   glGetUniformLocation(programs[PROGRAM_FORCE],
	                                        "imgList"),
	                   TEXTURE_LIST);
}


// set particles to grid
void build_grid()
{
	glEnable(GL_RASTERIZER_DISCARD);

	// empty cells
	init_sph_cells();

	// compute
	glUseProgram(programs[PROGRAM_GRID]);
	glBindVertexArray(vertexArrays[VERTEX_ARRAY_POS_DENSITY_PING+sphPingPong]);
		glDrawArrays(GL_POINTS, 0, particleCount);

	glDisable(GL_RASTERIZER_DISCARD);
}


// compute densities
void init_sph_density()
{
	// build grid
	build_grid();

	// compute densities and store ine TF
	glEnable(GL_RASTERIZER_DISCARD);
	glUseProgram(programs[PROGRAM_DENSITY]);
	glBindVertexArray(vertexArrays[VERTEX_ARRAY_POS_DENSITY_PING + sphPingPong]);
	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK,
	                        transformFeedbacks[TRANSFORM_FEEDBACK_DENSITY_PING
	                                           + sphPingPong]);
	// set sampler
	glUniform1i(glGetUniformLocation(programs[PROGRAM_DENSITY],
	                                 "sParticlePos"),
	            TEXTURE_POS_DENSITIES_PING + sphPingPong);

	// perform TF
	glBeginTransformFeedback(GL_POINTS);
		glDrawArrays(GL_POINTS, 0, particleCount);
	glEndTransformFeedback();

	// ping pong
	sphPingPong = 1 - sphPingPong;

	// back to defaults
	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
	glDisable(GL_RASTERIZER_DISCARD);
	glBindVertexArray(0);
}


// initialize the particles
void init_sph_particles()
{
	// variables / constants
	const float PARTICLE_SPACING = 0.6f; // in meters
	GLuint xCnt = SIMULATION_DOMAIN[0]*0.5f / PARTICLE_SPACING;
	GLuint zCnt = SIMULATION_DOMAIN[2]*0.5f / PARTICLE_SPACING;
	GLuint yCnt = particleCount / (xCnt*zCnt)
	            + pow(particleCount % (xCnt*zCnt),0.25f); // rest
	Vector3 min = SIM_BOUNDS_MIN
	            + Vector3(SIMULATION_DOMAIN[0]*0.25f,
	                      6.0f*PARTICLE_SPACING,
	                      SIMULATION_DOMAIN[2]*0.25f);
	std::vector<Vector4> positions;
	std::vector<Vector4> velocities;

	// reserve memory
	positions.reserve(particleCount);
	velocities.reserve(particleCount);

	// set positions
	for(GLuint y=0; y<yCnt; ++y)
		for(GLuint x=0; x<xCnt; ++x)
			for(GLuint z=0; z<zCnt; ++z)
			{
				positions.push_back(Vector4(min[0]+x*PARTICLE_SPACING,
				                            min[1]+y*PARTICLE_SPACING,
				                            min[2]+z*PARTICLE_SPACING,
				                            0));
				velocities.push_back(Vector4::ZERO);
			}

	// send data to buffers
	glBindBuffer(GL_ARRAY_BUFFER,
	             buffers[BUFFER_POS_DENSITIES_PING + sphPingPong]);
		glBufferSubData(GL_ARRAY_BUFFER,
		                0,
		                sizeof(Vector4)*particleCount,
		                &positions[0]);
	glBindBuffer(GL_ARRAY_BUFFER,
	             buffers[BUFFER_VELOCITIES_PING + 1 - sphPingPong]);
		glBufferSubData(GL_ARRAY_BUFFER,
		                0,
		                sizeof(Vector4)*particleCount,
		                &velocities[0]);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// pre compute densities
	init_sph_density();
}


// build transform feedbacks
void set_transform_feedbacks()
{
	// transform feedbacks
	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK,
	                        transformFeedbacks[TRANSFORM_FEEDBACK_HEAD]);
		glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER,
		                  0,
		                  buffers[BUFFER_HEAD],
		                  0,
		                  cellCount * sizeof(GLint));
	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK,
	                        transformFeedbacks[TRANSFORM_FEEDBACK_LIST]);
		glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER,
		                  0,
		                  buffers[BUFFER_LIST],
		                  0,
		                  particleCount * sizeof(GLint));
	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK,
	                        transformFeedbacks[TRANSFORM_FEEDBACK_PARTICLE_PING]);
		glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER,
		                  0,
		                  buffers[BUFFER_POS_DENSITIES_PONG],
		                  0,
		                  particleCount*sizeof(Vector4));
		glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER,
		                  1,
		                  buffers[BUFFER_VELOCITIES_PONG],
		                  0,
		                  particleCount*sizeof(Vector4));
	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK,
	                        transformFeedbacks[TRANSFORM_FEEDBACK_PARTICLE_PONG]);
		glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER,
		                  0,
		                  buffers[BUFFER_POS_DENSITIES_PING],
		                  0,
		                  particleCount*sizeof(Vector4));
		glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER,
		                  1,
		                  buffers[BUFFER_VELOCITIES_PING],
		                  0,
		                  particleCount*sizeof(Vector4));
	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK,
	                        transformFeedbacks[TRANSFORM_FEEDBACK_DENSITY_PING]);
		glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER,
		                  0,
		                  buffers[BUFFER_POS_DENSITIES_PONG],
		                  0,
		                  particleCount*sizeof(Vector4));
	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK,
	                        transformFeedbacks[TRANSFORM_FEEDBACK_DENSITY_PONG]);
		glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER,
		                  0,
		                  buffers[BUFFER_POS_DENSITIES_PING],
		                  0,
		                  particleCount*sizeof(Vector4));
	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK,0);
}


#ifdef _ANT_ENABLE

#endif

////////////////////////////////////////////////////////////////////////////////
// on init cb
void on_init()
{
	// constants / variables
	const GLfloat CUBE_VERTICES[] = { -0.5f, -0.5f,  0.5f, 1,   // 0 
	                                  -0.5f,  0.5f,  0.5f, 1,   // 1
	                                   0.5f,  0.5f,  0.5f, 1,   // 2
	                                   0.5f, -0.5f,  0.5f, 1,   // 3
	                                  -0.5f, -0.5f, -0.5f, 1,   // 4
	                                  -0.5f,  0.5f, -0.5f, 1,   // 5
	                                   0.5f,  0.5f, -0.5f, 1,   // 6
	                                   0.5f, -0.5f, -0.5f, 1 }; // 7
	const GLushort CUBE_EDGE_INDEXES[] = { 2,1,1,0,0,3,     // front
	                                       6,2,2,3,3,7,     // right
	                                       5,6,6,7,7,4,     // back
	                                       1,5,5,4,4,0 };   // left

	// alloc names
	buffers      = new GLuint[BUFFER_COUNT];
	vertexArrays = new GLuint[VERTEX_ARRAY_COUNT];
	textures     = new GLuint[TEXTURE_COUNT];
	programs     = new GLuint[PROGRAM_COUNT];
	transformFeedbacks = new GLuint[TRANSFORM_FEEDBACK_COUNT];

	// gen names
	glGenBuffers(BUFFER_COUNT, buffers);
	glGenVertexArrays(VERTEX_ARRAY_COUNT, vertexArrays);
	glGenTextures(TEXTURE_COUNT, textures);
	glGenTransformFeedbacks(TRANSFORM_FEEDBACK_COUNT, transformFeedbacks);
	for(GLuint i=0; i<PROGRAM_COUNT;++i)
		programs[i] = glCreateProgram();

	// configure buffer objects
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[BUFFER_CUBE_INDEXES]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER,
		             sizeof(CUBE_EDGE_INDEXES),
		             CUBE_EDGE_INDEXES,
		             GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ARRAY_BUFFER, buffers[BUFFER_CUBE_VERTICES]);
		glBufferData(GL_ARRAY_BUFFER,
		             sizeof(CUBE_VERTICES),
		             CUBE_VERTICES,
		             GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, buffers[BUFFER_POS_DENSITIES_PING]);
		glBufferData(GL_ARRAY_BUFFER,
		             sizeof(Vector4)*MAX_PARTICLE_COUNT,
		             NULL,
		             GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, buffers[BUFFER_POS_DENSITIES_PONG]);
		glBufferData(GL_ARRAY_BUFFER,
		             sizeof(Vector4)*MAX_PARTICLE_COUNT,
		             NULL,
		             GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, buffers[BUFFER_VELOCITIES_PING]);
		glBufferData(GL_ARRAY_BUFFER,
		             sizeof(Vector4)*MAX_PARTICLE_COUNT,
		             NULL,
		             GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, buffers[BUFFER_VELOCITIES_PONG]);
		glBufferData(GL_ARRAY_BUFFER,
		             sizeof(Vector4)*MAX_PARTICLE_COUNT,
		             NULL,
		             GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_TEXTURE_BUFFER, buffers[BUFFER_HEAD]);
		glBufferData(GL_TEXTURE_BUFFER,
		             sizeof(GLint)*BUCKET_1D_MAX,
		             NULL,
		             GL_STATIC_DRAW);
	glBindBuffer(GL_TEXTURE_BUFFER, buffers[BUFFER_LIST]);
		glBufferData(GL_TEXTURE_BUFFER,
		             sizeof(GLint)*MAX_PARTICLE_COUNT,
		             NULL,
		             GL_STATIC_DRAW);
	glBindBuffer(GL_TEXTURE_BUFFER, 0);

//	std::cout << "BUCKET_1D_MAX " << BUCKET_1D_MAX << std::endl;

	// textures
	glActiveTexture(GL_TEXTURE0 + TEXTURE_HEAD);
		glBindTexture(GL_TEXTURE_BUFFER, textures[TEXTURE_HEAD]);
		glTexBuffer(GL_TEXTURE_BUFFER, GL_R32I, buffers[BUFFER_HEAD]);

	glActiveTexture(GL_TEXTURE0 + TEXTURE_LIST);
		glBindTexture(GL_TEXTURE_BUFFER, textures[TEXTURE_LIST]);
		glTexBuffer(GL_TEXTURE_BUFFER, GL_R32I, buffers[BUFFER_LIST]);

	glActiveTexture(GL_TEXTURE0 + TEXTURE_POS_DENSITIES_PING);
		glBindTexture(GL_TEXTURE_BUFFER, textures[TEXTURE_POS_DENSITIES_PING]);
		glTexBuffer(GL_TEXTURE_BUFFER,
		            GL_RGBA32F,
		            buffers[BUFFER_POS_DENSITIES_PING]);

	glActiveTexture(GL_TEXTURE0 + TEXTURE_POS_DENSITIES_PONG);
		glBindTexture(GL_TEXTURE_BUFFER, textures[TEXTURE_POS_DENSITIES_PONG]);
		glTexBuffer(GL_TEXTURE_BUFFER,
		            GL_RGBA32F,
		            buffers[BUFFER_POS_DENSITIES_PONG]);

	glActiveTexture(GL_TEXTURE0 + TEXTURE_VELOCITIES_PING);
		glBindTexture(GL_TEXTURE_BUFFER, textures[TEXTURE_VELOCITIES_PING]);
		glTexBuffer(GL_TEXTURE_BUFFER,
		            GL_RGBA32F,
		            buffers[BUFFER_VELOCITIES_PING]);

	glActiveTexture(GL_TEXTURE0 + TEXTURE_VELOCITIES_PONG);
		glBindTexture(GL_TEXTURE_BUFFER, textures[TEXTURE_VELOCITIES_PONG]);
		glTexBuffer(GL_TEXTURE_BUFFER,
		            GL_RGBA32F,
		            buffers[BUFFER_VELOCITIES_PONG]);

	glBindImageTexture(TEXTURE_HEAD,
	                   textures[TEXTURE_HEAD],
	                   0,
	                   GL_FALSE,
	                   0,
	                   GL_READ_WRITE,
	                   GL_R32I);

	glBindImageTexture(TEXTURE_LIST,
	                   textures[TEXTURE_LIST],
	                   0,
	                   GL_FALSE,
	                   0,
	                   GL_READ_WRITE,
	                   GL_R32I);

	// configure vertex arrays
	glBindVertexArray(vertexArrays[VERTEX_ARRAY_BUCKET]);
		// empty !
	glBindVertexArray(vertexArrays[VERTEX_ARRAY_CUBE]);
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, buffers[BUFFER_CUBE_VERTICES]);
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, FW_BUFFER_OFFSET(0));
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[BUFFER_CUBE_INDEXES]);
	glBindVertexArray(vertexArrays[VERTEX_ARRAY_POS_DENSITY_PING]);
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, buffers[BUFFER_POS_DENSITIES_PING]);
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, FW_BUFFER_OFFSET(0));
	glBindVertexArray(vertexArrays[VERTEX_ARRAY_POS_DENSITY_PONG]);
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, buffers[BUFFER_POS_DENSITIES_PONG]);
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, FW_BUFFER_OFFSET(0));
	glBindVertexArray(vertexArrays[VERTEX_ARRAY_FLUID_RENDER_PING]);
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glBindBuffer(GL_ARRAY_BUFFER, buffers[BUFFER_POS_DENSITIES_PING]);
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, FW_BUFFER_OFFSET(0));
		glBindBuffer(GL_ARRAY_BUFFER, buffers[BUFFER_VELOCITIES_PING]);
		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, FW_BUFFER_OFFSET(0));
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[BUFFER_CUBE_INDEXES]);
	glBindVertexArray(vertexArrays[VERTEX_ARRAY_FLUID_RENDER_PONG]);
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glBindBuffer(GL_ARRAY_BUFFER, buffers[BUFFER_POS_DENSITIES_PONG]);
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, FW_BUFFER_OFFSET(0));
		glBindBuffer(GL_ARRAY_BUFFER, buffers[BUFFER_VELOCITIES_PONG]);
		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, FW_BUFFER_OFFSET(0));
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[BUFFER_CUBE_INDEXES]);
	glBindVertexArray(0);

	// configure programs
	fw::build_glsl_program(programs[PROGRAM_DENSITY],
	                       "sph_density.glsl",
	                       "",
	                       GL_FALSE);
	const GLchar* varyings1[] = {"oData"};
	glTransformFeedbackVaryings(programs[PROGRAM_DENSITY],
	                            1,
	                            varyings1,
	                            GL_INTERLEAVED_ATTRIBS);
	glLinkProgram(programs[PROGRAM_DENSITY]);

	fw::build_glsl_program(programs[PROGRAM_BUCKET],
	                       "sph_cell_init.glsl",
	                       "",
	                       GL_FALSE);
	glTransformFeedbackVaryings(programs[PROGRAM_BUCKET],
	                            1,
	                            varyings1,
	                            GL_INTERLEAVED_ATTRIBS);
	glLinkProgram(programs[PROGRAM_BUCKET]);

	fw::build_glsl_program(programs[PROGRAM_FORCE],
	                       "sph_force.glsl",
	                       "",
	                       GL_FALSE);
	const GLchar* varyings2[] = {"oData0", "oData1"};
	glTransformFeedbackVaryings(programs[PROGRAM_FORCE],
	                            1,
	                            varyings2,
	                            GL_SEPARATE_ATTRIBS);
	glLinkProgram(programs[PROGRAM_FORCE]);

	fw::build_glsl_program(programs[PROGRAM_GRID],
	                       "sph_grid.glsl",
	                       "",
	                       GL_TRUE);

	fw::build_glsl_program(programs[PROGRAM_FLUID_RENDER],
	                       "sph_render.glsl",
	                       "",
	                       GL_TRUE);

	fw::build_glsl_program(programs[PROGRAM_CUBE_RENDER],
	                       "cube.glsl",
	                       "",
	                       GL_TRUE);

	fw::build_glsl_program(programs[PROGRAM_BUCKET_RENDER],
	                       "sph_bucket_render.glsl",
	                       "",
	                       GL_TRUE);

	// set constants
	set_runtime_constant_uniforms();
	set_sph_constants();
	set_delta();
	set_gravity_vector();

	// set TF
	set_transform_feedbacks();

	// set particles
	init_sph_particles();


	// set global params
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glClearColor(0.13,0.13,0.15,1.0);

#ifdef _ANT_ENABLE
	// start ant
	TwInit(TW_OPENGL, NULL);
	// send the ''glutGetModifers'' function pointer to AntTweakBar
	TwGLUTModifiersFunc(glutGetModifiers);

#endif // _ANT_ENABLE
	fw::check_gl_error();
}


////////////////////////////////////////////////////////////////////////////////
// on clean cb
void on_clean()
{
	// delete objects
	glDeleteBuffers(BUFFER_COUNT, buffers);
	glDeleteVertexArrays(VERTEX_ARRAY_COUNT, vertexArrays);
	glDeleteTextures(TEXTURE_COUNT, textures);
	glDeleteTransformFeedbacks(TRANSFORM_FEEDBACK_COUNT, transformFeedbacks);
	for(GLuint i=0; i<PROGRAM_COUNT;++i)
		glDeleteProgram(programs[i]);

	// release memory
	delete[] buffers;
	delete[] vertexArrays;
	delete[] textures;
	delete[] programs;
	delete[] transformFeedbacks;

#ifdef _ANT_ENABLE
	TwTerminate();
#endif // _ANT_ENABLE

	fw::check_gl_error();
}


////////////////////////////////////////////////////////////////////////////////
// on update cb
void on_update()
{
	// Global variable
	static fw::Timer deltaTimer;
	GLint windowWidth  = glutGet(GLUT_WINDOW_WIDTH);
	GLint windowHeight = glutGet(GLUT_WINDOW_HEIGHT);
	float aspect = float(windowWidth)/float(windowHeight);
#ifdef _ANT_ENABLE
	fw::Timer spfTimer;
#endif // _ANT_ENABLE

	// stop the timer during update
	deltaTimer.Stop();

#ifdef _ANT_ENABLE
	// begin timing
	spfTimer.Start();
#endif // _ANT_ENABLE

	// update transformations
	cameraProjection.FitHeightToAspect(aspect);
	Matrix4x4 mvp = cameraProjection.ExtractTransformMatrix()
	              * invCameraWorld.ExtractTransformMatrix();

	// update transforms
	glProgramUniformMatrix4fv(programs[PROGRAM_CUBE_RENDER],
	                          glGetUniformLocation(programs[PROGRAM_CUBE_RENDER],
	                                               "uModelViewProjection"),
	                          1,
	                          0,
	                          reinterpret_cast<const float * >(&mvp));
	glProgramUniformMatrix4fv(programs[PROGRAM_FLUID_RENDER],
	                          glGetUniformLocation(programs[PROGRAM_FLUID_RENDER],
	                                               "uModelViewProjection"),
	                          1,
	                          0,
	                          reinterpret_cast<const float * >(&mvp));
	glProgramUniformMatrix4fv(programs[PROGRAM_BUCKET_RENDER],
	                          glGetUniformLocation(programs[PROGRAM_BUCKET_RENDER],
	                                               "uModelViewProjection"),
	                          1,
	                          0,
	                          reinterpret_cast<const float * >(&mvp));

	// set viewport
	glViewport(0,0,windowWidth, windowHeight);

	// clear back buffer
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// render cube
	glUseProgram(programs[PROGRAM_CUBE_RENDER]);
	glBindVertexArray(vertexArrays[VERTEX_ARRAY_CUBE]);
	glDrawElements(GL_LINES,
	               24,
	               GL_UNSIGNED_SHORT,
	               FW_BUFFER_OFFSET(0));

	// render cells
	if(renderBucket)
	{
		glUseProgram(programs[PROGRAM_BUCKET_RENDER]);
		glDrawElementsInstanced(GL_LINES,
		                        24,
		                        GL_UNSIGNED_SHORT,
		                        FW_BUFFER_OFFSET(0),
		                        cellCount);
	}

	// build grid
//	build_grid();
	init_sph_density();

	// update attributes
	glEnable(GL_RASTERIZER_DISCARD);

	glUseProgram(programs[PROGRAM_FORCE]);
	glUniform1i(glGetUniformLocation(programs[PROGRAM_FORCE],
	                                 "sData0"),
	            TEXTURE_POS_DENSITIES_PING + sphPingPong);
	glUniform1i(glGetUniformLocation(programs[PROGRAM_FORCE],
	                                 "sData1"),
	            TEXTURE_VELOCITIES_PING + sphPingPong);

	glBindTransformFeedback( GL_TRANSFORM_FEEDBACK,
		transformFeedbacks[TRANSFORM_FEEDBACK_PARTICLE_PING + sphPingPong]
		);

	glBindVertexArray(vertexArrays[VERTEX_ARRAY_FLUID_RENDER_PING+sphPingPong]);
	glBeginTransformFeedback(GL_POINTS);
		glDrawArrays(GL_POINTS, 0, particleCount);
	glEndTransformFeedback();

	// ping pong
	sphPingPong = 1 - sphPingPong;

	// restore state
	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
	glDisable(GL_RASTERIZER_DISCARD);

	// render particles
	glUseProgram(programs[PROGRAM_FLUID_RENDER]);
	glBindVertexArray(vertexArrays[VERTEX_ARRAY_FLUID_RENDER_PING + sphPingPong]);
	glDrawArrays(GL_POINTS, 0, particleCount);

	// back to default vertex array
	glBindVertexArray(0);

#ifdef _ANT_ENABLE
	// stop timing
	spfTimer.Stop();
	secondsPerFrame = spfTimer.Ticks();

	// draw gui
	TwDraw();
#endif // _ANT_ENABLE

	// check gl errors
	fw::check_gl_error();

	// restart timer
	deltaTimer.Start();

	glutSwapBuffers();
	glutPostRedisplay();
}


////////////////////////////////////////////////////////////////////////////////
// on resize cb
void on_resize(GLint w, GLint h)
{
#ifdef _ANT_ENABLE
	TwWindowSize(w, h);
#endif
}


////////////////////////////////////////////////////////////////////////////////
// on key down cb
void on_key_down(GLubyte key, GLint x, GLint y)
{
#ifdef _ANT_ENABLE
	if(1==TwEventKeyboardGLUT(key, x, y))
		return;
#endif
	if (key==27) // escape
		glutLeaveMainLoop();
	if(key=='f')
		glutFullScreenToggle();
	if(key=='p')
		fw::save_gl_front_buffer(0,
		                         0,
		                         glutGet(GLUT_WINDOW_WIDTH),
		                         glutGet(GLUT_WINDOW_HEIGHT));
	if(key=='r')
	{
		++smoothingLength;
		set_sph_constants();
	}
	if(key=='b')
		renderBucket = !renderBucket;
}


////////////////////////////////////////////////////////////////////////////////
// on mouse button cb
void on_mouse_button(GLint button, GLint state, GLint x, GLint y)
{
#ifdef _ANT_ENABLE
	if(1 == TwEventMouseButtonGLUT(button, state, x, y))
		return;
#endif // _ANT_ENABLE

	if(state==GLUT_DOWN)
	{
		mouseLeft  |= button == GLUT_LEFT_BUTTON;
		mouseRight |= button == GLUT_RIGHT_BUTTON;
	}
	else
	{
		mouseLeft  &= button == GLUT_LEFT_BUTTON  ? false : mouseLeft;
		mouseRight &= button == GLUT_RIGHT_BUTTON ? false : mouseRight;
	}
	if(button == 3)
		invCameraWorld.TranslateWorld(Vector3(0,0,15.0f));
	if(button == 4)
		invCameraWorld.TranslateWorld(Vector3(0,0,-15.0f));
}


////////////////////////////////////////////////////////////////////////////////
// on mouse motion cb
void on_mouse_motion(GLint x, GLint y)
{
#ifdef _ANT_ENABLE
	if(1 == TwEventMouseMotionGLUT(x,y))
		return;
#endif // _ANT_ENABLE

	static GLint sMousePreviousX = 0;
	static GLint sMousePreviousY = 0;
	const GLint MOUSE_XREL = x-sMousePreviousX;
	const GLint MOUSE_YREL = y-sMousePreviousY;
	sMousePreviousX = x;
	sMousePreviousY = y;

	if(mouseLeft)
	{
		invCameraWorld.RotateAboutWorldX(-0.01f*MOUSE_YREL);
		invCameraWorld.RotateAboutLocalY( 0.01f*MOUSE_XREL);
	}
	if(mouseRight)
	{
		invCameraWorld.TranslateWorld(Vector3( 0.61f*MOUSE_XREL,
		                                      -0.61f*MOUSE_YREL,
		                                       0));
	}
}


////////////////////////////////////////////////////////////////////////////////
// on mouse wheel cb
void on_mouse_wheel(GLint wheel, GLint direction, GLint x, GLint y)
{
#ifdef _ANT_ENABLE
	if(1 == TwMouseWheel(wheel))
		return;
#endif // _ANT_ENABLE
	invCameraWorld.TranslateWorld(Vector3(0,0,-direction));
}


////////////////////////////////////////////////////////////////////////////////
// Main
//
////////////////////////////////////////////////////////////////////////////////
int main(int argc, char** argv)
{
	const GLuint CONTEXT_MAJOR = 4;
	const GLuint CONTEXT_MINOR = 1;

	// init glut
	glutInit(&argc, argv);
	glutInitContextVersion(CONTEXT_MAJOR ,CONTEXT_MINOR);
#ifdef _ANT_ENABLE
	glutInitContextFlags(GLUT_DEBUG);
	glutInitContextProfile(GLUT_COMPATIBILITY_PROFILE);
#else
	glutInitContextFlags(GLUT_DEBUG);
	glutInitContextProfile(GLUT_CORE_PROFILE);
#endif

	// build window
	glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowSize(1024, 768);
	glutInitWindowPosition(0, 0);
	glutCreateWindow("SPH solver");

	// init glew

	glewExperimental = GL_TRUE; // segfault on GenVertexArrays on Nvidia otherwise
	GLenum err = glewInit();
	if(GLEW_OK != err)
	{
		std::stringstream ss;
		ss << err;
		std::cerr << "glewInit() gave error " << ss.str() << std::endl;
		return 1;
	}

	// glewInit generates an INVALID_ENUM error for some reason...
	glGetError();

	fw::check_gl_error();
	// set callbacks
	glutCloseFunc(&on_clean);
	glutReshapeFunc(&on_resize);
	glutDisplayFunc(&on_update);
	glutKeyboardFunc(&on_key_down);
	glutMouseFunc(&on_mouse_button);
	glutPassiveMotionFunc(&on_mouse_motion);
	glutMotionFunc(&on_mouse_motion);
	glutMouseWheelFunc(&on_mouse_wheel);

	// run
	try
	{
		// run demo
		on_init();
		glutMainLoop();
	}
	catch(std::exception& e)
	{
		std::cerr << "Fatal exception: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}

