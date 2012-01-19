#version 420 core

uniform vec4  uCellSize;     // size of a cell
uniform ivec2 uBucket3dSize; // size of the 3d bucket
uniform vec4  uBucketBoundsMin;
uniform mat4  uModelViewProjection;

#ifdef _VERTEX_

layout(location=0) in vec4 iPosition;

void main()
{
	// compute 3d cell pos
	vec4 bucket = vec4(gl_InstanceID / uBucket3dSize.x % uBucket3dSize.x,
	                   gl_InstanceID % uBucket3dSize.y,
	                   gl_InstanceID / (uBucket3dSize.x*uBucket3dSize.y),
	                   0.0);

	// world position
	vec4 pos = iPosition*uCellSize // w = 1
	         + bucket*uCellSize    // w = 0
	         + uBucketBoundsMin;   // w = 0

	// clip pos
	gl_Position = uModelViewProjection * pos;
}

#endif // _VERTEX_


#ifdef _FRAGMENT_

layout(location=0) out vec4 oColor;

void main()
{
	oColor = vec4(0.0);
}

#endif // _FRAGMENT_

