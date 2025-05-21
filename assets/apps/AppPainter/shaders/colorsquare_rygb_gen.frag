/**
Generates a cubemap texture in RYGB space, given luminance and saturation
https://github.com/imjal/TetriumColor/blob/main/scripts/generation/testVSXYZtoRYGB.py
*/
#version 450

layout(std140, binding = 0) uniform UBO {
    float cubemap_u;
    float cubemap_v;
} ubo;

layout(binding = 1) uniform sampler2D t_vshMaxSaturationLUT;

layout(location = 0) out vec4 outColor;

layout(location = 1) in vec2 fragUV; // outUV from vertex shader

const float g_luminanceMin = 0.33837989700422405;
const float g_luminanceRange = 1.6616201029957725;

const float g_saturationMin = 0.50594715131446155;
const float g_saturationRange = 1.0311205558296506;


const mat4 g_heringToRYGB = mat4(
    vec4(0.5, 0.5, 0.5, 0.5),
    vec4(-0.28867513459481287, -0.28867513459481287, -0.28867513459481287, 0.866025403784439),
    vec4(-0.408248290463863, -0.408248290463863, 0.8164965809277261, -3.183243964787847e-17),
    vec4(-0.7071067811865477, 0.7071067811865476, -9.80883324026333e-17, -7.571128974804755e-19)
);

const mat4 g_heringToDisp = mat4(
    vec4(0.5000000000000183, 0.5000000000000026, 0.5000000000000002, 0.499999999999987),
    vec4(-0.2860494702830666, -0.2631280610866057, 0.7861275057955296, -0.34856903388084665),
    vec4(-0.2864170221014012, 0.7038041314050284, 0.08365259538586325, -0.9736861462436817),
    vec4(-0.7523786286615098, 0.08743393179139701, -0.0007915212975174189, 0.9167057740993428)
);

const mat3 g_invMetamericDirMat = mat3(
    vec3(0.9865474651933757, 0.009838790494736325, -0.16317872754169252),
    vec3(0.009838790494736328, 0.9928041963993547, 0.11934414863508082),
    vec3(0.16317872754169252, -0.11934414863508083, 0.9793516615927303)
);

const float g_maxLuminance = 2;

vec3 convertCartesianToSpherical(vec3 xyz) {
    float x = xyz.x;
    float y = xyz.y;
    float z = xyz.z;

    float radius = length(xyz);
    float theta = atan(y, x);
    float phi = acos(clamp(z / radius, -1.0, 1.0));

    return vec3(radius, theta, phi);
}

vec3 convertSphericalToCartesian(vec3 spherical) {
    float r = spherical.x;
    float theta = spherical.y;
    float phi = spherical.z;
    
    float x = r * sin(phi) * cos(theta);
    float y = r * sin(phi) * sin(theta);
    float z = r * cos(phi);

    return vec3(x, y, z);
}

vec4 convertVSHHToHering(vec4 vshh) {
    return vec4(vshh.x, convertSphericalToCartesian(vshh.yzw));
}

vec4 convertHeringToVSHH(vec4 hering) {
    float luminance = hering.x;
    vec3 spherical = convertCartesianToSpherical(hering.yzw);
    
    float radius = spherical.x;
    float theta = spherical.y;
    float phi = spherical.z;
    
    return vec4(luminance, radius, theta, phi);
}

float solveForBoundary(float luminance, float max_l, float luminance_cusp, float saturation_cusp) {
    if (luminance >= luminance_cusp) {
        float slope = -(max_l - luminance_cusp) / saturation_cusp;
        return (luminance - max_l) / slope;
    } else {
        float slope = luminance_cusp / saturation_cusp;
        return luminance / slope;
    }
}

