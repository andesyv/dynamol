#version 450

in vec4 gFragmentPosition;

/** Note: sampler2DShadow is used for depth comparison mode
 * which filters out values before applying interpolation.
 * Comparison mode also only works if texture has comparison
 * mode enabled.
 * See https://www.khronos.org/opengl/wiki/Sampler_Object#Comparison_mode 
 */
layout (binding = 0) uniform sampler2D LOD0;
layout (binding = 1) uniform sampler2D LOD1;

uniform float interpolation = 0.0;

out vec4 fragColor;

void main() {
    vec2 uv = gFragmentPosition.xy * 0.5 + 0.5;

    float depth = mix(texture(LOD0, uv).r, texture(LOD1, uv).r, clamp(interpolation, 0.0, 1.0));

    fragColor = vec4(vec3(depth), 1.0);
}