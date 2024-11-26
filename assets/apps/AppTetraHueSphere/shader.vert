#version 450

layout (binding = 0) uniform UBO_T {
    mat4 view;
    mat4 proj;
    mat4 model;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragNormal;

void main() {
    vec4 world_pos = ubo.model * vec4(inPosition, 1.0);
    gl_Position = ubo.proj * ubo.view * world_pos;

    fragColor = inColor;
    fragTexCoord = inTexCoord;
    fragNormal = inNormal;
}
