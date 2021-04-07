#version 450

layout(location = 0) in uint gridIndex;

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

void main() {
    uint prevCount = atomicExchange(cells[gridIndex].count, 0);
    if (prevCount == 0)
        return;
    
    uint id = cells[gridIndex].pos.w;
    vec4 pos = vec4(cells[gridIndex].pos.xyz / float(prevCount * 1000), 0.);
    pos.w = uintBitsToFloat(id);

    gl_Position = pos;
}