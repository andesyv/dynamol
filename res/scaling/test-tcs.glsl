#version 450

layout(vertices = 3) out;

// Note on naming: Name outputs from one shader step as the input to another as this will make individual shaders more clear
in vec3 fragPos_tcs_in[];

out vec3 fragPos_es_in[];

void main() {
    fragPos_es_in[gl_InvocationID] = fragPos_tcs_in[gl_InvocationID];

    gl_TessLevelOuter[0] = 3;
    gl_TessLevelOuter[1] = 2;
    gl_TessLevelOuter[2] = 2;
    // gl_TessLevelOuter[3] = 1;
    gl_TessLevelInner[0] = 2;
    // gl_TessLevelInner[1] = 1;
}
