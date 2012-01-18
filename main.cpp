////////////////////////////////////////////////////////////////////////////////
// \author   Jonathan Dupuy
// \brief    An SPH solver, running on the GPU via OpenGL4.2 and GLSL420.
//
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
const float  PI = 3.14159265;
const GLuint MAX_PARTICLE_COUNT     = 512u*1024u;
const Vector3 SIMULATION_DOMAIN     = Vector3(0.5f,0.5f,0.5f);
const float MIN_SMOOTHING_LENGTH    = 0.005f; // meters
const Vector3 BUCKET_MAX_RESOLUTION = (SIMULATION_DOMAIN/MIN_SMOOTHING_LENGTH).Ceil()
                                    + Vector3(2,2,2); // border cells

enum // OpenGLNames
{
	// buffers
	BUFFER_POS_DENSITIES_PING = 0,
	BUFFER_POS_DENSITIES_PONG,
	BUFFER_VELOCITIES_PING,
	BUFFER_VELOCITIES_PONG,
	BUFFER_HEAD,
	BUFFER_LIST,
	BUFFER_COUNT,

	// vertex arrays
	VERTEX_ARRAY_CELL_INIT = 0,
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
	TRANSFORM_FEEDBACK_PARTICLE_PING,
	TRANSFORM_FEEDBACK_PARTICLE_PONG,
	TRANSFORM_FEEDBACK_COUNT,

	// programs
	PROGRAM_SPH_DENSITY_INIT = 0,
	PROGRAM_SPH_CELL_INIT,
	PROGRAM_SPH_GRID,
	PROGRAM_PARTICLE_RENDER,
	PROGRAM_SIM_BOUNDS_RENDER,
	PROGRAM_COUNT
};

// OpenGL objects
GLuint *buffers      = NULL;
GLuint *vertexArrays = NULL;
GLuint *textures     = NULL;
GLuint *programs     = NULL;
GLuint *transformFeedbacks = NULL;

// SPH variables
GLfloat smoothingLength = 1.0f;  // grams
GLfloat particleMass    = 1.0f;  // grams
GLuint particleCount    = 1024u; // number of particles
GLuint cellCount        = 2048u; // number of cells
Vector3 gravityVector   = Vector3(0,-1,0); // gravity direction
GLint sphPingpong       = 0;

// Tools
Affine invCameraWorld       = Affine::Translation(Vector3(0,0,-5));
Projection cameraProjection = Projection::Perspective(PI*0.25f,
                                                      1.0f,
                                                      0.125f,
                                                      1024.0f);

bool mouseLeft  = false;
bool mouseRight = false;

#ifdef _ANT_ENABLE
double framesPerSecond = 0.0; // fps
#endif

////////////////////////////////////////////////////////////////////////////////
// Functions
//
////////////////////////////////////////////////////////////////////////////////

// precompute sph force constant components
void set_sph_constants()
{
//	GLfloat h6        = pow(smoothingLength,6.0f);
//	GLfloat h9        = pow(smoothingLength,9.0f);
//	GLfloat poly6     =  315.0f/(64.0f*PI*h9);
//	GLfloat gradPoly6 = -945.0f/(32.0f*PI*h9);
//	GLfloat gradSpiky = -45.0f/(PI*h6);
//	GLfloat grad2Viscosity = -gradSpiky; /* = 45.0f/(PI*h6); */

	// set uniforms
}

// initialize the particles
void init_sph_particles()
{
//	std::vector<Vector4> positions;
//	std::vector<Vector4> velocities;

//	positions.reserve(particleCount);
//	velocities.reserve(particleCount);

//	GLint i=0;
//	while(i<particleCount)
//	{
//		
//		++i;
//	}

}

// initialize cells
void init_sph_cells()
{
	glEnable(GL_RASTERIZER_DISCARD);
	glUseProgram(programs[PROGRAM_SPH_CELL_INIT]);
	glBindVertexArray(vertexArrays[VERTEX_ARRAY_CELL_INIT]);

	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK,
	                        transformFeedbacks[TRANSFORM_FEEDBACK_HEAD]);
	glBeginTransformFeedback(GL_POINTS);
		glDrawArrays(GL_POINTS, 0, cellCount/4);
	glEndTransformFeedback();

	glFinish(); // apparently, this is mandatory on AMD11.12...

	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK,
	                        transformFeedbacks[TRANSFORM_FEEDBACK_LIST]);
	glBeginTransformFeedback(GL_POINTS);
		glDrawArrays(GL_POINTS, 0, particleCount/4);
	glEndTransformFeedback();

	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
	glDisable(GL_RASTERIZER_DISCARD);
}

// compute densities
void init_sph_density()
{
	
}


#ifdef _ANT_ENABLE

#endif

