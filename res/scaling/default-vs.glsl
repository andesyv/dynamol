#version 450

layout(location = 0) in vec3 inPos;

out vec3 fragPos;
out vec2 uv;

void main() {
    uv = (inPos.xy + 1.0) * 0.5;
    fragPos = inPos;
    gl_Position = vec4(inPos, 1.0);
}