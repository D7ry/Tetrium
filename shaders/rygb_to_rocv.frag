#version 450

layout(binding = 1) uniform SystemUBOStatic {
    mat4 transformMat;
    int frameBufferIdx;
    int toRGB; // toOCV = !toRGB;
} systemUBOStatic;

layout(binding = 2) uniform sampler2D rygbFrameBufferSampler[3];

layout(location = 0) out vec4 outColor;

layout(location = 1) in vec2 fragUV; // outUV from vertex shader

void main() {
    bool toRGB = systemUBOStatic.toRGB == 1;
    vec4 colorRYGB = texture(rygbFrameBufferSampler[systemUBOStatic.frameBufferIdx], fragUV);

    // [R, O, C, V]
    vec4 colorROCV = systemUBOStatic.transformMat * colorRYGB;

    if (toRGB) {
        // [R, 0, 0, 0]
        outColor = vec4(colorROCV.x, 0, 0, 0);
    } else {
        // [0, O, C, V]
        outColor = vec4(0, colorROCV.y, colorROCV.z, colorROCV.w);
    }
}
