#version 420 core

// images
layout(r32i) coherent uniform iimageBuffer imgHead;
layout(r32i) coherent uniform iimageBuffer imgList;

// uniforms
uniform vec3 uBucket1dCoeffs;
uniform vec3 uBucket3dSize;
uniform vec3 uBucketBoundsMin;

#ifdef _VERTEX_

layout(location = 0) in vec4 iData;

void main()
{
	// 3d bucket texture (in [0,D]x[0,W]x[0,H])
	vec3 relPos    = iData.xyz - uBucketBoundsMin;
	ivec3 bucket3d = ivec3(relPos / uBucket3dSize);

	// 1d bucket pos (no dot products for ivec...)
	int bucket1d = int(dot(bucket3d, uBucket1dCoeffs));

	// check position in grid
	int index = imageAtomicCompSwap(imgHead,
	                                bucket1d,
	                                -1,
	                                gl_VertexID);
	while(index != -1)
		index = imageAtomicCompSwap(imgList,
		                            index,
		                            -1,
		                            gl_VertexID);
}

#endif // _VERTEX_

