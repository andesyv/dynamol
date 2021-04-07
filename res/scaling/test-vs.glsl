#version 450

layout(location = 0) in uvec4 inPos;
layout(location = 1) in uvec4 inCount;

out vec3 col;

// struct Cell
// {
// 	// Sum of positions in this cell
// 	uvec4 pos;
// 	// Count of particles in this cell
// 	uint count;
// 	// Offset to first child cell.
// 	// Child cells: [offset, offset + 1, offset + 2, ..., offset + 7]
//     // If 0, offset not set
// 	// uint childOffset;
// };

// layout(std140, binding = 7) buffer VertexBuffer
// {
//     Cell cells[];
// };

uniform mat4 modelViewProjectionMatrix;

void main() {
    uint offset = 80;
    vec3 pos = vec3(inPos.xyz) / float(inCount.x * 1000);

	const vec3 positions[] = {
		vec3(-0.5, -0.5, 0.0),
		vec3(0.0, 0.5, 0.0),
		vec3(0.5, -0.5, 0.0)
	};

    col = vec3(inPos.xyz) / 4000000000.0;
    gl_Position = modelViewProjectionMatrix * vec4(pos, 1.0);
}