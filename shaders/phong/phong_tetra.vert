#version 450

layout(binding = 0) uniform UBOStatic {
    mat4 view;
    mat4 proj;
} uboStatic;

layout(binding = 1) uniform UBODynamic {
    mat4 model;
    bool isRGB; // isRGB = !isOCV
    int textureRGB;
    int textureOCV;
} uboDynamic;


layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec4 fragPos; // frag position in world
layout(location = 4) out vec4 fragGlobalLightPos; // light position in world

layout(location = 5) flat out int fragTexIndex;

const vec3 globalLightPos = vec3(-6, -3, 0.0);

void main() {
    // dw about optimization our superior compiler stack constant props all those
    mat4 model = uboDynamic.model;
    mat4 view = uboStatic.view;
    mat4 proj = uboStatic.proj;

    vec4 world_pos = model * vec4(inPosition, 1.0);
    gl_Position = proj * view * world_pos;

    fragColor = inColor;
    fragTexCoord = inTexCoord;
    fragNormal = inNormal;
    fragPos = world_pos;
    fragGlobalLightPos = vec4(globalLightPos, 1.f);

    fragTexIndex = uboDynamic.isRGB ? uboDynamic.textureRGB : uboDynamic.textureOCV;
}
