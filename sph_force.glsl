#version 420 core

// images
layout(r32i) readonly uniform iimageBuffer imgHead;
layout(r32i) readonly uniform iimageBuffer imgList;

// samplers
uniform samplerBuffer sData0; // pos + density
uniform samplerBuffer sData1; // velocity

// uniforms
uniform vec3  uBucket1dCoeffs;     // for conversion from bucket 3d to bucket 1d
uniform vec3  uBucketBoundsMin;    // constant
uniform float uBucketCellSize;     // dimensions of the bucket

uniform float uSmoothingLength;        // h
uniform float uSmoothingLengthSquared; // h2
uniform float uPressureConstants;      // kernel constants
uniform float uViscosityConstants;     // kernel constants
uniform float uDensityConstants;       // kernel constants
uniform float uParticleMass;           // mass of the particles
uniform float uRestDensity;            // rest density
uniform float uK;                      // constant

uniform vec3 uSimBoundsMin; // simulation bounds (min)
uniform vec3 uSimBoundsMax; // simulation bounds (max)

uniform float uStiffness = 1000.0;
uniform float uDampening = 25.60;

uniform vec3 uGravityDir;   // direction of gravity acceleration

uniform float uTicks;    // dt

// compute the pressure for a given density
float pressure(float k, float d, float d0) {
	return k * (d - d0);
}

//// poly 
//vec3 poly6_coeffs(float h2, vec3 rij) {
////	return rij * pow(vec3(max(h2 - dot(rij,rij),0.0)), vec3(2.0)); 
//	return rij * pow(max(h2-dot(rij,rij),0.0), 2.0);
//}

// compute spiky kernel gradient variables
vec3 spiky_coeffs(float h, vec3 rij, float r) {
//	return rij * pow(max(vec3(h-r),vec3(0.0)), vec3(2.0)) / r;
	return (rij * pow(max(h-r,0.0), 2.0) / r);
}

// compute viscosity kernel second order gradient variables
float viscosity_coeffs(float h, float r) {
	return max(h-r, 0.0);
}

void sph_forces(in vec3 ri,
                in float di,
                in vec3 vi,
                out vec3 fPressure,
                out vec3 fViscosity) {
	// variables
	int buckets1d[27];
	int iter    = 0;     // iterator
	int offset  = 0;     // texture offset
	float invDi = 1.0/di;
	fPressure  = vec3(0.0);
	fViscosity = vec3(0.0);
//	density    = 0.0; 

	// 3d bucket texture (in [0,D]x[0,W]x[0,H])
	vec3 relPos    = ri - uBucketBoundsMin;
	vec3 bucket3d  = floor(relPos / uBucketCellSize);

	// compute 1d bucket positions of neighbour cells
	for(int i=-1; i<2; ++i)
	for(int j=-1; j<2; ++j)
	for(int k=-1; k<2; ++k)
		buckets1d[i+1+3*(j+1)+9*(k+1)] = int(dot(bucket3d + vec3(i,j,k),
		                                         uBucket1dCoeffs));

	// loop through neighbours
	while(iter<27) {
		// get offset
		offset = imageLoad(imgHead, buckets1d[iter]).r;
		while(offset != -1) {
			if(offset != gl_VertexID) { 
				// get neighbour attributes
				vec3 rj  = texelFetch(sData0, offset).rgb;
				float dj = texelFetch(sData0, offset).a;
				vec3 vj  = texelFetch(sData1, offset).rgb;

				// compute rij and vij
				vec3 rij = ri - rj;
				vec3 vij = vj - vi;

				// precompute variables
				float r = length(rij);
				float invDj = 1.0/dj;

//				// evaluate density
//				density    -= dot(vij,poly6_coeffs(uSmoothingLengthSquared,
//				                                   rij));

				// evaluate pressure
				fPressure  += ( pressure(uK, di, uRestDensity)
				              + pressure(uK, dj, uRestDensity) ) * invDj
				            * spiky_coeffs(uSmoothingLength,
				                           rij,
				                           r);

				// evaluate viscosity
				fViscosity += vij * invDj
				            * viscosity_coeffs(uSmoothingLength, r);
			}
			// get next offset (if any)
			offset = imageLoad(imgList, offset).r;
		}
		++iter;
	}

	// multiply results by constants
	fPressure  *= (uPressureConstants * invDi);
	fViscosity *= (uViscosityConstants * invDi);
//	density    *= uDensityConstants;
}


vec3 boundary_force(in vec3 ri, in vec3 vi) {
	vec3 force = vec3(0.0);
	vec3 d;

#define EPSILON 0.5

	d = EPSILON - ri + uSimBoundsMin;
	force += max(d, 0.0)*(uStiffness*d-uDampening*vi);

	d = EPSILON + ri - uSimBoundsMax;
	force -= max(d, 0.0)*(uStiffness*d+uDampening*vi);

	return force;
}


vec3 gravity_force() {
	return 9.81 * uGravityDir * uParticleMass;
}


#ifdef _VERTEX_

layout(location=0) in vec4 iData0; // pos + density
layout(location=1) in vec4 iData1; // velocity

layout(location=0) out vec4 oData0; // pos + density
layout(location=1) out vec4 oData1; // velocity


#define iPosition iData0.xyz
#define iDensity  iData0.w
#define iVelocity iData1.xyz
#define oPosition oData0.xyz
#define oDensity  oData0.w
#define oVelocity oData1.xyz

void main()
{
	// variables
	vec3 acceleration;
	vec3 fPressure, fViscosity, fBoundary, fGravity;

	// compute forces
	sph_forces(iPosition, iDensity, iVelocity, fPressure, fViscosity);
	fBoundary = boundary_force(iPosition, iVelocity);
	fGravity  = gravity_force();

	// compute acceleration
	acceleration = (fPressure + fViscosity + fBoundary + fGravity)
	             / uParticleMass;

	// set attributes
	oDensity  = iDensity;// + density * uTicks;
	oVelocity = iVelocity + acceleration * uTicks;
	oPosition = iPosition + oVelocity * uTicks;

	oData1.w = length(acceleration);
	// check position
	oPosition = clamp(oPosition, uSimBoundsMin+0.05, uSimBoundsMax-0.05);

}

#endif // _VERTEX_


