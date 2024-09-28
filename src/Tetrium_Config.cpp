// Extension setups, and default configurations for the engine

#ifdef __linux__
// clang-format off
#include <X11/extensions/Xrandr.h>
#include "vulkan/vulkan_xlib.h"
#include "vulkan/vulkan_xlib_xrandr.h"
// clang-foramt on
#endif

#include "Tetrium.h"

const std::vector<const char*> Tetrium::DEFAULT_INSTANCE_EXTENSIONS = {
#ifndef NDEBUG
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif // NDEBUG
    VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef __linux__
    VK_KHR_DISPLAY_EXTENSION_NAME,
#endif        // __linux__
#if __APPLE__ // molten vk support
    VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
#endif // __APPLE__
};

const std::vector<const char*> Tetrium::DEFAULT_DEVICE_EXTENSIONS = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,
#if __APPLE__ // molten vk support
    VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME,
#endif // __APPLE__
       // https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_KHR_timeline_semaphore.html
    VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME, // for softare side v-sync
};

const std::vector<const char*> Tetrium::EVEN_ODD_HARDWARE_INSTANCE_EXTENSIONS = {
// https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_EXT_display_surface_counter.html
#if __linux__
    // https://github.com/nvpro-samples/vk_video_samples/blob/main/common/libs/VkShell/Shell.cpp#L181
    VK_EXT_DIRECT_MODE_DISPLAY_EXTENSION_NAME,
    VK_EXT_DISPLAY_SURFACE_COUNTER_EXTENSION_NAME,
    VK_EXT_ACQUIRE_XLIB_DISPLAY_EXTENSION_NAME,
    // VK_EXT_ACQUIRE_DRM_DISPLAY_EXTENSION_NAME,
    VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
#endif // __LINUX__
};

const std::vector<const char*> Tetrium::EVEN_ODD_HARDWARE_DEVICE_EXTENSIONS = {
// https://registry.khronos.org/VulkanSC/specs/1.0-extensions/man/html/VK_EXT_display_control.html
#if __linux__
    VK_EXT_DISPLAY_CONTROL_EXTENSION_NAME, // to wake up display
#endif
};

const std::vector<const char*> Tetrium::EVEN_ODD_SOFTWARE_DEVICE_EXTENSIONS = {
    // https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkGetRefreshCycleDurationGOOGLE.html
    // https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkGetPastPresentationTimingGOOGLE.html
    // https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkPresentTimesInfoGOOGLE.html
    VK_GOOGLE_DISPLAY_TIMING_EXTENSION_NAME, // for query refresh rate
};