////////////////////////////////////////////////////////////////////////////////
// on init cb
void on_init()
{
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
		             sizeof(GLint)*MAX_PARTICLE_COUNT,
		             NULL,
		             GL_STATIC_DRAW);
	glBindBuffer(GL_TEXTURE_BUFFER, buffers[BUFFER_LIST]);
		glBufferData(GL_TEXTURE_BUFFER,
		             sizeof(GLint)*MAX_PARTICLE_COUNT,
		             NULL,
		             GL_STATIC_DRAW);
	glBindBuffer(GL_TEXTURE_BUFFER, 0);

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
	glBindVertexArray(vertexArrays[VERTEX_ARRAY_CELL_INIT]);
	glBindVertexArray(0);

	// transform feedbacks
	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK,
	                        transformFeedbacks[TRANSFORM_FEEDBACK_HEAD]);
		glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER,
		                  0,
		                  buffers[BUFFER_HEAD],
		                  0,
		                  fw::next_power_of_two(cellCount * sizeof(GLint)));
	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK,
	                        transformFeedbacks[TRANSFORM_FEEDBACK_LIST]);
		glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER,
		                  0,
		                  buffers[BUFFER_LIST],
		                  0,
		                  fw::next_power_of_two(particleCount * sizeof(GLint)));
	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK,
	                        transformFeedbacks[TRANSFORM_FEEDBACK_PARTICLE_PING]);
		glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER,
		                  0,
		                  buffers[BUFFER_POS_DENSITIES_PONG],
		                  0,
		                  fw::next_power_of_two(particleCount*sizeof(Vector4)));
		glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER,
		                  1,
		                  buffers[BUFFER_VELOCITIES_PONG],
		                  0,
		                  fw::next_power_of_two(particleCount*sizeof(Vector4)));
	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK,
	                        transformFeedbacks[TRANSFORM_FEEDBACK_PARTICLE_PONG]);
		glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER,
		                  0,
		                  buffers[BUFFER_POS_DENSITIES_PING],
		                  0,
		                  fw::next_power_of_two(particleCount*sizeof(Vector4)));
		glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER,
		                  1,
		                  buffers[BUFFER_VELOCITIES_PING],
		                  0,
		                  fw::next_power_of_two(particleCount*sizeof(Vector4)));
	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK,0);

	// configure programs
	fw::build_glsl_program(programs[PROGRAM_SPH_DENSITY_INIT],
	                       "sph_density_init.glsl",
	                       "",
	                       GL_TRUE);

	fw::build_glsl_program(programs[PROGRAM_SPH_CELL_INIT],
	                       "sph_cell_init.glsl",
	                       "",
	                       GL_FALSE);
	const GLchar* varyings1[] = {"oData"};
	glTransformFeedbackVaryings(programs[PROGRAM_SPH_CELL_INIT],
	                            1,
	                            varyings1,
	                            GL_INTERLEAVED_ATTRIBS);
	glLinkProgram(programs[PROGRAM_SPH_CELL_INIT]);

	fw::build_glsl_program(programs[PROGRAM_SPH_GRID],
	                       "sph_grid.glsl",
	                       "",
	                       GL_TRUE);

	// Set uniforms
	glProgramUniform1i(programs[PROGRAM_SPH_CELL_INIT],
	                   glGetUniformLocation(programs[PROGRAM_SPH_CELL_INIT],
	                                        "imgHead"),
	                   TEXTURE_HEAD);
	glProgramUniform1i(programs[PROGRAM_SPH_CELL_INIT],
	                   glGetUniformLocation(programs[PROGRAM_SPH_CELL_INIT],
	                                        "imgList"),
	                   TEXTURE_LIST);

	// test
	init_sph_cells();
//	glBindBuffer(GL_TEXTURE_BUFFER, buffers[BUFFER_HEAD]);
//	GLint *ptr = (GLint *) glMapBufferRange(GL_TEXTURE_BUFFER,
//	                                        0,
//	                                        sizeof(GLint)*cellCount,
//	                                        GL_MAP_READ_BIT);
//	for(GLuint i=0; i<cellCount; ++i)
//		std::cout << ptr[i] << ' ';
//	std::cout << std::endl;
//	glUnmapBuffer(GL_TEXTURE_BUFFER);
//	glBindBuffer(GL_TEXTURE_BUFFER, buffers[BUFFER_LIST]);
//	ptr        = (GLint *) glMapBufferRange(GL_TEXTURE_BUFFER,
//	                                        0,
//	                                        sizeof(GLint)*particleCount,
//	                                        GL_MAP_READ_BIT);
//	for(GLuint i=0; i<particleCount; ++i)
//		std::cout << ptr[i] << ' ';
//	std::cout << std::endl;
//	glUnmapBuffer(GL_TEXTURE_BUFFER);
//	glBindBuffer(GL_TEXTURE_BUFFER, 0);

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

	// stop the timer during update
	deltaTimer.Stop();
#ifdef _ANT_ENABLE
	// Compute fps
	framesPerSecond = 1.0/deltaTimer.Ticks();
#endif // _ANT_ENABLE

	// update transformations
	cameraProjection.FitHeightToAspect(aspect);
	Matrix4x4 mvp = cameraProjection.ExtractTransformMatrix()
	              * invCameraWorld.ExtractTransformMatrix();

	// set viewport
	glViewport(0,0,windowWidth, windowHeight);

	// clear back buffer
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


#ifdef _ANT_ENABLE
	// back to default vertex array
	glBindVertexArray(0);
	TwDraw();
#endif // _ANT_ENABLE

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
		invCameraWorld.TranslateWorld(Vector3(0,0,0.15f));
	if(button == 4)
		invCameraWorld.TranslateWorld(Vector3(0,0,-0.15f));
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
		invCameraWorld.TranslateWorld(Vector3( 0.01f*MOUSE_XREL,
		                                      -0.01f*MOUSE_YREL,
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
	glutInitWindowSize(800, 600);
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

