#version 450

layout(triangles, equal_spacing, ccw) in;

in vec3 fragPos_es_in[];

out vec3 fragPos_fs;

void main() {
    // Note: gl_TessCoord are barysentric coordinates of first, second and third triangle coordinate
    vec3 position = fragPos_es_in[0] * gl_TessCoord.x + fragPos_es_in[1] * gl_TessCoord.y + fragPos_es_in[2] * gl_TessCoord.z;
    fragPos_fs = position;

    gl_Position = vec4(position, 1.0);
}