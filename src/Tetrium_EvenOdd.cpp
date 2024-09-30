// Even-Odd frame rendering implementations
#include "Tetrium.h"

#if __APPLE__
#include "MoltenVK/mvk_private_api.h"

namespace MVK
{
inline uint32_t GetFrameCounter(VkInstance instance, VkDevice device)
{
    // auto func = (PFN_vkGetPerformanceStatisticsMVK
    // )vkGetInstanceProcAddr(instance, "vkGetPerformanceStatisticsMVK");
    // ASSERT(func);

    MVKPerformanceStatistics stats;
    size_t statsSize = sizeof(stats);
    VK_CHECK_RESULT(vkGetPerformanceStatisticsMVK(device, &stats, &statsSize));

    return stats.queue.retrieveCAMetalDrawable.count;
}
} // namespace MVK
#endif // __APPLE__

void Tetrium::initEvenOdd()
{
    switch (_tetraMode) {
    case TetraMode::kEvenOddHardwareSync:
        checkHardwareEvenOddFrameSupport();
        setupHardwareEvenOddFrame();
        break;
    case TetraMode::kEvenOddSoftwareSync:
        checkSoftwareEvenOddFrameSupport();
        setupSoftwareEvenOddFrame();
        break;
    default:
        break;
    }
}

void Tetrium::cleanupEvenOdd()
{
    return; // nothing needs to be cleaned up
}

void Tetrium::setupSoftwareEvenOddFrame()
{
    DEBUG("Setting up even-odd frame resources...");
    auto& ctx = _softwareEvenOddCtx;
    ctx.timeEngineStart = std::chrono::steady_clock::now();

    // get refresh cycle
    VkRefreshCycleDurationGOOGLE refreshCycleDuration;
    ASSERT(_device->logicalDevice);

    PFN_vkGetRefreshCycleDurationGOOGLE ptr = reinterpret_cast<PFN_vkGetRefreshCycleDurationGOOGLE>(
        vkGetInstanceProcAddr(_instance, "vkGetRefreshCycleDurationGOOGLE")
    );
    ASSERT(ptr);
    VK_CHECK_RESULT(ptr(_device->logicalDevice, _swapChain.chain, &refreshCycleDuration));
    ctx.nanoSecondsPerFrame = refreshCycleDuration.refreshDuration;
    ASSERT(ctx.nanoSecondsPerFrame != 0);
}

void Tetrium::checkSoftwareEvenOddFrameSupport() {}

void Tetrium::setupHardwareEvenOddFrame()
{
#if WIN32
    NEEDS_IMPLEMENTATION();
#endif
#if __APPLE__
    NEEDS_IMPLEMENTATION();
#endif
    DEBUG("Setting up even-odd frame resources...");
    auto& ctx = _hardWareEvenOddCtx;
    ctx.vkGetSwapchainCounterEXT = reinterpret_cast<PFN_vkGetSwapchainCounterEXT>(
        vkGetInstanceProcAddr(_instance, "vkGetSwapchainCounterEXT")
    );
    if (ctx.vkGetSwapchainCounterEXT == nullptr) {
        PANIC("Failed to get function pointer to {}", "vkGetSwapchainCounterEXT");
    }
}

// check for hardware and software support for even-odd frame rendering.
// specifically, the GPU must provide a surface counter support --
// the counter ticks every time a vertical blanking period occurs,
// which we use to decide whether the next frame to present should be
// even or odd.
void Tetrium::checkHardwareEvenOddFrameSupport()
{
#ifdef WIN32
    NEEDS_IMPLEMENTATION();
#endif // WIN32
#ifndef __linux__
    return;
#endif // __linux__
    // check instance extensions
    uint32_t numExtensions = 0;
    VkResult result = VK_SUCCESS;

    VK_CHECK_RESULT(vkEnumerateInstanceExtensionProperties(nullptr, &numExtensions, nullptr));
    std::vector<VkExtensionProperties> extensions(numExtensions);
    VK_CHECK_RESULT(
        vkEnumerateInstanceExtensionProperties(nullptr, &numExtensions, extensions.data())
    );

    std::unordered_set<std::string> evenOddExtensions(
        EVEN_ODD_HARDWARE_INSTANCE_EXTENSIONS.begin(), EVEN_ODD_HARDWARE_INSTANCE_EXTENSIONS.end()
    );

    for (VkExtensionProperties& property : extensions) {
        evenOddExtensions.erase(std::string(property.extensionName));
    }

    if (!evenOddExtensions.empty()) {
        ERROR("The following instance extensions required for even-odd rendering is not available: "
        );
        for (const std::string& extension : evenOddExtensions) {
            ERROR(extension);
        }
        PANIC("Even-odd frame not supported!");
    }

    VkSurfaceCapabilities2EXT capabilities{
        .sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_EXT, .pNext = VK_NULL_HANDLE
    };

    auto func = (PFN_vkGetPhysicalDeviceSurfaceCapabilities2EXT
    )vkGetInstanceProcAddr(_instance, "vkGetPhysicalDeviceSurfaceCapabilities2EXT");
    if (!func) {
        PANIC(
            "Failed to find function pointer to {}", "vkGetPhysicalDeviceSurfaceCapabilities2EXT"
        );
    }
    ASSERT(_mainProjectorDisplay.surface && _mainProjectorDisplay.display);
    VK_CHECK_RESULT(func(_device->physicalDevice, _mainProjectorDisplay.surface, &capabilities));

    bool hasVerticalBlankingCounter
        = (capabilities.supportedSurfaceCounters
           & VkSurfaceCounterFlagBitsEXT::VK_SURFACE_COUNTER_VBLANK_BIT_EXT)
          != 0;
    if (!hasVerticalBlankingCounter) {
        PANIC("Even-odd frame not supported!");
    }

    DEBUG("Even-odd frame support check passed!");
}

