#version 420 core

// images
layout(r32i) coherent uniform iimageBuffer imgCells;
layout(r32i) coherent uniform iimageBuffer imgParticles;

// uniforms
uniform ivec3 uBucket1DCoeffs;
uniform vec3  uBucketTexelSize;
uniform vec3  uBucketBoundsMin;

#ifdef _VERTEX_

layout(location = 0) in vec4 iData;

void main()
{
	// 3d bucket texture (in [0,D]x[0,W]x[0,H])
	vec3 relPos    = iData.xyz - uBucketBoundsMin;
	ivec3 bucket3D = ivec3(relPos / uBucketTexelSize);

	// 1d bucket pos (no dot products for ivec...)
	int bucket1D = bucket3D.x * uBucket1DCoeffs.x
	             + bucket3D.y * uBucket1DCoeffs.y
	             + bucket3D.z * uBucket1DCoeffs.z;

	// check position in grid
	int index = imageAtomicCompSwap(imgCells,
	                                bucket1D,
	                                -1,
	                                gl_VertexID);
	while(index != gl_VertexID)
		index = imageAtomicCompSwap(imgParticles,
		                            index,
		                            -1,
		                            gl_VertexID);
}

#endif // _VERTEX_

