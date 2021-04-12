#version 450

layout(pixel_center_integer) in vec4 gl_FragCoord;

in vec4 gFragmentPosition;

/** Note: sampler2DShadow is used for depth comparison mode
 * which filters out values before applying interpolation.
 * Comparison mode also only works if texture has comparison
 * mode enabled.
 * See https://www.khronos.org/opengl/wiki/Sampler_Object#Comparison_mode 
 */
layout (binding = 0) uniform sampler2D LOD0Depth;
layout (binding = 1) uniform sampler2D LOD1Depth;

layout (binding = 2) uniform sampler2D LOD0Surface;
layout (binding = 3) uniform sampler2D LOD1Surface;

layout (binding = 4) uniform sampler2D LOD0Normal;
layout (binding = 5) uniform sampler2D LOD1Normal;

layout (binding = 6) uniform sampler2D LOD0Diffuse;
layout (binding = 7) uniform sampler2D LOD1Diffuse;

uniform float interpolation = 0.0;
uniform float smoothness = 0.0;
uniform vec3 lightPosition = vec3(0.0);
uniform mat4 modelViewProjectionMatrix = mat4(1.0);

out vec4 fragColor;

// https://www.iquilezles.org/www/articles/smin/smin.htm
// polynomial smooth min (k = 0.1);
float smin( float a, float b, float k )
{
    float h = max( k-abs(a-b), 0.0 )/k;
    return min( a, b ) - h*h*k*(1.0/4.0);
}

float scrDepth(vec2 fragCoord) {
    // return smin(texture(LOD0Depth, fragCoord).r, texture(LOD1Depth, fragCoord).r, interpolation);
    return mix(texture(LOD0Depth, fragCoord).r, texture(LOD1Depth, fragCoord).r, interpolation);
}

float linearizeDepth(vec3 pos)
{
	float far = gl_DepthRange.far; 
	float near = gl_DepthRange.near;
	vec4 clip_space_pos = modelViewProjectionMatrix * vec4(pos, 1.0);
	float ndc_depth = clip_space_pos.z / clip_space_pos.w;
	return (((far - near) * ndc_depth) + near + far) / 2.0;
}

// float gradient(vec2 fragPos) {
    
// }

void main() {
    vec2 uv = gFragmentPosition.xy * 0.5 + 0.5;

    const float t = clamp(interpolation, 0.0, 1.0);

    float derivativeScale = 30.0;

    // float depth = mix(texture(LOD0Depth, uv).r, texture(LOD1Depth, uv).r, t);
    // vec3 xtan = normalize(vec3(1.0, 0.0, dFdx(scrDepth(uv)) * derivativeScale));
    // vec3 ytan = normalize(vec3(0.0, 1.0, dFdy(scrDepth(uv)) * derivativeScale));
    // vec3 normal = normalize(cross(xtan, ytan));



    // fragColor = vec4(texelFetch(informationTex,ivec2(gl_FragCoord.xy),0).rgb, 1.0);
    fragColor = vec4(vec3(scrDepth(uv)), 1.);
    // fragColor = vec4(normal, 1.0);
    vec4 lod0pos = /*modelViewProjectionMatrix */ vec4(texture(LOD0Surface, uv).rgb, 1.0);
    // lod0pos.xyz *= lod0pos.w;
    vec4 lod1pos = /*modelViewProjectionMatrix */ vec4(texture(LOD1Surface, uv).rgb, 1.0);
    // lod1pos.xyz *= lod1pos.w;
    // vec4 interpPos = mix(lod0pos, lod1pos, u);
    float interpPos = min(linearizeDepth(lod0pos.xyz), linearizeDepth(lod1pos.xyz));

    float d0 = linearizeDepth(lod0pos.xyz); // texture(LOD0Depth, uv).r;
    float d1 = linearizeDepth(lod1pos.xyz); // texture(LOD1Depth, uv).r;

    float u = step(d0, d1);
    // float u = 1.0 - smoothstep(d0, d0 + smoothness, d1);


    // fragColor = vec4(vec3(interpPos), 1.0);
    // fragColor = vec4(interpPos.xyz, 1.0);

    // fragColor = vec4(vec3(mix(d0, d1, 0.5)), 1.0);


    // float du = dFdx(min(d0, d1));
    // float dv = dFdy(min(d0, d1));
    
    // float depth = smin(d0, d1, max(smoothness, 0.01));
    // fragColor = vec4(vec3(depth), 1.0);
    
    fragColor = vec4(vec3(scrDepth(uv) + 0.5 * dFdx(scrDepth(uv)) + 0.5 * dFdy(scrDepth(uv))), 1.0);
}