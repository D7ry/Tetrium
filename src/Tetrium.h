#pragma once
// stl
#include "components/TaskQueue.h"
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <optional>

// vulkan
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

// vq library
#include "lib/VQBuffer.h"
#include "lib/VQDevice.h"

// structs
#include "structs/ImGuiTexture.h"
#include "structs/SharedEngineStructs.h"

// Engine Components
#include "components/Camera.h"
#include "components/DeletionStack.h"
#include "components/DeltaTimer.h"
#include "components/InputManager.h"
#include "components/Profiler.h"
#include "components/TextureManager.h"
#include "components/imgui_widgets/ImGuiWidget.h"
#include "components/SoundManager.h"

#include "components/imgui_widgets/ImGuiWidgetColorTile.h"
#include "components/imgui_widgets/ImGuiWidgetEvenOddCalibration.h"
#include "components/imgui_widgets/ImGuiWidgetTemp.h"
#include "components/imgui_widgets/ImGuiWidgetBlobHunter.h"

// Applications
#include "apps/App.h"

class ImPlotContext;

class Tetrium
{
  private:
    static const std::vector<const char*> DEFAULT_INSTANCE_EXTENSIONS;
    static const std::vector<const char*> DEFAULT_DEVICE_EXTENSIONS;

    static const std::vector<const char*> EVEN_ODD_HARDWARE_INSTANCE_EXTENSIONS;
    static const std::vector<const char*> EVEN_ODD_HARDWARE_DEVICE_EXTENSIONS;
    static const std::vector<const char*> EVEN_ODD_SOFTWARE_DEVICE_EXTENSIONS;

    enum class EngineTexture
    {
        kCursor,
        kCalibrationGraient,
        kNumTextures
    };

    static const std::array<std::string, static_cast<int>(EngineTexture::kNumTextures)> ENGINE_TEXTURE_PATHS;

  public:
    // Display mode to present tetracolor outputs.
    enum class TetraMode
    {
        kEvenOddHardwareSync, // use NVIDIA gpu to hardware sync even-odd frames
        kEvenOddSoftwareSync, // use a timer/frame render callback to software sync even-odd frames
        kDualProjector        // use two projectors and superposition the outputs, not implemented
    };

    // Initialization options
    struct InitOptions
    {
        TetraMode tetraMode = TetraMode::kEvenOddHardwareSync;
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
    };

    void Init(const InitOptions& options);
    void Run();
    void Tick();
    void Cleanup();