// whether to use the new virtual frame counter
// the old counter uses a nanosecond-precision timer,
// the new counter takes the max of the image id so far presented.
// TODO: profile precision of the new counter vs. old counter
#define NEW_VIRTUAL_FRAMECOUNTER 1

uint64_t Tetrium::getSurfaceCounterValue()
{
    uint64_t surfaceCounter;
    switch (_tetraMode) {
    case TetraMode::kEvenOddSoftwareSync: {
        return MVK::GetFrameCounter(_instance, _device->Get());
        uint32_t imageCount;
#if NEW_VIRTUAL_FRAMECOUNTER
#if __APPLE__
        vkGetPastPresentationTimingGOOGLE(
            _device->logicalDevice, _swapChain.chain, &imageCount, nullptr
        );
        std::vector<VkPastPresentationTimingGOOGLE> images(imageCount);
        vkGetPastPresentationTimingGOOGLE(
            _device->logicalDevice, _swapChain.chain, &imageCount, images.data()
        );
        auto& ctx = _softwareEvenOddCtx;
        for (int i = 0; i < imageCount; i++) {
            auto& img = images.at(i);
            if (img.presentID > ctx.lastPresentedImageId) {
                ctx.numFramesPresented += 1;
                ctx.lastPresentedImageId = img.presentID;
            }
            _softwareEvenOddCtx.mostRecentPresentFinish
                = std::max(_softwareEvenOddCtx.mostRecentPresentFinish, img.actualPresentTime);
            // INFO(
            //     "i = {}; present id: {} : expected: {} actual: {}",
            //     i,
            //     img.presentID,
            //     img.desiredPresentTime,
            //     img.actualPresentTime
            // );
        }
        surfaceCounter = ctx.numFramesPresented;
#else  // !__APPLE__
        NEEDS_IMPLEMENTATION(); // only apple device supports software counter so far.
#endif // __APPLE__
#else  // ! NEW_VIRTUAL_FRAMECOUNTER
       // old method: count the time
       // return a software-based surface counter
        unsigned long long timeSinceStartNanoSeconds
            = std::chrono::duration<double, std::chrono::nanoseconds::period>(
                  std::chrono::steady_clock().now() - _softwareEvenOddCtx.timeEngineStart
              )
                  .count()
              + _softwareEvenOddCtx.timeOffset;
        surfaceCounter = timeSinceStartNanoSeconds / _softwareEvenOddCtx.nanoSecondsPerFrame;
#endif // NEW_VIRTUAL_FRAMECOUNTER
    } break;
    case TetraMode::kEvenOddHardwareSync:
#if WIN32
        NEEDS_IMPLEMENTATION();
#endif // WIN32
#if __APPLE__
        NEEDS_IMPLEMENTATION();
#endif
#if __linux__
        _hardWareEvenOddCtx.vkGetSwapchainCounterEXT(
            _device->logicalDevice,
            _swapChain.chain,
            VkSurfaceCounterFlagBitsEXT::VK_SURFACE_COUNTER_VBLANK_EXT,
            &surfaceCounter
        );
#endif
        break;
    default:
        surfaceCounter = 0;
    }

    return surfaceCounter;
}

bool Tetrium::isEvenFrame() { return getSurfaceCounterValue() % 2 == 0; }
