#version 450

#define N 6

layout(points) in;
layout(triangle_strip, max_vertices = N * 3) out;

flat out vec3 atomPos;

vec2 rotate(vec2 inp, float angle) {
	mat2 T = mat2(
		vec2(cos(angle), -sin(angle)),
		vec2(sin(angle), cos(angle))
	);
	return T * inp;
}

void main() {
    vec4 pos = gl_in[0].gl_Position;
    atomPos = pos.xyz;

    float angle = 6.28 / float(N);

    for (int i = 0; i < N; ++i) {
        gl_Position = pos;
        EmitVertex();
        gl_Position = pos + vec4(rotate(vec2(0.1, 0.), angle * (i - 1)), 0., 0.);
        EmitVertex();
        gl_Position = pos + vec4(rotate(vec2(0.1, 0.), angle * i), 0., 0.);
        EmitVertex();
        EndPrimitive();
    }

    // gl_Position = pos + vec4(vec2(-0.1, -0.1), 0., 0.);
    // EmitVertex();
    // gl_Position = pos + vec4(vec2(0.1, -0.1), 0., 0.);
    // EmitVertex();
    // gl_Position = pos + vec4(vec2(0.1, 0.1), 0., 0.);
    // EmitVertex();
    // EndPrimitive();

    // for (int i = 0; i < 3; ++i) {
    // }
}