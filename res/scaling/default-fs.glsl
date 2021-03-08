#version 450

in vec3 fragPos;
in vec2 uv;

out vec4 fragColor;

void main() {
    fragColor = vec4(uv, 0., 1.);
}