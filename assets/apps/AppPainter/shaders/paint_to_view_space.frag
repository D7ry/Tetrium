
#version 450

layout(std140, binding = 0) uniform UBO {
    mat4x4 transformMat;
} ubo;

// samplers that samples frame buffer from RYGB pass
layout(binding = 1) uniform sampler2D canvasRYGB;

layout(location = 0) out vec4 outColor;

layout(location = 1) in vec2 fragUV; // outUV from vertex shader

void main() {
    // vec4 are single-precision floats already
    vec4 colorRYGB = texture(canvasRYGB, fragUV);

    // apply color transform matrix, converting RYGB to RGB/OCV
    vec4 colorViewSpace = vec4((ubo.transformMat * colorRYGB).xyz, 1.f);

    outColor = colorViewSpace;
    //outColor = vec4(colorRYGB.rba, 1.f);
}
