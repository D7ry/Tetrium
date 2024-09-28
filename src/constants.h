#pragma once
#include <array>

#if __APPLE__
#include <vulkan/vulkan_beta.h> // VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME, for molten-vk support
#endif

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
const float DEFAULT_FONT_SIZE = 17;
#else
const float DEFAULT_FONT_SIZE = 34;
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

const std::vector<const char*> DEFAULT_INSTANCE_EXTENSIONS = {
#ifndef NDEBUG
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif // NDEBUG
    VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef __linux__
    VK_KHR_DISPLAY_EXTENSION_NAME,
    // https://github.com/nvpro-samples/vk_video_samples/blob/main/common/libs/VkShell/Shell.cpp#L181
    VK_EXT_DIRECT_MODE_DISPLAY_EXTENSION_NAME,
#endif        // __linux__
#if __APPLE__ // molten vk support
    VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
#endif // __APPLE__
};
// required device extensions
const std::vector<const char*> DEFAULT_DEVICE_EXTENSIONS = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,
#if __APPLE__ // molten vk support
    VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME,
#endif // __APPLE__
       // https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_KHR_timeline_semaphore.html
    VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME, // for softare side v-sync
};

// instance extensions required for even-odd rendering
const std::vector<const char*> EVEN_ODD_HARDWARE_INSTANCE_EXTENSIONS = {
// https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_EXT_display_surface_counter.html
#if __linux__
    VK_EXT_DISPLAY_SURFACE_COUNTER_EXTENSION_NAME,
    VK_EXT_ACQUIRE_XLIB_DISPLAY_EXTENSION_NAME,
    // VK_EXT_ACQUIRE_DRM_DISPLAY_EXTENSION_NAME,
    VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
#endif // __LINUX__
};

// device extensions required for even-odd rendering
// only tested on NVIDIA GPUs
const std::vector<const char*> EVEN_ODD_HARDWARE_DEVICE_EXTENSIONS = {
// https://registry.khronos.org/VulkanSC/specs/1.0-extensions/man/html/VK_EXT_display_control.html
#if __linux__
    VK_EXT_DISPLAY_CONTROL_EXTENSION_NAME, // to wake up display
#endif                                     // __linux__
};

const std::vector<const char*> EVEN_ODD_SOFTWARE_DEVICE_EXTENSIONS = {
    // https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkGetRefreshCycleDurationGOOGLE.html
    // https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkGetPastPresentationTimingGOOGLE.html
    // https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkPresentTimesInfoGOOGLE.html
    VK_GOOGLE_DISPLAY_TIMING_EXTENSION_NAME, // for query refresh rate
};

} // namespace Engine

} // namespace DEFAULTS

using INDEX_BUFFER_INDEX_TYPE = unsigned int;

namespace DIRECTORIES
{
const std::string ASSETS = "../assets/";
const std::string SHADERS = "../shaders/";
} // namespace DIRECTORIES
