// Even-Odd frame rendering implementations
#include "Tetrium.h"

#include <chrono>

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
    ctx.nanoSecondsPerFrame = 16666666; // default config
    ASSERT(ctx.nanoSecondsPerFrame != 0);
}

void Tetrium::checkSoftwareEvenOddFrameSupport() {}

void Tetrium::setupHardwareEvenOddFrame()
{
#if defined(WIN32)
    // TODO: move some stuff to here
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
#if defined(__APPLE__)
    NEEDS_IMPLEMENTATION();
#endif

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
        PANIC("Hardware Even-odd frame not supported!");
    }

#if defined(__LINUX__)
    VkSurfaceCapabilities2EXT capabilities{
        .sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_EXT, .pNext = VK_NULL_HANDLE};

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
#endif

#if defined(WIN32)
    // TODO: implement
#endif // WIN32

    DEBUG("Even-odd frame support check passed!");
}

// whether to use the new virtual frame counter
// the old counter uses a nanosecond-precision timer,
// the new counter takes the max of the image id so far presented.
#define FAKE_SOFTWARE_FRAME_COUNTER 1

void Tetrium::updateSurfaceCounterValue()
{
    uint64_t surfaceCounter;
    switch (_tetraMode) {
    case TetraMode::kEvenOddSoftwareSync: {
        uint32_t imageCount;
#if FAKE_SOFTWARE_FRAME_COUNTER
        surfaceCounter = _numTicks;
#else  // ! FAKE_SOFTWARE_FRAME_COUNTER
       // old method: count the time
       // return a software-based surface counter
        unsigned long long timeSinceStartNanoSeconds
            = std::chrono::duration<double, std::chrono::nanoseconds::period>(
                  std::chrono::steady_clock().now() - _softwareEvenOddCtx.timeEngineStart
              )
                  .count()
              + _softwareEvenOddCtx.timeOffset;
        surfaceCounter = timeSinceStartNanoSeconds / _softwareEvenOddCtx.nanoSecondsPerFrame;
#endif // ! FAKE_SOFTWARE_FRAME_COUNTER
    } break;
    case TetraMode::kEvenOddHardwareSync: {
#if defined(WIN32)
        surfaceCounter = _swapChain.chainDXGI->GetVBlankCount();
#endif // WIN32
#if __APPLE__
        NEEDS_IMPLEMENTATION();
#endif
#if __linux__
        VkResult result = _hardWareEvenOddCtx.vkGetSwapchainCounterEXT(
            _device->logicalDevice,
            _swapChain.chain,
            VkSurfaceCounterFlagBitsEXT::VK_SURFACE_COUNTER_VBLANK_EXT,
            &surfaceCounter
        );

        // Log counter value periodically for debugging
        static uint64_t lastLoggedCounter = 0;
        static int framesSinceLog = 0;
        if (framesSinceLog++ > 60) { // Log every ~60 frames
            if (result != VK_SUCCESS) {
                ERROR("vkGetSwapchainCounterEXT failed with result: {}", (int)result);
            }
            DEBUG(
                "VBlank counter: {} (delta: {})", surfaceCounter, surfaceCounter - lastLoggedCounter
            );
            lastLoggedCounter = surfaceCounter;
            framesSinceLog = 0;
        }
#endif
    } break;
    default:
        surfaceCounter = 0;
    }

    _surfaceCounterValue = surfaceCounter;
}

bool Tetrium::isEvenFrame()
{
    uint64_t surfaceCounterValue = getSurfaceCounterValue();
#if defined(WIN32)
    // TODO: test if this may fix things if we turn it back on
    // offset the surface counter with # of dropped frames,
    // dropping a frame leads to a wrong swapchain offset, here we
    // offset it back.
    // surfaceCounterValue += _swapChain.chainDXGI->GetNumDroppedFrames();
#endif // WIN32
    return surfaceCounterValue % 2 == 0;
}

uint64_t Tetrium::getSurfaceCounterValue()
{
    if (_tetraMode == TetraMode::kEvenOddHardwareSync) {
        return _hardWareEvenOddCtx.expectedNextCounter;
    }
    return _surfaceCounterValue;
}

ColorSpace Tetrium::getCurrentColorSpace()
{
    ColorSpace cs = isEvenFrame() ? ColorSpace::RGB : ColorSpace::OCV;
    if (_flipEvenOdd) {
        cs = cs == ColorSpace::RGB ? ColorSpace::OCV : ColorSpace::RGB;
    }
    return cs;
}

VkResult Tetrium::readVBlankCounter(uint64_t& counter)
{
    counter = 0;
#if __linux__
    VkResult result = _hardWareEvenOddCtx.vkGetSwapchainCounterEXT(
        _device->logicalDevice,
        _swapChain.chain,
        VkSurfaceCounterFlagBitsEXT::VK_SURFACE_COUNTER_VBLANK_EXT,
        &counter
    );
    if (result != VK_SUCCESS) {
        // VK_ERROR_UNKNOWN (-13) is expected before the first vkQueuePresentKHR on NVIDIA —
        // the counter isn't initialized by the driver until after the first present.
        static bool firstFailureLogged = false;
        if (!firstFailureLogged) {
            WARN("vkGetSwapchainCounterEXT returned {} — expected on first frame, will retry", (int)result);
            firstFailureLogged = true;
        }
    }
    return result;
#else
    return VK_SUCCESS;
#endif
}

void Tetrium::waitAndAlignVBlank()
{
    auto& ctx = _hardWareEvenOddCtx;

    uint64_t current = 0;
    if (readVBlankCounter(current) != VK_SUCCESS) {
        return;
    }

    if (ctx.pacingInitialized && current > ctx.expectedNextCounter) {
        uint64_t behind = current - ctx.expectedNextCounter;
        ctx.skippedPresentCount += behind;
        WARN(
            "Vblank pacing: missed {} target vblanks (counter={}, cumulative skips: {})",
            behind,
            current,
            ctx.skippedPresentCount
        );
    }

    ctx.expectedNextCounter = current + 1;
    ctx.pacingInitialized = true;
}
