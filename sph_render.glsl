#version 420 core

uniform mat4 uModelViewProjection;

#ifdef _VERTEX_

layout(location=0) in vec4 iData0;
layout(location=1) in vec4 iData1;

layout(location=0) flat out vec4 oData;

void main()
{
	// get densities and velocities
	oData = vec4(iData1.xyz, iData0.w);
//	oData = iData1;

	// compute position
	gl_Position = uModelViewProjection * vec4(iData0.xyz, 1.0);

}

#endif // _VERTEX_

#ifdef _FRAGMENT_

layout(location=0) flat in vec4 iData;

layout(location=0) out vec4 oColor;

#define iVelocity iData.xyz
#define iDensity iData.w

void main()
{
#if defined _VELOCITY
	oColor = abs(normalize(iVelocity));
#elif defined _DENSITY
	oColor = vec4(iDensity);
#else
	float coeff = smoothstep(0.01, 0.4, iDensity);
//	float coeff = smoothstep(1.9, 4.9, iDensity);
	oColor.rgb = coeff*vec3(0.0,0.0,1.0) + vec3(1.0-coeff);
#endif
}

#endif // _FRAGMENT_


