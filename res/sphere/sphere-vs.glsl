#version 450
#extension GL_ARB_shading_language_include : require
#include "/defines.glsl"

in vec4 position;
in vec4 parentPosition;
in float radius;
#ifdef INTERPOLATION
in vec4 nextPosition;
#endif
uniform float animationDelta;
uniform float animationTime;
uniform float animationAmplitude;
uniform float animationFrequency;

uniform float clustering = 0.0;
uniform uint gridScale = 2;
uniform vec3 maxb;
uniform vec3 minb;

out float vRadius;

struct Cell
{
	// Sum of positions in this cell
	uvec4 pos;
	// Count of particles in this cell
	uint count;
	// Offset to first child cell.
	// Child cells: [offset, offset + 1, offset + 2, ..., offset + 7]
	// uint childOffset;
};

layout(std430, binding = 7) buffer scenegraphBuffer
{
	Cell cells[];
};

//	Simplex 4D Noise 
//	by Ian McEwan, Ashima Arts
//
vec4 permute(vec4 x){return mod(((x*34.0)+1.0)*x, 289.0);}
float permute(float x){return floor(mod(((x*34.0)+1.0)*x, 289.0));}
vec4 taylorInvSqrt(vec4 r){return 1.79284291400159 - 0.85373472095314 * r;}
float taylorInvSqrt(float r){return 1.79284291400159 - 0.85373472095314 * r;}

vec4 grad4(float j, vec4 ip){
  const vec4 ones = vec4(1.0, 1.0, 1.0, -1.0);
  vec4 p,s;

  p.xyz = floor( fract (vec3(j) * ip.xyz) * 7.0) * ip.z - 1.0;
  p.w = 1.5 - dot(abs(p.xyz), ones.xyz);
  s = vec4(lessThan(p, vec4(0.0)));
  p.xyz = p.xyz + (s.xyz*2.0 - 1.0) * s.www; 

  return p;
}

float snoise(vec4 v){
  const vec2  C = vec2( 0.138196601125010504,  // (5 - sqrt(5))/20  G4
                        0.309016994374947451); // (sqrt(5) - 1)/4   F4
// First corner
  vec4 i  = floor(v + dot(v, C.yyyy) );
  vec4 x0 = v -   i + dot(i, C.xxxx);

// Other corners

// Rank sorting originally contributed by Bill Licea-Kane, AMD (formerly ATI)
  vec4 i0;

  vec3 isX = step( x0.yzw, x0.xxx );
  vec3 isYZ = step( x0.zww, x0.yyz );
//  i0.x = dot( isX, vec3( 1.0 ) );
  i0.x = isX.x + isX.y + isX.z;
  i0.yzw = 1.0 - isX;

//  i0.y += dot( isYZ.xy, vec2( 1.0 ) );
  i0.y += isYZ.x + isYZ.y;
  i0.zw += 1.0 - isYZ.xy;

  i0.z += isYZ.z;
  i0.w += 1.0 - isYZ.z;

  // i0 now contains the unique values 0,1,2,3 in each channel
  vec4 i3 = clamp( i0, 0.0, 1.0 );
  vec4 i2 = clamp( i0-1.0, 0.0, 1.0 );
  vec4 i1 = clamp( i0-2.0, 0.0, 1.0 );

  //  x0 = x0 - 0.0 + 0.0 * C 
  vec4 x1 = x0 - i1 + 1.0 * C.xxxx;
  vec4 x2 = x0 - i2 + 2.0 * C.xxxx;
  vec4 x3 = x0 - i3 + 3.0 * C.xxxx;
  vec4 x4 = x0 - 1.0 + 4.0 * C.xxxx;

// Permutations
  i = mod(i, 289.0); 
  float j0 = permute( permute( permute( permute(i.w) + i.z) + i.y) + i.x);
  vec4 j1 = permute( permute( permute( permute (
             i.w + vec4(i1.w, i2.w, i3.w, 1.0 ))
           + i.z + vec4(i1.z, i2.z, i3.z, 1.0 ))
           + i.y + vec4(i1.y, i2.y, i3.y, 1.0 ))
           + i.x + vec4(i1.x, i2.x, i3.x, 1.0 ));
// Gradients
// ( 7*7*6 points uniformly over a cube, mapped onto a 4-octahedron.)
// 7*7*6 = 294, which is close to the ring size 17*17 = 289.

  vec4 ip = vec4(1.0/294.0, 1.0/49.0, 1.0/7.0, 0.0) ;

  vec4 p0 = grad4(j0,   ip);
  vec4 p1 = grad4(j1.x, ip);
  vec4 p2 = grad4(j1.y, ip);
  vec4 p3 = grad4(j1.z, ip);
  vec4 p4 = grad4(j1.w, ip);

// Normalise gradients
  vec4 norm = taylorInvSqrt(vec4(dot(p0,p0), dot(p1,p1), dot(p2, p2), dot(p3,p3)));
  p0 *= norm.x;
  p1 *= norm.y;
  p2 *= norm.z;
  p3 *= norm.w;
  p4 *= taylorInvSqrt(dot(p4,p4));

// Mix contributions from the five corners
  vec3 m0 = max(0.6 - vec3(dot(x0,x0), dot(x1,x1), dot(x2,x2)), 0.0);
  vec2 m1 = max(0.6 - vec2(dot(x3,x3), dot(x4,x4)            ), 0.0);
  m0 = m0 * m0;
  m1 = m1 * m1;
  return 49.0 * ( dot(m0*m0, vec3( dot( p0, x0 ), dot( p1, x1 ), dot( p2, x2 )))
               + dot(m1*m1, vec2( dot( p3, x3 ), dot( p4, x4 ) ) ) ) ;

}

