/**
Generates a cubemap texture in RYGB space, given luminance and saturation
https://github.com/imjal/TetriumColor/blob/main/scripts/generation/testVSXYZtoRYGB.py
*/
#version 450

layout(std140, binding = 0) uniform UBO {
    float luminance;
    float saturation;
} ubo;

layout(location = 0) out vec4 outColor;

layout(location = 1) in vec2 fragUV; // outUV from vertex shader

// Convert UV coordinates from a full cubemap into XYZ coordinates
// assuming the cubemap faces are arranged in a 3x2 grid.
// u and v should be in the range [0.0, 1.0].
vec3 convertCubemapUVToXYZ(vec2 uv, float radius) {
    // Assumes a 4x3 grid layout for the cubemap faces (4 columns, 3 rows)
    float faceWidth = 1.0 / 4.0;
    float faceHeight = 1.0 / 3.0;

    // Calculate the face index (0-3 for columns, 0-2 for rows)
    int faceX = int(floor(uv.x * 4.0));  // Which column (0-3)
    int faceY = int(floor(uv.y * 3.0));  // Which row (0-2)

    // Local u, v for each face, in [0, 1] range
    float uLocal = uv.x * 4.0 - float(faceX);
    float vLocal = uv.y * 3.0 - float(faceY);

    // Convert to [-1, 1] range
    float uc = 2.0 * uLocal - 1.0;
    float vc = 2.0 * vLocal - 1.0;

    // Initialize the XYZ coordinates
    vec3 xyz = vec3(0.0, 0.0, 0.0);

    // Process each cubemap face based on the 4x3 grid layout
    if (faceX == 1 && faceY == 0) {
        // Bottom (Negative Y)
        xyz = vec3(uc, -1.0, vc);
    } else if (faceX == 1 && faceY == 2) {
        // Top (Positive Y)
        xyz = vec3(uc, 1.0, -vc);
    } else if (faceX == 0 && faceY == 1) {
        // Left (Negative X)
        xyz = vec3(-1.0, vc, uc);
    } else if (faceX == 1 && faceY == 1) {
        // Front (Positive Z)
        xyz = vec3(uc, vc, 1.0);
    } else if (faceX == 2 && faceY == 1) {
        // Right (Positive X)
        xyz = vec3(1.0, vc, -uc);
    } else if (faceX == 3 && faceY == 1) {
        // Back (Negative Z)
        xyz = vec3(-uc, vc, -1.0);
    }

    // Normalize to the given radius if requested
    xyz = normalize(xyz) * radius;

    return xyz;
}
void main() {
    // TODO: use luminance and saturation from UBO

    // use only fragUV for now
    vec3 xyz = convertCubemapUVToXYZ(fragUV, ubo.saturation);
    xyz = (xyz + 1.0) * 0.5;
    outColor = vec4(xyz, 1.0);
    //outColor = vec4(xyz.x, 0.0, 0.0, 1.0);
}
