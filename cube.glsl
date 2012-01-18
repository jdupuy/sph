#version 420 core

uniform vec4 uCubeSize;
uniform mat4 uModelViewProjection;

#ifdef _VERTEX_

layout(location=0) in  vec4 iPosition;

void main()
{
	gl_Position = uModelViewProjection * (iPosition * uCubeSize);
}

#endif // _VERTEX_

#ifdef _FRAGMENT_

layout(location=0) out vec4 oColor;

void main()
{
	oColor = vec4(1.0);
}

#endif // _FRAGMENT_


