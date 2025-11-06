#pragma once
#include <array>

#if __APPLE__
#include <vulkan/vulkan_beta.h> // VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME, for molten-vk support
#endif

#include "TetriumColor/ColorSpaceType.h"
#include "vulkan/vulkan.hpp"

// absolute constants
const int NUM_FRAME_IN_FLIGHT = 2; // how many frames to pipeline
const int TEXTURE_ARRAY_SIZE = 16; // size of the texture array for bindless texture indexing

// default setting values.
// Note taht values are only used on engine initialization
// and is subject to change at runtime.
namespace DEFAULTS
{
#if __APPLE__
const size_t WINDOW_WIDTH = 1280;
const size_t WINDOW_HEIGHT = 720;
#else
const size_t WINDOW_WIDTH = 1440;
const size_t WINDOW_HEIGHT = 900;
#endif // __APPLE__
const float FOV = 90.f;
const float ZNEAR = 0.1f;
const float ZFAR = 500.f;

const float PROFILER_PERF_PLOT_RANGE_SECONDS = 10; // how large the plot window is

#if !__APPLE__
const float MAX_FPS = 144.f;
#else
const float MAX_FPS = 60.f;
#endif // __APPLE__

namespace ImGui
{
#if !__APPLE__
const float DEFAULT_FONT_SIZE = 44;
#else
const float DEFAULT_FONT_SIZE = 14;
#endif // __APPLE__
const int TEXTURE_DESCRIPTOR_POOL_SIZE = 1024;
} // namespace ImGui

namespace Engine
{
#ifdef NDEBUG
const bool ENABLE_VALIDATION_LAYERS = false;
#else
const bool ENABLE_VALIDATION_LAYERS = true;
#endif // NDEBUG

const std::array<const char*, 1> VALIDATION_LAYERS = {"VK_LAYER_KHRONOS_validation"};

const char* const APPLICATION_NAME = "Tetrium";

const struct
{
    uint32_t major = 1;
    uint32_t minor = 0;
    uint32_t patch = 0;
} APPLICATION_VERSION;

const char* const ENGINE_NAME = "Tetrium Engine";

const struct
{
    uint32_t major = 0;
    uint32_t minor = 0;
    uint32_t patch = 3;
} ENGINE_VERSION;

const char* const BANNER_TEXT = "___  ___ ___  __               \n"
                                " |  |__   |  |__) | |  |  |\\/| \n"
                                " |  |___  |  |  \\ | \\__/  |  | \n";

} // namespace Engine

} // namespace DEFAULTS

using INDEX_BUFFER_INDEX_TYPE = unsigned int;

namespace GLOBALS
{
/**
 * Expected extent of the DLP's presented content.
 * This extent is different from the projector's swapchain; the swapchain's pixels maps 1:1 to the
 * individual physical lights of the DLP. The DLP optically squishes the output from the LED into a
 * view of DISPLAY_EXTENT. When rendering in none-pattern mode, the DLP internally re-samples the
 * HDMI input to this extent, projects the light, and then optically corrects the aspect ratio using
 * its lenses. When rendering in pattern mode, however,, the re-sampling does not happen in DLP.
 * Therefore we we-sample the image ourselves.
 */
#if !__APPLE__
const vk::Extent2D DISPLAY_EXTENT{2560, 1600};
#else
const vk::Extent2D DISPLAY_EXTENT{
    DEFAULTS::WINDOW_WIDTH * 2,
    DEFAULTS::WINDOW_HEIGHT * 2}; // stupid MacOS
#endif // __APPLE__

} // namespace GLOBALS

// Helper function to get the appropriate output color space for the current platform
// On Mac (dev machines), use sRGB for simulation; on other platforms use DISP_6P for projector
inline TetriumColor::ColorSpaceType GetOutputColorSpace()
{
#if defined(__APPLE__)
    return TetriumColor::ColorSpaceType::SRGB;
#else
    return TetriumColor::ColorSpaceType::DISP_6P;
#endif
}

// Helper function to get texture paths based on output color space
// Returns pair of (RGB path, OCV path)
inline std::pair<std::string, std::string> GetTexturePaths(
    const std::string& baseFilename,
    TetriumColor::ColorSpaceType outputSpace = GetOutputColorSpace()
)
{
    if (outputSpace == TetriumColor::ColorSpaceType::SRGB) {
        // For SRGB output, use _SRGB.png for both RGB and OCV (MONO_COLOR_SPACE will only show RGB)
        INFO("Using SRGB output for texture paths");
        std::string srgbPath = baseFilename + "_SRGB.png";
        return {srgbPath, srgbPath};
    } else {
        // For DISP_6P output, use separate _RGB.png and _OCV.png files
        return {baseFilename + "_RGB.png", baseFilename + "_OCV.png"};
    }
}
