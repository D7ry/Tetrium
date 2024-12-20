
#version 450

layout(binding = 0) uniform UBO {
    mat4x3 transformMat;
} ubo;

// samplers that samples frame buffer from RYGB pass
layout(binding = 1) uniform sampler2D canvasRYGB;

layout(location = 0) out vec4 outColor;

layout(location = 1) in vec2 fragUV; // outUV from vertex shader

void main() {
    // vec4 are single-precision floats already
    vec4 colorRYGB = texture(canvasRYGB, fragUV);

    // apply color transform matrix, converting RYGB to RGB/OCV
    vec4 colorViewSpace = vec4(ubo.transformMat * colorRYGB, 1.f);

    outColor = colorViewSpace;
}
