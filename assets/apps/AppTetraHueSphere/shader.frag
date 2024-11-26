#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

layout (binding = 0) uniform UBO_T {
    mat4 view;
    mat4 proj;
    mat4 model;
    int textureIndex;
} ubo;

layout(binding = 1) uniform sampler2D texSampler[4]; // [uglyRGB, uglyOCV, prettyRGB, prettyOCV]

void main() {

    vec4 texColor = texture(texSampler[ubo.textureIndex], fragTexCoord);
    outColor = texColor;
}
