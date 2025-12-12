#version 450

layout(std140, binding = 0) uniform UBO {
    mat4x4 transformMat;
} ubo;

// Sampler for RYGB texture
layout(binding = 1) uniform sampler2D rygbTexture;

layout(location = 0) out vec4 outColor;
layout(location = 1) in vec2 fragUV;

void main() {
    // Sample RYGB color from texture
    vec4 colorRYGB = texture(rygbTexture, fragUV);

    // Apply color transform matrix, converting RYGB to RGB/OCV
    vec4 colorViewSpace = vec4((ubo.transformMat * colorRYGB).xyz, 1.0);

    outColor = colorViewSpace;
}


