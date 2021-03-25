#version 450

#define BIAS 0.001

layout(location = 0) in vec3 inPos;

uniform mat4 modelViewProjectionMatrix = mat4(1.0);
uniform mat4 inverseModelViewProjectionMatrix = mat4(1.0);
uniform uint gridScale = 2;
uniform uint gridDepth = 9;
uniform vec3 minb;
uniform vec3 maxb;

// layout(binding = 0) uniform atomic_uint 

struct Cell
{
	// Sum of positions in this cell
	uvec4 pos;
	// Count of particles in this cell
	uint count;
	// Offset to first child cell.
	// Child cells: [offset, offset + 1, offset + 2, ..., offset + 7]
    // If 0, offset not set
	// uint childOffset;
};

layout(std140, binding = 7) buffer VertexBuffer
{
    Cell cells[];
};

flat out ivec3 gridIndex;
flat out vec3 vPos;
flat out uint index;

void main() {
    vec4 p = vec4(inPos, 1.0);
    vPos = p.xyz;

    uint offsetIndex = 0;

    for (uint d = 1; d <= gridDepth; ++d) {
        uint gridStep = 1;
        for (uint j = 0; j < d; ++j)
            gridStep *= gridScale;
        vec3 bdir = (maxb - minb + 2.0 * BIAS).xyz / float(gridStep);

        // Calculate index:
        vec3 dir = p.xyz - minb - BIAS;
        gridIndex = ivec3(dir / bdir);
        float dirdot = dot(dir, (maxb - minb + 2.0 * BIAS).xyz);
        if (/*dirdot <= 1.0 &&*/ 0.0 <= dirdot) {
            index = offsetIndex + gridIndex.x + gridIndex.y * gridStep + gridIndex.z * gridStep * gridStep;

            // Increment count
            atomicAdd(cells[index].count, 1);

            // Add position
            uvec3 upos = uvec3(floor(p * 1000.0)); // Multiply by 1000 and discretize to int
            // Atomic add position:
            atomicAdd(cells[index].pos.x, upos.x);
            atomicAdd(cells[index].pos.y, upos.y);
            atomicAdd(cells[index].pos.z, upos.z);
        }

        // Add previous cells: 8 -> 64 -> 512
        offsetIndex += gridStep * gridStep * gridStep;
    }

    gl_Position = modelViewProjectionMatrix * p;
}
