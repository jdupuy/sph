////////////////////////////////////////////////////////////////////////////////
// \author   Jonathan Dupuy
//
////////////////////////////////////////////////////////////////////////////////

// enable gui
#define _ANT_ENABLE

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
const GLuint MAX_PARTICLE_COUNT = 512u*1024u;
const Vector3 SIMULATION_DOMAIN = Vector3(1,1,1);

enum // OpenGLNames
{
	// buffers
	BUFFER_POS_DENSITIES = 0,
	BUFFER_VELOCITIES,
	BUFFER_COUNT,

	// vertex arrays
	VERTEX_ARRAY_PARTICLES = 0,
	VERTEX_ARRAY_COUNT,

	// textures
	TEXTURE_HEAD = 0,
	TEXTURE_LIST,
	TEXTURE_POS_DENSITIES,
	TEXTURE_VELOCITIES,
	TEXTURE_COUNT,

	// programs
	PROGRAM_SPH_DENSITY_INIT = 0,
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

// SPH variables
float smoothingLength = 1.0f;
float particleMass    = 1.0f;
//float poly6Csts       =  315.0f/(64.0f*PI*pow(h,9.0f));
//float gradPoly6Csts   = -945.0f/(32.0f*PI*pow(h,9.0f));
//float 

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
	float h6        = pow(smoothingLength,6.0f);
	float h9        = pow(smoothingLength,9.0f);
	float poly6     =  315.0f/(64.0f*PI*h9);
	float gradPoly6 = -945.0f/(32.0f*PI*h9);
	float gradSpiky = -45.0f/(PI*h6);
	float grad2Viscosity = -gradSpiky; /* = 45.0f/(PI*h6); */

	// set uniforms
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

	// gen names
	glGenBuffers(BUFFER_COUNT, buffers);
	glGenVertexArrays(VERTEX_ARRAY_COUNT, vertexArrays);
	glGenTextures(TEXTURE_COUNT, textures);
	for(GLuint i=0; i<PROGRAM_COUNT;++i)
		programs[i] = glCreateProgram();

	// textures
	glActiveTexture(GL_TEXTURE0 + TEXTURE_HEAD);
	glActiveTexture(GL_TEXTURE0 + TEXTURE_LIST);

	// configure buffer objects
	glBindBuffer(GL_ARRAY_BUFFER, buffers[BUFFER_POS_DENSITIES]);
		glBufferData(GL_ARRAY_BUFFER,
		             sizeof(Vector4)*MAX_PARTICLE_COUNT,
		             NULL,
		             GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, buffers[BUFFER_VELOCITIES]);
		glBufferData(GL_ARRAY_BUFFER,
		             sizeof(Vector4)*MAX_PARTICLE_COUNT,
		             NULL,
		             GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// configure vertex arrays


	// configure programs
	fw::build_glsl_program(programs[PROGRAM_SPH_DENSITY_INIT],
	                       "sph_density_init.glsl",
	                       "",
	                       GL_TRUE);

	fw::build_glsl_program(programs[PROGRAM_SPH_GRID],
	                       "sph_grid.glsl",
	                       "",
	                       GL_TRUE);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glClearColor(0.13,0.13,0.15,1.0);
	glPatchParameteri(GL_PATCH_VERTICES, 3);

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
	for(GLuint i=0; i<PROGRAM_COUNT;++i)
		glDeleteProgram(programs[i]);

	// release memory
	delete[] buffers;
	delete[] vertexArrays;
	delete[] textures;
	delete[] programs;

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
	glutInitContextFlags(GLUT_DEBUG | GLUT_FORWARD_COMPATIBLE);
	glutInitContextProfile(GLUT_CORE_PROFILE);
#endif

	// build window
	glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowSize(800, 600);
	glutInitWindowPosition(0, 0);
	glutCreateWindow("Sph - OpenGL4.2");

	// init glew
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