uint powi(uint x, uint p) {
  uint y = 1;
  for (uint d = 0; d < p; ++d)
    y *= x;
  return y;
}

//#include "/globals.glsl";
//#define ANIMATION
void main()
{
  vRadius = radius;

	vec4 vertexPosition = position;

  vertexPosition.xyz = mix(vertexPosition.xyz, parentPosition.xyz, clustering);

  // if (0.1 < clustering) {
  //   const float BIAS = 0.001;
  //   uint offsetIndex = 0;
  //   float dist = 1000000.0;
  //   vec3 closestPos = vertexPosition.xyz;

  //   for (uint LOD = 1; LOD < 6; LOD++) {
  //     // 1. Navigate to correct cell:
  //     offsetIndex = (powi(8, LOD+1) - 8) / 7;
  //     const uint gridScale3 = gridScale * gridScale * gridScale;
  //     uint gridStep = powi(gridScale, LOD);

  //     vec3 bdir = (maxb - minb + 2.0 * BIAS).xyz / float(gridStep);

  //     // Calculate index:
  //     vec3 dir = vertexPosition.xyz - minb - BIAS;
  //     ivec3 gridIndex = ivec3(floor(dir / bdir));

  //     uint pCount = 0;

  //     // Check 3 ranges of increasing size
  //     // Note: Checking increasing ranges has n^3 complexity, as opposed to
  //     // checking different levels which should have log(d) complexity, so should
  //     // probably do that preemtively.
  //     for (int r = 0; r < 3; ++r) {
  //       for (int z = -r; z < r + 1; ++z) {
  //         for (int y = -r; y < r + 1; y++) {
  //           for (int x = -r; x < r + 1; x++) {
  //             // Skip grids we've already accounted for
  //             if (!(abs(x) == r || abs(y) == r || abs(z) == r))
  //               continue;

  //             ivec3 offsetGridIndex = ivec3(gridIndex.x + x, gridIndex.y + y, gridIndex.z + z);
  //             // Bounds checking:
  //             if (offsetGridIndex.x < 0 ||
  //               offsetGridIndex.y < 0 ||
  //               offsetGridIndex.z < 0 ||
  //               gridStep < offsetGridIndex.x ||
  //               gridStep < offsetGridIndex.y ||
  //               gridStep < offsetGridIndex.z)
  //               continue;

  //             uint index = offsetIndex + offsetGridIndex.x + offsetGridIndex.y * gridStep + offsetGridIndex.z * gridStep * gridStep;
  //             if (0 < cells[index].count) {
  //               vec3 cellPos = vec3(cells[index].pos.xyz) / float(cells[index].count * 1000);
  //               float d = length(cellPos - vertexPosition.xyz);
  //               if (d < dist) {
  //                 dist = d;
  //                 closestPos = cellPos;
  //                 pCount = cells[index].count;
  //               }
  //             }
  //           }
  //         }
  //       }
  //       if (dist < 10000.0)
  //         break;
  //     }
      
  //     // If grid count was less than threshold, we don't need to go any deeper.
  //     if (pCount < 10)
  //       break;
  //   }


  //   if (dist < 10000.0) {
  //     vertexPosition.xyz = mix(vertexPosition.xyz, closestPos, clustering);
  //   }
  // }

#ifdef INTERPOLATION
	vertexPosition.xyz = mix(position.xyz,nextPosition.xyz,animationDelta);
#endif

#ifdef ANIMATION
	vec3 offset;
	offset.x = snoise(vec4(vertexPosition.xyz,animationFrequency*animationTime));
	offset.y = snoise(vec4(vertexPosition.yyx,animationFrequency*animationTime));
	offset.z = snoise(vec4(vertexPosition.zyx,animationFrequency*animationTime));

	if (animationTime >= 0.0)
		vertexPosition.xyz += offset*animationAmplitude;
#endif
	gl_Position = vertexPosition;
}