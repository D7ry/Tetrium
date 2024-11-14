#version 450

layout(binding = 1) uniform SystemUBOStatic {
    int texIndex;
} systemUBOStatic;

layout(binding = 2) uniform sampler2D textureSampler[16];

layout(location = 0) out vec4 outColor;

layout(location = 1) in vec2 fragUV; // outUV from vertex shader

void main() {
    int texIndex = systemUBOStatic.texIndex;
    outColor = texture(textureSampler[texIndex], fragUV);
}
