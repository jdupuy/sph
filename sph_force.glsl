#version 420 core

// images
layout(r32i) readonly uniform iimageBuffer imgHead;
layout(r32i) readonly uniform iimageBuffer imgList;

// samplers
uniform samplerBuffer sParticlePos;
uniform samplerBuffer sParticleVel;

// uniforms
uniform vec3  uBucket1dCoeffs;     // for conversion from bucket 3d to bucket 1d
uniform float uBucketCellSize;     // dimensions of the bucket
uniform vec3  uBucketBoundsMin;    // constant
uniform vec3  uSmoothingLength;    // h
uniform float uPressureConstants;  // kernel constants
uniform float uViscosityConstants; // kernel constants
uniform float uParticleMass;       // mass of the particles

#ifdef _VERTEX_

layout(location=0) in vec4 iData0; // pos + density
layout(location=1) in vec4 iData1; // velocity

layout(location=0) flat out vec4 oForce;

#define iPosition iData0.xyz
#define iDensity  iData0.w
#define iVelocity iData1.xyz
#define oPressure  oData.xyz
#define oViscosity oData.xyz

// compute the pressure for a given density
float pressure(float rho)
{
	return uK * (rho - uRho0);
}


// compute spiky kernel gradient variables
vec3 eval_spiky(float h, vec3 rij, float r)
{
	return pow(max(h-r,0.0),2.0) * rij / r;
}


// compute viscosity kernel second order gradient variables
float eval_viscosity(float h, float r)
{
	return max(h-r,0.0);
}


void main()
{
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
	int i = 0;                // iterator
	int offset = 0;           // texture offset
	vec3 neighbourPosDensity; // neighbour position
	vec3 neighbourVel;        // neighbour velocity
	vec3 rij, vij;
	while(i<27)
	{
		// get offset
		offset = imageLoad(imgHead, buckets1d[i]).r;
		while(offset != -1)
		{
			// get stored particle position
			neighbourPosDensity = texelFetch(sParticlePosDensity, offset);
			neighbourVel        = texelFetch(sParticleVel, offset).rgb;

			// compute rij and vij
			rij = iPosition - neighbourPosDensity.xyz;
			vij = iVelocity - neighbourVel.xyz;

			// add contribution
			if(offset != gl_VertexID) // step(0.0, abs(offset - gl_VertexID));
			{
				// compute vector magnitude
				float r = length(rij);
				float rhoij = 1.0/(neighbourPosDensity.a * iDensity);

				// evaluate pressure
				oPressure+= uPressureConstants
				          * eval_spiky(uSmoothingLength,rij,r)
				          * (pressure(neighbourPosDensity.a)-pressure(iDensity))
				          * (rhoij);

				// evaluate viscosity
				oViscosity+= uViscosityConstants
				           * eval_viscosity(uSmoothingLength,r)
				           * (neighbourVel - iVelocity)
				           * (rhoij);
			}

			// next offset
			offset = imageLoad(imgList, offset).r;
		}
		++i;
	}
}

#endif // _VERTEX_


