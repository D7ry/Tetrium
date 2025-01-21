#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

layout (binding = 0) uniform UBO_T {
    mat4 view;
    mat4 proj;
    mat4 model;
} ubo;

layout(binding = 1) uniform sampler2D texSampler;
vec2 rotate90(vec2 uv) {
  return vec2(-uv.y, uv.x);
}
vec2 flipUVDiagonal1(vec2 uv) {
  return vec2(1.0 - uv.y, uv.x); 
}

vec2 flipUVDiagonal2(vec2 uv) {
  return vec2(uv.y, 1.0 - uv.x); 
}
vec2 flipUV(vec2 uv, bool flipX, bool flipY) {
  return vec2(flipX ? 1.0 - uv.x : uv.x, flipY ? 1.0 - uv.y : uv.y);
}

// convert spherical normal to uv to index into a 3x4 cubemap.
vec2 normalToCubemapUV(vec3 normal) {
    // First, find which face we're on by determining primary axis
    vec3 absN = abs(normal);
    float maxNAbs = max(max(absN.x, absN.y), absN.z);
    
    // Initialize UV coordinates
    vec2 uv;
    
    // Scale factor for final UV mapping
    const float invFaceWidth = 0.25;    // 1/4
    const float invFaceHeight = 0.333333; // 1/3
    
    if (maxNAbs == absN.y) {
        // Y-axis face (top/bottom)
        uv = vec2(normal.x, -normal.z) / maxNAbs;  // Fixed Z orientation
        uv = uv * 0.5 + 0.5; // Convert from [-1,1] to [0,1]
        uv = clamp(uv, -1.0, 1.0); // Clamp to avoid NaN in atan
        if (normal.y > 0.0) {
            // Positive Y face (top)
            uv = flipUV(uv, false, true);
            uv.x = uv.x * invFaceWidth + invFaceWidth;  // Second column
            uv.y = uv.y * invFaceHeight;                // Top row
        } else {
            // Negative Y face (bottom)
            uv.x = uv.x * invFaceWidth + invFaceWidth;  // Second column
            uv.y = uv.y * invFaceHeight + 2.0 * invFaceHeight; // Bottom row
        }
    }
    else if (maxNAbs == absN.x) {
        // X-axis face (right/left)
        uv = vec2(-normal.z, -normal.y) / maxNAbs;  // Fixed orientation
        uv = uv * 0.5 + 0.5;
        uv = clamp(uv, -1.0, 1.0); // Clamp to avoid NaN in atan
        if (normal.x > 0.0) {
            // Positive X face (right)

            uv.x = uv.x * invFaceWidth + 2.0 * invFaceWidth; // Third column
            uv.y = uv.y * invFaceHeight + invFaceHeight;     // Middle row
        } else {
            // Negative X face (left)
            uv = flipUV(uv, true, false);
            uv.x = uv.x * invFaceWidth;                      // First column
            uv.y = uv.y * invFaceHeight + invFaceHeight;     // Middle row

        }
    }
    else {
        // Z-axis face (front/back)
        uv = vec2(normal.x, -normal.y) / maxNAbs;
        uv = uv * 0.5 + 0.5;
        
        uv = clamp(uv, -1.0, 1.0); // Clamp to avoid NaN in atan
        if (normal.z > 0.0) {
            // Positive Z face (front)
            uv.x = uv.x * invFaceWidth + invFaceWidth;     // Second column
            uv.y = uv.y * invFaceHeight + invFaceHeight;   // Middle row

        } else {
            // Negative Z face (back)
            uv = flipUV(uv, true, false);
            uv.x = uv.x * invFaceWidth + 3.0 * invFaceWidth; // Fourth column
            uv.y = uv.y * invFaceHeight + invFaceHeight;     // Middle row
        }
    }
    
    return uv;
}

void main() {
    // assume fargNormal is normalized

    vec2 uv = normalToCubemapUV(fragNormal);
    vec4 texColor = texture(texSampler, uv);
    outColor = texColor;
    
    //outColor = vec4(fragNormal, 1.0);
}