    void RegisterApp(TetriumApp::App* app, const std::string& appName);

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
        size_t numImages;
        VkImage depthImage;
        VkDeviceMemory depthImageMemory;
        VkImageView depthImageView;
        VkSurfaceKHR surface;
    };

    // Dedicated display context, used only under `kEvenOddHardwareSync`
    struct DisplayContext
    {
        uint32_t refreshrate;
        VkExtent2D extent = {};
        VkDisplayKHR display = {};
        VkSurfaceKHR surface = {};
        uint32_t planeIndex = {};
    };

    // Synchronization primitives
    struct SyncPrimitives
    {
        VkSemaphore semaImageAvailable;
        VkSemaphore semaRenderFinished;
        VkSemaphore semaImageCopyFinished;
        VkSemaphore semaAppVulkanFinished;
        VkSemaphore semaVsync;
        VkFence fenceInFlight;
        VkFence fenceRenderFinished;
    };

    struct VirtualFrameBuffer
    {
        std::vector<VkFramebuffer> frameBuffer;
        std::vector<VkImage> image;
        std::vector<VkImageView> imageView;
        std::vector<VkDeviceMemory> imageMemory; // memory to hold virtual swap chain
    };

    // Render context for RGV/OCV color space
    // Each color space has its own context
    struct RenderContext
    {
        VkRenderPass renderPass;
        VirtualFrameBuffer virtualFrameBuffer;
    };

    // Context for imgui rendering
    // imgui stays as a struct due to its backend's coupling with Vulkan backend.
    struct ImGuiRenderContext
    {
        VkRenderPass renderPass;
        VkDescriptorPool descriptorPool;
        std::vector<VkFramebuffer> frameBuffer;
        ImGuiContext* backendImGuiContext;
        ImPlotContext* backendImPlotContext;
    };

    enum class ROCVPresentMode{
        kNormal,
        kRGBOnly,
        kOCVOnly
    }

    /* ---------- Windowing ---------- */;
    std::pair<GLFWmonitor*, GLFWvidmode> cliMonitorModeSelection();
    GLFWwindow* initGLFW(bool promptUserForFullScreenWindow);
    [[deprecated("Use selectDisplayXlib")]] void selectDisplayDRM(DisplayContext& ctx);
    void selectDisplayXlib(DisplayContext& ctx);
    void initExclusiveDisplay(DisplayContext& ctx);

    /* ---------- Initialization Subroutines ---------- */
    void initVulkan();
    void initDefaultStates();
    void loadEngineTextures();
    void cleanupEngineTextures();
    VkInstance createInstance();
    void createDevice();
    VkSurfaceKHR createGlfwWindowSurface(GLFWwindow* window);
    void createSynchronizationObjects(std::array<SyncPrimitives, NUM_FRAME_IN_FLIGHT>& primitives);
    VkRenderPass createRenderPass(
        VkDevice logicalDevice,
        VkImageLayout initialLayout,
        VkImageLayout finalLayout,
        VkFormat colorFormat,
        VkAttachmentLoadOp colorLoadOp,
        VkAttachmentStoreOp colorStoreOp,
        bool createDepthAttachment,
        VkSubpassDependency dependency = VkSubpassDependency{
            .srcSubpass = VK_SUBPASS_EXTERNAL, // wait for all previous subpasses
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // what to wait
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // what to not execute
            // we care about src and dst access masks only when there's a potential data race.
            .srcAccessMask = VK_ACCESS_NONE,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
        }
    );

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
    void recreateSwapChain(SwapChainContext& ctx);
    void cleanupSwapChain(SwapChainContext& ctx);
    void createSwapChain(Tetrium::SwapChainContext& ctx, const VkSurfaceKHR surface);
    void createImageViews(SwapChainContext& ctx);
    void createDepthBuffer(SwapChainContext& ctx);
    void createSwapchainFrameBuffers(SwapChainContext& ctx, VkRenderPass rgbOrCnyPass);

    /* ---------- FrameBuffers ---------- */
    void recreateVirtualFrameBuffers();
    void createVirtualFrameBuffer(
        VkRenderPass renderPass,
        const SwapChainContext& swapChain,
        VirtualFrameBuffer& vfb,
        uint32_t numFrameBuffers
    );
    void clearVirtualFrameBuffer(VirtualFrameBuffer& vfb);

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
    void drawFrame(ColorSpace colorSpace, uint8_t frameIdx);
    void drawMainMenu(ColorSpace colorSpace);
    void drawImGui(ColorSpace colorSpace, int currentFrameInFlight);

    void drawAppsImGui(ColorSpace colorSpace, int currentFrameInFlight);

    void getFullScreenViewportAndScissor(
        const SwapChainContext& swapChain,
        VkViewport& viewport,
        VkRect2D& scissor
    );

    void initRYGB2ROCVTransform(InitContext* ctx);
    void cleanupRYGB2ROCVTransform();
    void transformToROCVframeBuffer(
        VirtualFrameBuffer& rgybFrameBuffer,
        SwapChainContext& rocvSwapChain,
        uint32_t frameIdx,
        uint32_t swapChainImageIndex,
        ColorSpace colorSpace,
        vk::CommandBuffer CB,
        bool skip
    );

    /* ---------- Even-Odd frame ---------- */
    void initEvenOdd(); // initialize resources for even-odd rendering
    void cleanupEvenOdd();
    void checkHardwareEvenOddFrameSupport(); // checks hw support for even-odd rendering
    void setupHardwareEvenOddFrame();        // set up resources for even-odd frame
    void checkSoftwareEvenOddFrameSupport(); // checks sw support for even-odd rendering
    void setupSoftwareEvenOddFrame();        // set up resources for software-based even-odd frame
    uint64_t getSurfaceCounterValue(); // get the number of frames requested so far from the display
    bool isEvenFrame();
    ColorSpace getCurrentColorSpace();

    /* ---------- ImGui ---------- */
    void initImGuiRenderContext(Tetrium::ImGuiRenderContext& ctx);
    void destroyImGuiContext(Tetrium::ImGuiRenderContext& ctx);
    void reinitImGuiFrameBuffers(Tetrium::ImGuiRenderContext& ctx);
    void recordImGuiDrawCommandBuffer(
        Tetrium::ImGuiRenderContext& ctx,
        vk::CommandBuffer cb,
        vk::Extent2D extent,
        int swapChainImageIndex,
        ColorSpace colorSpace
    );

    void clearImGuiDrawData();

    /* ---------- Top-level data ---------- */
    VkInstance _instance;
    VkDebugUtilsMessengerEXT _debugMessenger;
    std::shared_ptr<VQDevice> _device;

    SwapChainContext _swapChain;
    ImGuiRenderContext _imguiCtx;

    /* ---------- Prensentation ---------- */
    GLFWwindow* _window;
    DisplayContext _mainProjectorDisplay;

    // ctx for rendering onto the RYGB FB.
    // the FB needs to be transformed into either RGB or OCV format
    RenderContext _renderContextRYGB;

    // render pass that transforms rygb frame buffer in `_renderContextRYGB` into
    // ROCV even-odd representation on the swapchain frame buffer.
    VkRenderPass _rocvTransformRenderPass;

    /* ---------- Synchronization Primivites ---------- */
    std::array<SyncPrimitives, NUM_FRAME_IN_FLIGHT> _syncProjector;

    /* ---------- Render Passes ---------- */
    // main render pass, and currently the only render pass
    std::array<vk::ClearValue, 2> _clearValues; // [color, depthStencil]

    /* ---------- Instance-static Data ---------- */
    TetraMode _tetraMode;

    /* ---------- Tick-dynamic Data ---------- */
    bool _framebufferResized = false;
    uint8_t _currentFrame = 0;

    // whether we are locking the cursor within the glfw window
    bool _windowFocused = false;
    // UI mode: ImGui processes user inputs,
    // and movement / camera inputs disables
    bool _uiMode = true;

    // engine level pause, toggle with P key
    bool _paused = false;

    // static ubo for each frame
    // each buffer stores a `EngineUBOStatic`
    std::array<VQBuffer, NUM_FRAME_IN_FLIGHT> _engineUBOStatic;

    float _FOV = 90;
    double _timeSinceStartSeconds; // seconds in time since engine start, regardless of pause
    unsigned long int _numTicks = 0; // how many ticks has happened so far

    // even-odd frame
    bool _flipEvenOdd = false; // whether to flip even-odd frame

    // context for hardware-based even-odd frame sync
    struct
    {
        PFN_vkGetSwapchainCounterEXT vkGetSwapchainCounterEXT = nullptr;
    } _hardWareEvenOddCtx;

    // context for software-based even-odd frame sync
    struct
    {
        std::chrono::time_point<std::chrono::steady_clock> timeEngineStart;
        uint64_t nanoSecondsPerFrame; // how many nanoseconds in between frames?
                                      // obtained through a precise vulkan API call
        int timeOffset = 0;           // time offset added to the time that's used
                                      // to evaluate current frame, used for the old counter method
    } _softwareEvenOddCtx;

    struct
    {
        uint32_t numDroppedFrames = 0;
        bool currShouldBeEven = true;
    } _evenOddDebugCtx;

    struct {
        bool blackOutEven = false;
        bool blackOutOdd = false;
    } _evenOddRenderingSettings;

    /* ---------- Engine Components ---------- */
    DeletionStack _deletionStack;
    TextureManager _textureManager;
    DeltaTimer _deltaTimer;
    InputManager _inputManager;
    Profiler _profiler;
    TaskQueue _taskQueue;
    SoundManager _soundManager;
    std::unique_ptr<std::vector<Profiler::Entry>> _lastProfilerData = _profiler.NewProfile();

    // ImGui widgets
    friend class ImGuiWidgetDeviceInfo;
    friend class ImGuiWidgetPerfPlot;
    friend class ImGuiWidgetEvenOddCalibration;
    friend class ImGuiWidgetBlobHunter;

    ImGuiWidgetDeviceInfo _widgetDeviceInfo;
    ImGuiWidgetPerfPlot _widgetPerfPlot;
    ImGuiWidgetEvenOddCalibration _widgetEvenOdd;
    ImGuiWidgetColorTile _widgetColorTile;

    ImGuiWidgetBlobHunter _widgetBlobHunter;

    std::unordered_map<std::string, TetriumApp::App*> _appMap;
    std::optional<TetriumApp::App*> _primaryApp = std::nullopt;

    std::array<std::pair<uint32_t, ImGuiTexture>, static_cast<int>(EngineTexture::kNumTextures)> _engineTextures;

    ROCVPresentMode _rocvPresentMode = ROCVPresentMode::kNormal;
};
