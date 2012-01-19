#version 420 core

uniform float uBucketCellSize; // size of a cell
uniform ivec2 uBucket3dSize;   // size of the 3d bucket
uniform vec3  uBucketBoundsMin;
uniform mat4  uModelViewProjection;

#ifdef _VERTEX_

layout(location=0) in vec4 iPosition;

void main()
{
	// compute 3d cell pos
	vec3 bucket = vec3(gl_InstanceID % uBucket3dSize.x,
	                   gl_InstanceID / uBucket3dSize.y % uBucket3dSize.y,
	                   gl_InstanceID / (uBucket3dSize.x*uBucket3dSize.y));

	// world position
	vec3 world = uBucketCellSize*(iPosition.xyz+bucket)
	           + uBucketBoundsMin;

	// clip pos
	gl_Position = uModelViewProjection * vec4(world,1.0);
}

#endif // _VERTEX_


#ifdef _FRAGMENT_

layout(location=0) out vec4 oColor;

void main()
{
	oColor = vec4(0.0,0.0,0.0,1.0);
}

#endif // _FRAGMENT_

