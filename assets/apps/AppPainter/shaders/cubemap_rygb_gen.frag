
#version 450

layout(std140, binding = 0) uniform UBO {
    mat4x4 transformMat;
} ubo;

layout(location = 0) out vec4 outColor;

layout(location = 1) in vec2 fragUV; // outUV from vertex shader

void main() {

    // use only fragUV for now
    outColor = vec4(fragUV.x, fragUV.y, 1.f, 1.f);
}
