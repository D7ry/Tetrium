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

#include "components/imgui_widgets/ImGuiWidgetColorTile.h"
#include "components/imgui_widgets/ImGuiWidgetEvenOddCalibration.h"
#include "components/imgui_widgets/ImGuiWidgetTemp.h"
#include "components/imgui_widgets/ImGuiWidgetTetraViewer.h"
#include "components/imgui_widgets/ImGuiWidgetTetraViewerDemo.h"
#include "components/imgui_widgets/ImGuiWidgetBlobHunter.h"
// ecs
#include "ecs/system/TetraImageDisplaySystem.h"

class ImPlotContext;
class TickContext;

class Tetrium
{
  private:
    static const std::vector<const char*> DEFAULT_INSTANCE_EXTENSIONS;
    static const std::vector<const char*> DEFAULT_DEVICE_EXTENSIONS;

    static const std::vector<const char*> EVEN_ODD_HARDWARE_INSTANCE_EXTENSIONS;
    static const std::vector<const char*> EVEN_ODD_HARDWARE_DEVICE_EXTENSIONS;
    static const std::vector<const char*> EVEN_ODD_SOFTWARE_DEVICE_EXTENSIONS;

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

    struct AnimationKeyFrame
    {
        glm::vec3 position = glm::vec3(0.f);
        glm::vec3 rotation = glm::vec3(0.f); // yaw pitch roll
        glm::vec3 scale = glm::vec3(1.f);    // x y z
    };

    void RegisterAnimatedObject(
        const std::string& meshPath,
        const std::string& texturePath,
        std::unique_ptr<std::vector<AnimationKeyFrame>> keyframes,
        float timeSecondsPerFrame // how long does each frame take before moving on to the next?
    );

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
        VkSemaphore semaVsync;
        VkFence fenceInFlight;
        VkFence fenceRenderFinished;
    };

    // render context for the dual-pass, virtual frame buffer rendering architecture.
    // RGB and OCV channel each have their own render context,
    // they are rendered in parallel for each tick.
    //
    // By the end of rendering, only one channel's render results from the "virtual frame buffer"
    // gets copied to the actual frame buffer, stored in `SwapChainContext::frameBuffer`
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
    struct ImGuiRenderContexts
    {
        VkRenderPass renderPass;
        VkDescriptorPool descriptorPool;
        std::unordered_map<std::string, ImGuiTexture> textures;
        std::vector<VkFramebuffer> frameBuffer;
        ImGuiContext* ctxImGui[ColorSpace::ColorSpaceSize] = {};
        ImPlotContext* ctxImPlot[ColorSpace::ColorSpaceSize] = {};
    };

    /* ---------- Windowing ---------- */;
    std::pair<GLFWmonitor*, GLFWvidmode> cliMonitorModeSelection();
    GLFWwindow* initGLFW(bool promptUserForFullScreenWindow);
    [[deprecated("Use selectDisplayXlib")]] void selectDisplayDRM(DisplayContext& ctx);
    void selectDisplayXlib(DisplayContext& ctx);
    void initExclusiveDisplay(DisplayContext& ctx);

    /* ---------- Initialization Subroutines ---------- */
    void initVulkan();
    void initDefaultStates();
    VkInstance createInstance();
    void createDevice();
    VkSurfaceKHR createGlfwWindowSurface(GLFWwindow* window);
    void createSynchronizationObjects(std::array<SyncPrimitives, NUM_FRAME_IN_FLIGHT>& primitives);
    void createFunnyObjects();
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
    void drawFrame(TickContext* tickData, uint8_t frame);
    void drawImGui(ColorSpace colorSpace);
    void flushEngineUBOStatic(uint8_t frame);
    void getMainProjectionMatrix(glm::mat4& projectionMatrix);

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

    /* ---------- ImGui ---------- */
    void initImGuiRenderContext(Tetrium::ImGuiRenderContexts& ctx);
    void destroyImGuiContext(Tetrium::ImGuiRenderContexts& ctx);
    void reinitImGuiFrameBuffers(Tetrium::ImGuiRenderContexts& ctx);
    void recordImGuiDrawCommandBuffer(
        Tetrium::ImGuiRenderContexts& ctx,
        ColorSpace colorSpace,
        vk::CommandBuffer cb,
        vk::Extent2D extent,
        int swapChainImageIndex
    );
    const ImGuiTexture& getOrLoadImGuiTexture(
        Tetrium::ImGuiRenderContexts& ctx,
        const std::string& texture
    );
    void clearImGuiDrawData();

    /* ---------- Top-level data ---------- */
    VkInstance _instance;
    VkDebugUtilsMessengerEXT _debugMessenger;
    std::shared_ptr<VQDevice> _device;

    SwapChainContext _swapChain;
    ImGuiRenderContexts _imguiCtx;

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
    unsigned long int
        _timeSinceStartNanoSeconds;  // nanoseconds in time since engine start, regardless of pause
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
        uint64_t mostRecentPresentFinish = 0;
        uint32_t lastPresentedImageId = 0; // each image are tagged with an image id,
                                           // image id corresponds to the tick #
                                           // when the images are presented
                                           // tick # and image id are bijective and they
                                           // strictly increase over time
        int vsyncFrameOffset = 0;
        uint64_t numFramesPresented = 0; // total number of frames that have been presented so far
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
    Camera _mainCamera;
    InputManager _inputManager;
    Profiler _profiler;
    TaskQueue _taskQueue;
    std::unique_ptr<std::vector<Profiler::Entry>> _lastProfilerData = _profiler.NewProfile();

    // ImGui widgets
    friend class ImGuiWidgetDeviceInfo;
    friend class ImGuiWidgetPerfPlot;
    friend class ImGuiWidgetUBOViewer;
    friend class ImGuiWidgetEvenOddCalibration;
    friend class ImGuiWidgetGraphicsPipeline;
    friend class ImGuiWidgetTetraViewerDemo;
    friend class ImGuiWidgetTetraViewer;
    friend class ImGuiWidgetTemp;
    friend class ImGuiWidgetBlobHunter;

    ImGuiWidgetDeviceInfo _widgetDeviceInfo;
    ImGuiWidgetPerfPlot _widgetPerfPlot;
    ImGuiWidgetUBOViewer _widgetUBOViewer;
    ImGuiWidgetEvenOddCalibration _widgetEvenOdd;
    ImGuiWidgetGraphicsPipeline _widgetGraphicsPipeline;
    ImGuiWidgetTetraViewerDemo _widgetTetraViewerDemo;
    ImGuiWidgetTetraViewer _widgetTetraViewer;
    ImGuiWidgetColorTile _widgetColorTile;
    ImGuiWidgetTemp _widgetTemp;

    ImGuiWidgetBlobHunter _widgetBlobHunter;


    struct
    {
        [[deprecated]] TetraImageDisplaySystem imageDisplay;
    } _rgbyRenderers;
};