vec4 remapVSHHGamutPoints(vec4 vshh) {

    float vshhLuminance = vshh.x;
    float vshhSaturation = vshh.y;
    float vshhTheta = vshh.z;
    float vshhPhi = vshh.w;
    
    vec2 lutResult = texture(t_vshMaxSaturationLUT, fragUV).xy;

    float luminanceCusp = lutResult.x * g_luminanceMin + g_luminanceRange;
    float saturationCusp = lutResult.y * g_saturationMin + g_saturationRange;

    float remappedSaturation = solveForBoundary(vshhLuminance, g_maxLuminance, luminanceCusp, saturationCusp);
    remappedSaturation = min(remappedSaturation, vshhSaturation);
    
    vec4 remappedVSHH = vec4(vshhLuminance, remappedSaturation, vshhTheta, vshhPhi);
    return remappedVSHH;
}

// Convert spherical cartesian coordinates, into RYGB color space
// basically a parallelized version of :
// https://github.com/imjal/TetriumColor/blob/6e73833890068f72b79b152b00b9044224e657c0/TetriumColor/PsychoPhys/HueSphere.py#L100C1-L101C1
vec4 convertCartesianToRYGB(vec3 xyz, float luminance, float saturation) {
    vec4 hering = vec4(luminance, xyz);
    vec4 vshh = convertHeringToVSHH(hering);
    vshh.y = saturation; // override radius component with saturation
    
    hering = convertVSHHToHering(vshh); // convert back to hering space
    
    vec4 colorRYGB = g_heringToRYGB * hering;

    //vec4 vshhRemapped = remapVSHHGamutPoints(vshh);
    //vec4 heringRemapped = convertVSHHToHering(vshhRemapped);
    //vec4 colorRYGB = g_heringToRYGB * heringRemapped;

    return colorRYGB;
}

// Convert UV coordinates from a full cubemap into XYZ coordinates
// assuming the cubemap faces are arranged in a 3x2 grid.
// u and v should be in the range [0.0, 1.0].
vec3 convertCubemapUVToCartesian(vec2 uv, float radius) {
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
    
    if (any(notEqual(xyz, vec3(0.0, 0.0, 0.0)))) {
        xyz = normalize(xyz) * radius; // scale to radius, only if not zero vector
    }

    return xyz;
}

//https://en.wikipedia.org/wiki/Degenerate_bilinear_form
// uv in [0,1]
vec3 mapUVToBarycentric(vec2 uv) {
    float u = uv[0];
    float v = uv[1];
    vec3 barycentric;

    // derive baricentric coords
    barycentric.z = v;
    barycentric.x = (1-v) * (1-u);
    barycentric.y = 1 - barycentric.z - barycentric.x;
    return barycentric;
}
vec2 mapUVToTriangle(vec2 uv, vec2 C) {
    uv[1] = pow(uv[1], 1.f/3.f);
    vec3 bary = mapUVToBarycentric(uv);
    return vec2(0, 0) * bary.z + vec2(0, 2) * bary.x + C * bary.y;
}

void main() {
    // sample lut to get bound for value and saturation
    vec2 lutResult = texture(t_vshMaxSaturationLUT, vec2(ubo.cubemap_u, ubo.cubemap_v)).xy;
    //float value = lutResult.x;
    //float saturation = lutResult.y;

    float value = lutResult.x * g_luminanceMin + g_luminanceRange;
    float saturation = lutResult.y * g_saturationMin + g_saturationRange;
    
    // remap cubemap uv onto bounded color triangle,
    // essentially collapsing the bottom edge
    // the mapped coordinate we use as v and s
    vec2 sv = mapUVToTriangle(fragUV, vec2(saturation, value));

    vec3 xyz = convertCubemapUVToCartesian(vec2(ubo.cubemap_u, ubo.cubemap_v), sv[0]);

    vec4 rygb = convertCartesianToRYGB(xyz, sv[1], sv[0]);

    outColor = rygb;
    //outColor = vec4(sv, 0, 1);

    //outColor = vec4(saturation, 0, 0, 1);
    //outColor = vec4(fragUV, 0, 1);
}
