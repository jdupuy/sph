#version 420 core

// images
layout(r32i) readonly uniform iimageBuffer imgHead;
layout(r32i) readonly uniform iimageBuffer imgList;

// samplers
uniform samplerBuffer sParticlePos;

// uniforms
uniform vec3  uBucket1dCoeffs;    // for conversion from bucket 3d to bucket 1d
uniform float uBucketCellSize;    // dimensions of the bucket
uniform vec3  uBucketBoundsMin;   // constant
uniform float uSmoothingLengthSquared;
uniform float uDensityConstants;

#ifdef _VERTEX_

layout(location=0) in  vec4 iData;  // position + reserved
layout(location=0) out vec4 oData;  // position + density

// evaluate density
float eval_density(float h2, vec3 ri, vec3 rj)
{
	vec3 rij    = ri - rj;
	float dist2 = h2 - dot(rij,rij);
	return (step(0.0, dist2) * pow(dist2, 3.0));
}


void main()
{
	// positions
	oData = vec4(iData.xyz,0.0);

	// 3d bucket texture (in [0,D]x[0,W]x[0,H])
	vec3 relPos    = iData.xyz - uBucketBoundsMin;
	vec3 bucket3d = floor(relPos / uBucketCellSize);

	// 1d bucket positions of cells
	int buckets1d[27];
	for(int i=-1; i<2; ++i)
	for(int j=-1; j<2; ++j)
	for(int k=-1; k<2; ++k)
		buckets1d[i+1+3*(j+1)+9*(k+1)] = int(dot(bucket3d + vec3(i,j,k),
		                                         uBucket1dCoeffs));

	// loop through neighbour particles
	int i = 0;         // iterator
	int offset = 0;    // texture offset
	vec3 neighbourPos; // neighbour position
	while(i<27)
	{
		// get offset
		offset = imageLoad(imgHead, buckets1d[i]).r;
		while(offset != -1)
		{
			// get stored particle position
			neighbourPos = texelFetch(sParticlePos, offset).rgb;

			// add contribution
			if(offset != gl_VertexID)
				oData.w += eval_density(uSmoothingLengthSquared,
				                        iData.xyz,
				                        neighbourPos);

			// next offset
			offset = imageLoad(imgList, offset).r;
		}
		++i;
	}

	// mutliply sum by constants
	oData.w *= uDensityConstants;
}

#endif // _VERTEX_

