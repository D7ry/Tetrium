#version 450


// binding = {0,1} reserved for UBOs
layout(binding = 2) uniform sampler2D textureSampler[16];


layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec4 fragPos;
layout(location = 4) in vec4 fragGlobalLightPos;
layout(location = 5) flat in int fragTexIndex;

layout(location = 0) out vec4 outColor;

vec3 ambientLighting = vec3(0.6431, 0.6431, 0.6431);
vec3 lightSourceColor = vec3(0.7, 1.0, 1.0); // White light
float lightSourceIntensity = 100;

void main() {
    vec4 textureColor = texture(textureSampler[fragTexIndex], fragTexCoord);
    vec3 diffToLight = vec3(fragGlobalLightPos - fragPos);
    vec3 lightDir = normalize(diffToLight);

    float distToLight = length(diffToLight);

    float cosTheta = max(dot(fragNormal, lightDir), 0.0);
    vec3 diffuseLighting = cosTheta * lightSourceColor * lightSourceIntensity / (distToLight * distToLight);

    vec4 diffuseColor = vec4(textureColor.rgb * diffuseLighting, textureColor.a);

    vec4 ambientColor = vec4(textureColor.rgb * ambientLighting.rgb, textureColor.a);

    outColor = vec4(0.f, 0.f, 1.f, 1.f);
}
