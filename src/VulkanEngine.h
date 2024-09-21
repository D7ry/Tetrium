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
#include "ecs/entity/Entity.h"
#include "ecs/system/SimpleRenderSystem.h"

class TickContext;

class VulkanEngine
{
  private:
    /* ---------- constants ---------- */

    static inline const std::vector<const char*> DEFAULT_INSTANCE_EXTENSIONS = {
#ifndef NDEBUG
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif // NDEBUG
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_DISPLAY_EXTENSION_NAME,
        // https://github.com/nvpro-samples/vk_video_samples/blob/main/common/libs/VkShell/Shell.cpp#L181
        VK_EXT_DIRECT_MODE_DISPLAY_EXTENSION_NAME,
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
    // VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
#endif // __APPLE__
    };
    //  NOTE: appple M3 does not have
    //  `VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME` which moltenVK
    //  requires to enable for metal compatbility. disabling it leads to a
    //  trivial validation layer error that may be safely ignored.

    // instance extensions required for even-odd rendering
    std::unordered_set<std::string> EVEN_ODD_INSTANCE_EXTENSIONS = {
        // https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_EXT_display_surface_counter.html
        VK_EXT_DISPLAY_SURFACE_COUNTER_EXTENSION_NAME,
#if __linux__
        VK_EXT_ACQUIRE_XLIB_DISPLAY_EXTENSION_NAME,
        // VK_EXT_ACQUIRE_DRM_DISPLAY_EXTENSION_NAME,
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
#endif // __LINUX__
    };

    // device extensions required for even-odd rendering
    // only tested on NVIDIA GPUs
    static inline const std::vector<const char*> EVEN_ODD_DEVICE_EXTENSIONS = {
        // https://registry.khronos.org/VulkanSC/specs/1.0-extensions/man/html/VK_EXT_display_control.html
        VK_EXT_DISPLAY_CONTROL_EXTENSION_NAME, // to wake up display
    };

  public:
    struct InitOptions
    {
        bool evenOddMode = true;             // enable even-odd mode
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
        bool flip;                   // a switch that gets flipped every frame
    };

    void Init(const InitOptions& options);
    void Run();
    void Tick();
    void Cleanup();

  private:
    struct QueueFamilyIndices
    {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentationFamily;
    };


    struct SwapChainContext
    {
        VkSwapchainKHR chain = VK_NULL_HANDLE;
        VkFormat imageFormat;
        VkExtent2D extent; // resolution of the swapchain images
        std::vector<VkFramebuffer> frameBuffer;
        std::vector<VkImage> image;
        std::vector<VkImageView> imageView;
        VkImage depthImage;
        VkDeviceMemory depthImageMemory;
        VkImageView depthImageView;
        VkSurfaceKHR surface;
    };

    struct DisplayContext {
        VkExtent2D extent;
        VkDisplayKHR display;
        VkSurfaceKHR surface;
        uint32_t planeIndex;
    };

    /* ---------- Initialization Subroutines ---------- */
    [[deprecated]]
    GLFWmonitor* cliMonitorSelection();
    void initGLFW(const InitOptions& options);

    [[deprecated("Use selectDisplayXlib")]]
    void selectDisplayDRM(DisplayContext& ctx);
    void selectDisplayXlib(DisplayContext& ctx);
    void initExclusiveDisplay(DisplayContext& ctx);
    void initVulkan();
    void createInstance();
    void createGlfwWindowSurface();
    void createDevice();
    void createMainRenderPass(VulkanEngine::SwapChainContext& ctx); // create main render pass
    void createSynchronizationObjects();

    /* ---------- Even-Odd frame ---------- */
    void checkHardwareEvenOddFrameSupport(); // checks hw support for even-odd rendering
    void setUpEvenOddFrame();        // set up resources for even-odd frame

    /* ---------- Physical Device Selection ---------- */
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
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
    void initSwapChain();
    void createSwapChain(VulkanEngine::SwapChainContext& ctx, const VkSurfaceKHR surface);
    void recreateSwapChain(SwapChainContext& ctx);
    void cleanupSwapChain(SwapChainContext& ctx);
    void createImageViews(SwapChainContext& ctx);
    void createDepthBuffer(SwapChainContext& ctx);
    void createFramebuffers(SwapChainContext& ctx);
    bool checkValidationLayerSupport();

    /* ---------- Debug Utilities ---------- */
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
    void bindDefaultInputs();

    /* ---------- Render-Time Functions ---------- */
    void getMainProjectionMatrix(glm::mat4& projectionMatrix);
    void flushEngineUBOStatic(uint8_t frame);
    void drawImGui();
    void drawFrame(TickContext* tickData, uint8_t frame);

    // record command buffer to perform some example GPU
    // operations. currently not used anymore
    void recordCommandBuffer(
        VkCommandBuffer commandBuffer,
        uint32_t imageIndex,
        TickContext* tickData
    );

    /* ---------- Top-level data ---------- */

    VkDebugUtilsMessengerEXT _debugMessenger;

    /* ---------- Prensentation ---------- */

    GLFWwindow* _window;
    VkInstance _instance;

    DisplayContext _mainProjectorDisplay;

    /* ---------- swapchain ---------- */

    SwapChainContext _mainProjectorSwapchain;
    SwapChainContext _controllerSwapchainCtx;

    /* ---------- Synchronization Primivites ---------- */
    struct EngineSynchronizationPrimitives
    {
        VkSemaphore semaImageAvailable;
        VkSemaphore semaRenderFinished;
        VkFence fenceInFlight;
    };

    std::array<EngineSynchronizationPrimitives, NUM_FRAME_IN_FLIGHT> _synchronizationPrimitives;

    /* ---------- Depth Buffer ---------- */

    /* ---------- Render Passes ---------- */
    // main render pass, and currently the only render pass
    VkRenderPass _mainRenderPass = VK_NULL_HANDLE;

    /* ---------- Instance-static Data ---------- */
    bool _evenOddMode = false; // whether to render in even-odd frame mode

    /* ---------- Tick-dynamic Data ---------- */
    bool _framebufferResized = false;
    uint8_t _currentFrame = 0;
    // whether we are locking the cursor within the glfw window
    bool _lockCursor = false;
    // whether we want to draw imgui, set to false disables
    // all imgui windows
    bool _wantToDrawImGui = true;
    // engine level pause, toggle with P key
    bool _paused = false;

    std::shared_ptr<VQDevice> _device;

    // static ubo for each frame
    // each buffer stores a `EngineUBOStatic`
    std::array<VQBuffer, NUM_FRAME_IN_FLIGHT> _engineUBOStatic;

    float _FOV = 90;
    float _timeSinceStartSeconds; // seconds in time since engine start
    unsigned long int _numTicks;  // how many ticks has happened so far

    uint64_t _surfaceCounterValue = 0;
    PFN_vkGetSwapchainCounterEXT _pFNvkGetSwapchainCounterEXT = nullptr;

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

    // ecs Systems
    SimpleRenderSystem _renderer;
};
