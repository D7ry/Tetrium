#pragma once
// stl
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <optional>

// vulkan
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#if __APPLE__
#include <vulkan/vulkan_beta.h> // VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME, for molten-vk support
#endif

#ifdef __linux__
// clang-format off
// direct display utilities
#include "X11/Xlib.h"
#include <X11/extensions/Xrandr.h>
#include "vulkan/vulkan_xlib.h"
#include "vulkan/vulkan_xlib_xrandr.h"
// clang-foramt on
#endif

// vq library
#include "lib/VQBuffer.h"
#include "lib/VQDevice.h"

// structs
#include "structs/SharedEngineStructs.h"

// Engine Components
#include "components/Camera.h"
#include "components/DeletionStack.h"
#include "components/DeltaTimer.h"
#include "components/ImGuiManager.h"
#include "components/InputManager.h"
#include "components/Profiler.h"
#include "components/TextureManager.h"
#include "components/imgui_widgets/ImGuiWidget.h"

// ecs
#include "ecs/system/SimpleRenderSystem.h"

class TickContext;

// TODO: implement even-odd for non-nvidia GPUs
// may be much less accurate but useful for testing

class VulkanEngine
{
  private:
    /* ---------- Extension Configurations ---------- */

    static inline const std::vector<const char*> DEFAULT_INSTANCE_EXTENSIONS = {
#ifndef NDEBUG
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif // NDEBUG
        VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef __linux__
        VK_KHR_DISPLAY_EXTENSION_NAME,
        // https://github.com/nvpro-samples/vk_video_samples/blob/main/common/libs/VkShell/Shell.cpp#L181
        VK_EXT_DIRECT_MODE_DISPLAY_EXTENSION_NAME,
#endif // __linux__
#if __APPLE__ // molten vk support
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
#endif // __APPLE__
    };
    // required device extensions
    static inline const std::vector<const char*> DEFAULT_DEVICE_EXTENSIONS = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,
        //https://forums.developer.nvidia.com/t/vk-khr-display-swapchain-not-present-on-linux/70781
        // VK_KHR_DISPLAY_SWAPCHAIN_EXTENSION_NAME, // NOTE: we don't need the display-swapchain extension,
                                                    // instead use VK_EXT_ACQUIRE_XLIB_DISPLAY_EXTENSION
#if __APPLE__ // molten vk support
        VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME,
#endif // __APPLE__
       //https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkGetRefreshCycleDurationGOOGLE.html
       //https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkGetPastPresentationTimingGOOGLE.html
       //https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkPresentTimesInfoGOOGLE.html
        VK_GOOGLE_DISPLAY_TIMING_EXTENSION_NAME, // for query refresh rate
        // https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_KHR_timeline_semaphore.html
        VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME, // for softare side v-sync
    };

    // instance extensions required for even-odd rendering
    std::unordered_set<std::string> EVEN_ODD_HARDWARE_INSTANCE_EXTENSIONS = {
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
    static inline const std::vector<const char*> EVEN_ODD_HARDWARE_DEVICE_EXTENSIONS = {
        // https://registry.khronos.org/VulkanSC/specs/1.0-extensions/man/html/VK_EXT_display_control.html
#if __linux__
        VK_EXT_DISPLAY_CONTROL_EXTENSION_NAME, // to wake up display
#endif // __linux__
    };

  public:

    enum class TetraMode
    {
        kEvenOddHardwareSync, // use NVIDIA gpu to hardware sync even-odd frames
        kEvenOddSoftwareSync, // use a timer to software sync even-odd frames
        kDualProjector // use two projetors, not implemented
    };

    struct InitOptions
    {
        TetraMode tetraMode = TetraMode::kEvenOddHardwareSync;
        bool fullScreen = false;             // full screen mode
        bool manualMonitorSelection = false; // the user may select a monitor that's not the primary
                                             // monitor through CLI
    };

    // Engine-wide static UBO that gets updated every Tick()
    // systems can read from `InitData::engineUBOStaticDescriptorBufferInfo`
    // to bind to the UBO in their own graphics pipelines.
    struct EngineUBOStatic
    {
        glm::mat4 view;              // view matrix
        glm::mat4 proj;              // proj matrix
        float timeSinceStartSeconds; // time in seconds since engine start
        float sinWave;               // a number interpolating between [0,1]
        bool isEvenFrame;                   // a switch that gets flipped every frame
    };

    void Init(const InitOptions& options);
    void Run();
    void Tick();
    void Cleanup();

  private:
    /* ---------- Packed Structs ---------- */
    // context for a single swapchain; 
    // each window & display manages their separate
    // swapchain context
    struct SwapChainContext
    {
        VkSwapchainKHR chain = VK_NULL_HANDLE;
        VkFormat imageFormat;
        VkExtent2D extent; // resolution of the swapchain images
        std::vector<VkImage> image;
        std::vector<VkImageView> imageView;
        std::vector<VkFramebuffer> frameBuffer;
        VkImage depthImage;
        VkDeviceMemory depthImageMemory;
        VkImageView depthImageView;
        VkSurfaceKHR surface;
    };

    // context for a dedicated display
    struct DisplayContext {
        uint32_t refreshrate;
        VkExtent2D extent = {};
        VkDisplayKHR display = {};
        VkSurfaceKHR surface = {};
        uint32_t planeIndex = {};
    };

    struct SyncPrimitives
    {
        VkSemaphore semaImageAvailable;
        VkSemaphore semaRenderFinished;
        VkSemaphore semaImageCopyFinished;
        VkSemaphore semaVsync;
        VkFence fenceInFlight;
    };

    // render context for the dual-pass, virtual frame buffer rendering architecture.
    // RGB and CMY channel each have their own render context,
    // they are rendered in parallel for each tick; both's render results are written onto `RenderContext::virtualFrameBuffer`,
    // associated with `RenderContext::imageView`, `RenderContext::imageMemory`, and `RenderContext::image`
    //
    // By the end of rendering, only one channel's render results from the "virtual frame buffer" gets
    // copied to the actual frame buffer, stored in `SwapChainContext::frameBuffer`
    struct RenderContext {
        VkRenderPass renderPass;
        SwapChainContext* swapchain; // global swap chain
        std::vector<VkFramebuffer> virtualFrameBuffer;
        std::vector<VkImage> image;
        std::vector<VkImageView> imageView;
        std::vector<VkDeviceMemory> imageMemory; // memory to hold virtual swap chain
    };

    SwapChainContext _swapChain;

    /* ---------- Initialization Subroutines ---------- */
    [[deprecated]]
    GLFWmonitor* cliMonitorSelection();
    void initGLFW(const InitOptions& options);

    [[deprecated("Use selectDisplayXlib")]]
    void selectDisplayDRM(DisplayContext& ctx);
    void selectDisplayXlib(DisplayContext& ctx);
    void initExclusiveDisplay(DisplayContext& ctx);
    void initVulkan();
    void initDefaultStates();
    void createInstance();
    void createGlfwWindowSurface();
    void createDevice();
    VkSurfaceKHR createGlfwWindowSurface(GLFWwindow* window);
    vk::RenderPass createRenderPass(const VkFormat imageFormat); // create main render pass
    void createSynchronizationObjects(std::array<SyncPrimitives, NUM_FRAME_IN_FLIGHT>& primitives);
    void createFunnyObjects();


    /* ---------- Physical Device Selection ---------- */
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    bool isDeviceSuitable(VkPhysicalDevice device);
    VkPhysicalDevice pickPhysicalDevice();
    const std::vector<const char*> getRequiredDeviceExtensions() const;

    /* ---------- Swapchain ---------- */
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(
        const std::vector<VkSurfaceFormatKHR>& availableFormats
    );
    VkPresentModeKHR chooseSwapPresentMode(
        const std::vector<VkPresentModeKHR>& availablePresentModes
    );

    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    void createSwapChain(VulkanEngine::SwapChainContext& ctx, const VkSurfaceKHR surface);
    void recreateSwapChain(SwapChainContext& ctx);
    void cleanupSwapChain(SwapChainContext& ctx);
    void createImageViews(SwapChainContext& ctx);
    void createDepthBuffer(SwapChainContext& ctx);
    void createSwapchainFrameBuffers(SwapChainContext& ctx, VkRenderPass rgbOrCnyPass);

    /* ---------- FrameBuffers ---------- */
    void recreateFrameBuffers(RenderContext& ctx);
    void createFramebuffers(RenderContext& ctx);

    /* ---------- Debug Utilities ---------- */
    bool checkValidationLayerSupport();
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
    void setupDebugMessenger(); // unused for now
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData
    );

    /* ---------- Input ---------- */
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
    void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
    void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    void bindDefaultInputs();

    /* ---------- Render-Time Functions ---------- */
    void drawFrame(TickContext* tickData, uint8_t frame);
    void drawImGui(ColorSpace colorSpace);
    void flushEngineUBOStatic(uint8_t frame);
    void getMainProjectionMatrix(glm::mat4& projectionMatrix);

    /* ---------- Even-Odd frame ---------- */
    void checkHardwareEvenOddFrameSupport(); // checks hw support for even-odd rendering
    void setupHardwareEvenOddFrame();        // set up resources for even-odd frame
    void checkSoftwareEvenOddFrameSupport(); // checks sw support for even-odd rendering
    void setupSoftwareEvenOddFrame();        // set up resources for software-based even-odd frame
    uint64_t getSurfaceCounterValue(); // get the number of frames requested so far from the display
    bool isEvenFrame();
    void setupEvenOddRenderContext(); // set up `_evenOddRenderContexts`

    /* ---------- Top-level data ---------- */
    VkInstance _instance;
    VkDebugUtilsMessengerEXT _debugMessenger;
    std::shared_ptr<VQDevice> _device;

    /* ---------- Prensentation ---------- */
    GLFWwindow* _window;
    DisplayContext _mainProjectorDisplay;

    /* ---------- swapchain ---------- */
    SwapChainContext _mainWindowSwapChain;
    SwapChainContext _auxWindowSwapchain; // unused for now


    struct {
        RenderContext RGB; // red, green, blue
        RenderContext CMY; // cyan, magenta, yellow
    } _renderContexts;

    /* ---------- Synchronization Primivites ---------- */
    std::array<SyncPrimitives, NUM_FRAME_IN_FLIGHT> _syncProjector;

    /* ---------- Render Passes ---------- */
    // main render pass, and currently the only render pass
    VkRenderPass _mainRenderPass = VK_NULL_HANDLE;
    std::array<vk::ClearValue, 2> _clearValues; // [color, depthStencil]

    /* ---------- Instance-static Data ---------- */
    TetraMode _tetraMode;

    /* ---------- Tick-dynamic Data ---------- */
    bool _framebufferResized = false;
    uint8_t _currentFrame = 0;

    // whether we are locking the cursor within the glfw window
    bool _lockCursor = false;
    // UI mode: ImGui processes user inputs,
    // and movement / camera inputs disables
    bool _uiMode = true;

    // whether we want to draw imgui, set to false disables
    // all imgui windows
    bool _wantToDrawImGui = true;
    // engine level pause, toggle with P key
    bool _paused = false;


    // static ubo for each frame
    // each buffer stores a `EngineUBOStatic`
    std::array<VQBuffer, NUM_FRAME_IN_FLIGHT> _engineUBOStatic;

    float _FOV = 90;
    double _timeSinceStartSeconds; // seconds in time since engine start, regardless of pause
    unsigned long int _timeSinceStartNanoSeconds; // nanoseconds in time since engine start, regardless of pause
    unsigned long int _numTicks=0;  // how many ticks has happened so far

    // even-odd frame
    bool _flipEvenOdd = false; // whether to flip even-odd frame

    // context for hardware-based even-odd frame sync
    struct {
        PFN_vkGetSwapchainCounterEXT vkGetSwapchainCounterEXT = nullptr;
    } _hardWareEvenOddCtx;

    // context for software-based even-odd frame sync
    struct {
        std::chrono::time_point<std::chrono::steady_clock> timeEngineStart;
        uint64_t nanoSecondsPerFrame; // how many nanoseconds in between frames?
                                      // obtained through a precise vulkan API call
        int timeOffset = 0; // time offset added to the time that's used
                             // to evaluate current frame, used for the old counter method
        uint64_t mostRecentPresentFinish = 0;
        uint32_t lastPresentedImageId = 0; // each image are tagged with an image id,
                                           // image id corresponds to the tick # 
                                           // when the images are presented
                                           // tick # and image id are bijective and they
                                           // strictly increase over time
        int vsyncFrameOffset = 0;
    } _softwareEvenOddCtx;

    /* ---------- Engine Components ---------- */
    DeletionStack _deletionStack;
    TextureManager _textureManager;
    ImGuiManager _imguiManager;
    DeltaTimer _deltaTimer;
    Camera _mainCamera;
    InputManager _inputManager;
    Profiler _profiler;
    std::unique_ptr<std::vector<Profiler::Entry>> _lastProfilerData = _profiler.NewProfile();

    // ImGui widgets
    friend class ImGuiWidgetDeviceInfo;
    ImGuiWidgetDeviceInfo _widgetDeviceInfo;
    friend class ImGuiWidgetPerfPlot;
    ImGuiWidgetPerfPlot _widgetPerfPlot;
    friend class ImGuiWidgetUBOViewer;
    ImGuiWidgetUBOViewer _widgetUBOViewer;
    friend class ImGuiWidgetEvenOdd;
    ImGuiWidgetEvenOdd _widgetEvenOdd;
    friend class ImGuiWidgetGraphicsPipeline;
    ImGuiWidgetGraphicsPipeline _widgetGraphicsPipeline;

    SimpleRenderSystem _renderer;

};
