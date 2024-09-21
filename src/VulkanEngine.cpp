#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <set>

// graphics libraries
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>

// imgui
#include "imgui.h"

// Molten VK Config
#if __APPLE__
#include "MoltenVKConfig.h"
#endif // __APPLE__

// ecs
#include "ecs/component/TransformComponent.h"

#include "VulkanEngine.h"

#include <fcntl.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

// in CLI pop up a monitor selection interface, that lists
// monitor names and properties
// the user would input a number to select the right monitor.
GLFWmonitor* VulkanEngine::cliMonitorSelection()
{
    const char* line = nullptr;
    line = "---------- Please Select Monitor Index ----------"; // lol
    std::cout << line << std::endl;
    int numMonitors;
    GLFWmonitor** monitors = glfwGetMonitors(&numMonitors);
    // print out monitor details
    for (int monitorIdx = 0; monitorIdx < numMonitors; monitorIdx++) {
        GLFWmonitor* monitor = monitors[monitorIdx];
        const char* name = glfwGetMonitorName(monitor);
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        fmt::println( // print out monitor's detailed infos
            "{}: {} {} x {}, {} Hz",
            monitorIdx,
            name,
            mode->width,
            mode->height,
            mode->refreshRate
        );
    }
    line = "-------------------------------------------------";
    std::cout << line << std::endl;
    // scan user input for monitor idx and choose monitor
    int monitorIdx = 0;
    do {
        std::string input;
        getline(std::cin, input);
        char* endPtr;
        monitorIdx = strtol(input.c_str(), &endPtr, 10);
        if (endPtr) { // conversion success
            if (monitorIdx < numMonitors) {
                break;
            } else {
                std::cout << "Monitor index out of range!" << std::endl;
            }
        } else {
            std::cout << "Please input a valid integer number!" << std::endl;
        }
    } while (1);
    return monitors[monitorIdx];
}

// select exclusive display using DRM
void VulkanEngine::selectDisplayDRM(DisplayContext& ctx)
{
    int drmFd = open("/dev/dri/card0", O_RDWR);
    if (drmFd < 0) {
        PANIC("Failed to open DRM device");
    }

    ASSERT(drmSetMaster(drmFd) == 0);

    drmModeRes* resources = drmModeGetResources(drmFd);
    if (!resources) {
        PANIC("Failed to get DRM resources");
    }

    printf("Available displays:\n");
    for (int i = 0; i < resources->count_connectors; i++) {
        drmModeConnector* connector = drmModeGetConnector(drmFd, resources->connectors[i]);
        if (connector) {
            printf(
                "%d: (ID: %d, Status: %s",
                i + 1,
                connector->connector_id,
                (connector->connection == DRM_MODE_CONNECTED) ? "Connected" : "Disconnected"
            );

            if (connector->connection == DRM_MODE_CONNECTED && connector->count_modes > 0) {
                drmModeModeInfo mode = connector->modes[0]; // Using the first mode
                printf(", Size: %dx%d", mode.hdisplay, mode.vdisplay);
            }
            printf(")\n");

            drmModeFreeConnector(connector);
        }
    }

    int choice;
    do {
        printf("Enter the number of the display you want to use: ");
        if (scanf("%d", &choice) != 1 || choice < 1 || choice > resources->count_connectors) {
            printf("Invalid input. Please try again.\n");
            while (getchar() != '\n')
                ; // Clear input buffer
        } else {
            break;
        }
    } while (1);

    drmModeConnector* selectedConnector
        = drmModeGetConnector(drmFd, resources->connectors[choice - 1]);
    if (!selectedConnector) {
        PANIC("Failed to get selected connector");
        drmModeFreeResources(resources);
        close(drmFd);
    }

    uint32_t connectorId = selectedConnector->connector_id;
    printf("Selected display: (ID: %d", connectorId);

    if (selectedConnector->connection == DRM_MODE_CONNECTED && selectedConnector->count_modes > 0) {
        drmModeModeInfo mode = selectedConnector->modes[0]; // Using the first mode
        printf(", Size: %dx%d", mode.hdisplay, mode.vdisplay);
    }
    printf(")\n");

    // get the VkDisplayKHR object from drm display
    PFN_vkGetDrmDisplayEXT fnPtr = reinterpret_cast<PFN_vkGetDrmDisplayEXT>(
        vkGetInstanceProcAddr(_instance, "vkGetDrmDisplayEXT")
    );
    ASSERT(fnPtr);
    VK_CHECK_RESULT(fnPtr(_device->physicalDevice, drmFd, connectorId, &ctx.display));
    ASSERT(ctx.display);

    PFN_vkAcquireDrmDisplayEXT fnPtr2 = reinterpret_cast<PFN_vkAcquireDrmDisplayEXT>(
        vkGetInstanceProcAddr(_instance, "vkAcquireDrmDisplayEXT")
    );
    ASSERT(fnPtr2);
    VK_CHECK_RESULT(fnPtr2(_device->physicalDevice, drmFd, ctx.display));

    drmModeFreeConnector(selectedConnector);
    drmModeFreeResources(resources);
    close(drmFd);
}

// select exclusive display using xlib
void VulkanEngine::selectDisplayXlib(DisplayContext& ctx)
{
    auto device = _device->physicalDevice;
    // Get the X11 display name for the selected Vulkan display
    PFN_vkGetRandROutputDisplayEXT vkGetRandROutputDisplayEXT
        = reinterpret_cast<PFN_vkGetRandROutputDisplayEXT>(
            vkGetInstanceProcAddr(_instance, "vkGetRandROutputDisplayEXT")
        );
    ASSERT(vkGetRandROutputDisplayEXT);

    Display* xDisplay = XOpenDisplay(NULL);
    ASSERT(xDisplay);

    int screenCount = ScreenCount(xDisplay);
    std::vector<VkDisplayKHR> displays;
    std::vector<std::string> displayNames;
    for (int i = 0; i < screenCount; ++i) {
        Screen* screen = ScreenOfDisplay(xDisplay, i);
        Window root = RootWindowOfScreen(screen);
        XRRScreenResources* resources = XRRGetScreenResources(xDisplay, root);

        for (int j = 0; j < resources->noutput; ++j) {
            RROutput output = resources->outputs[j];
            VkDisplayKHR display;
            VkResult result = vkGetRandROutputDisplayEXT(device, xDisplay, output, &display);
            if (result == VK_SUCCESS) {
                // display display properties
                displays.push_back(display);
                XRROutputInfo* outputInfo = XRRGetOutputInfo(xDisplay, resources, output);
                displayNames.push_back(outputInfo->name);
                XRRFreeOutputInfo(outputInfo);
            }
        }

        XRRFreeScreenResources(resources);
    }

    for (size_t i = 0; i < displays.size(); ++i) {
        VkDisplayPropertiesKHR displayProperties;

        std::cout << "Display " << i << ":\n";
        std::cout << "  Name: " << displayNames[i] << "\n";
    }

    // Prompt user for selection
    size_t selectedIndex = 0; // defaults to 1 for vulkan configurator
    do {
        std::cout << "Enter the index of the display you want to use (0-" << displays.size() - 1
                  << "): ";
        std::cin >> selectedIndex;
    } while (selectedIndex >= displays.size());

    ctx.display = displays[selectedIndex];

    DEBUG("Acquiring exclusive access to display...");
    // Acquire the display
    PFN_vkAcquireXlibDisplayEXT vkAcquireXlibDisplayEXT
        = reinterpret_cast<PFN_vkAcquireXlibDisplayEXT>(
            vkGetInstanceProcAddr(_instance, "vkAcquireXlibDisplayEXT")
        );
    ASSERT(vkAcquireXlibDisplayEXT);

    VK_CHECK_RESULT(vkAcquireXlibDisplayEXT(device, xDisplay, ctx.display));
}

// take complete control over a physical display
// the display must be directly connected to the GPU through x11
// prompts the user to select the display and resolution in an ImGui window
void VulkanEngine::initExclusiveDisplay(VulkanEngine::DisplayContext& ctx)
{
    selectDisplayXlib(ctx);

    auto device = _device->physicalDevice;
    uint32_t modeCount = 0;
    vkGetDisplayModePropertiesKHR(device, ctx.display, &modeCount, nullptr);
    std::vector<VkDisplayModePropertiesKHR> modeProperties(modeCount);
    vkGetDisplayModePropertiesKHR(device, ctx.display, &modeCount, modeProperties.data());

    // List all modes for the selected display
    std::cout << "Available Modes for selected display:\n";
    for (uint32_t i = 0; i < modeCount; ++i) {
        std::cout << "Index: " << i
                  << ", Resolution: " << modeProperties[i].parameters.visibleRegion.width << "x"
                  << modeProperties[i].parameters.visibleRegion.height
                  << ", Refresh Rate: " << modeProperties[i].parameters.refreshRate << " Hz"
                  << std::endl;
    }

    // Get user input for mode selection
    uint32_t selectedModeIndex = 0;
    do {
        std::cout << "Enter the index of the mode you want to use (0-" << modeCount - 1 << "): ";
        std::cin >> selectedModeIndex;
    } while (selectedModeIndex >= modeCount);

    VkDisplayModeKHR displayMode = modeProperties[selectedModeIndex].displayMode;

    // Find a compatible plane
    bool foundPlaneIndex = false;
    uint32_t planeCount = 0;
    uint32_t stackIndex = 0;
    vkGetPhysicalDeviceDisplayPlanePropertiesKHR(device, &planeCount, nullptr);
    std::vector<VkDisplayPlanePropertiesKHR> planeProperties(planeCount);
    vkGetPhysicalDeviceDisplayPlanePropertiesKHR(device, &planeCount, planeProperties.data());

    for (uint32_t i = 0; i < planeCount; i++) {
        uint32_t displayCount = 0;
        const VkDisplayPlanePropertiesKHR& planeProperty = planeProperties[i];
        vkGetDisplayPlaneSupportedDisplaysKHR(device, i, &displayCount, nullptr);
        if (displayCount > 0) {
            std::vector<VkDisplayKHR> displays(displayCount);
            vkGetDisplayPlaneSupportedDisplaysKHR(device, i, &displayCount, displays.data());
            if (std::find(displays.begin(), displays.end(), ctx.display) != displays.end()) {
                ctx.planeIndex = i;
                stackIndex = planeProperty.currentStackIndex;
                foundPlaneIndex = true;
                break;
            }
        }
    }
    ASSERT(foundPlaneIndex);
    DEBUG("plane index: {}; stack index: {}", ctx.planeIndex, stackIndex);

    // Create display surface
    VkDisplaySurfaceCreateInfoKHR surfaceCreateInfo = {};
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.displayMode = displayMode;
    surfaceCreateInfo.planeIndex = ctx.planeIndex;
    surfaceCreateInfo.planeStackIndex = stackIndex;
    surfaceCreateInfo.transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    surfaceCreateInfo.globalAlpha = 1.0f;
    surfaceCreateInfo.alphaMode = VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR;
    surfaceCreateInfo.imageExtent = modeProperties[selectedModeIndex].parameters.visibleRegion;

    VK_CHECK_RESULT(
        vkCreateDisplayPlaneSurfaceKHR(_instance, &surfaceCreateInfo, nullptr, &ctx.surface)
    );
    ASSERT(ctx.surface != VK_NULL_HANDLE);

    _deletionStack.push([this, ctx]() { vkDestroySurfaceKHR(_instance, ctx.surface, nullptr); });

    // Store the display properties for later use
    ctx.extent = modeProperties[selectedModeIndex].parameters.visibleRegion;
    DEBUG("display extent: {} {}", ctx.extent.width, ctx.extent.height);
}

void VulkanEngine::initGLFW(const InitOptions& options)
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    if (!glfwVulkanSupported()) {
        FATAL("Vulkan is not supported on this machine!");
    }
    size_t width = DEFAULTS::WINDOW_WIDTH;
    size_t height = DEFAULTS::WINDOW_HEIGHT;
    // having monitor as nullptr initializes a windowed window
    GLFWmonitor* monitor = nullptr;
    if (options.fullScreen) {
        NEEDS_IMPLEMENTATION();
        monitor = glfwGetPrimaryMonitor();
        if (options.manualMonitorSelection) {
            monitor = cliMonitorSelection();
        }
        fmt::println("Selected {} as full-screen monitor.", glfwGetMonitorName(monitor));
        // update width and height with monitor's form factor
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        glfwWindowHint(GLFW_RED_BITS, mode->redBits);
        glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
        glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
        glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
        width = mode->width;
        height = mode->height;
#if __APPLE__
        const int MACOS_SCALING_FACTOR = 2;
        if (monitor == glfwGetPrimaryMonitor()) {
            width *= MACOS_SCALING_FACTOR;
            height *= MACOS_SCALING_FACTOR;
        }
#endif // __APPLE__
    }

    this->_window = glfwCreateWindow(width, height, DEFAULTS::Engine::APPLICATION_NAME, monitor, nullptr);
    if (this->_window == nullptr) {
        FATAL("Failed to initialize GLFW windlw!");
    }
    glfwSetWindowUserPointer(_window, this);
    _deletionStack.push([this]() {
        glfwDestroyWindow(this->_window);
        glfwTerminate();
    });
}

void VulkanEngine::cursorPosCallback(GLFWwindow* window, double xpos, double ypos)
{
    static bool updatedCursor = false;
    static int prevX = -1;
    static int prevY = -1;
    if (!updatedCursor) {
        prevX = xpos;
        prevY = ypos;
        updatedCursor = true;
    }
    double deltaX = prevX - xpos;
    double deltaY = prevY - ypos;
    // handle camera movement
    deltaX *= 0.3;
    deltaY *= 0.3; // make movement slower
    if (_lockCursor) {
        _mainCamera.ModRotation(deltaX, deltaY, 0);
    }
    prevX = xpos;
    prevY = ypos;
}

void VulkanEngine::Init(const VulkanEngine::InitOptions& options)
{
    // populate static config fields
    _evenOddMode = options.evenOddMode;
    if (!_evenOddMode) {
        // none-even odd needs dual-display + dual-fb implementation
        NEEDS_IMPLEMENTATION();
    }
#if __APPLE__
    MoltenVKConfig::Setup();
#endif // __APPLE__
    initGLFW(options);
    { // Input Handling
        auto keyCallback = [](GLFWwindow* window, int key, int scancode, int action, int mods) {
            VulkanEngine* pThis = reinterpret_cast<VulkanEngine*>(glfwGetWindowUserPointer(window));
            if (key == GLFW_KEY_P && action == GLFW_PRESS) {
                pThis->_paused = !pThis->_paused;
            }
            pThis->_inputManager.OnKeyInput(window, key, scancode, action, mods);
        };
        glfwSetKeyCallback(this->_window, keyCallback);
        // don't have a mouse input manager yet, so manually bind cursor pos
        // callback
        auto cursorPosCallback = [](GLFWwindow* window, double xpos, double ypos) {
            VulkanEngine* pThis = reinterpret_cast<VulkanEngine*>(glfwGetWindowUserPointer(window));
            pThis->cursorPosCallback(window, xpos, ypos);
        };
        glfwSetCursorPosCallback(this->_window, cursorPosCallback);
        bindDefaultInputs();
    }

    // frame buffer never resizes, so no need for callback
    // glfwSetFramebufferSizeCallback(_window, this->framebufferResizeCallback);
    this->initVulkan();
    _textureManager.Init(_device);
    this->_deletionStack.push([this]() { _textureManager.Cleanup(); });

    // create static engine ubo
    {
        for (VQBuffer& engineUBO : _engineUBOStatic)
            _device->CreateBufferInPlace(
                sizeof(EngineUBOStatic),
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                engineUBO
            );
        _deletionStack.push([this]() {
            for (VQBuffer& engineUBO : _engineUBOStatic)
                engineUBO.Cleanup();
        });
    }

    InitContext initCtx;
    { // populate initData
        initCtx.device = this->_device.get();
        initCtx.textureManager = &_textureManager;
        initCtx.swapChainImageFormat = this->_mainProjectorSwapchain.imageFormat;
        initCtx.renderPass.mainPass = _mainRenderPass;
        for (int i = 0; i < _engineUBOStatic.size(); i++) {
            initCtx.engineUBOStaticDescriptorBufferInfo[i].range = sizeof(EngineUBOStatic);
            initCtx.engineUBOStaticDescriptorBufferInfo[i].buffer = _engineUBOStatic[i].buffer;
            initCtx.engineUBOStaticDescriptorBufferInfo[i].offset = 0;
        }
    }

    _renderer.Init(&initCtx);
    _deletionStack.push([this]() { _renderer.Cleanup(); });
    // create example mesh
    {
        // lil cow
        Entity* spot = new Entity("Spot");
        auto meshInstance = _renderer.MakeMeshInstanceComponent(
            DIRECTORIES::ASSETS + "models/spot.obj", DIRECTORIES::ASSETS + "textures/spot.png"
        );
        // give the lil cow a mesh
        spot->AddComponent(meshInstance);
        // give the lil cow a transform
        spot->AddComponent(new TransformComponent());
        // register lil cow
        _renderer.AddEntity(spot);
    }
}

void VulkanEngine::Run()
{
    while (!glfwWindowShouldClose(_window)) {
        glfwPollEvents();
        Tick();
    }
}

void VulkanEngine::Tick()
{
    _deltaTimer.Tick(); // tick deltaTimer regardless of pause,
                        // for correct _timeSinceStartSeconds
    if (_paused) {
        std::this_thread::yield();
        return;
    }
    {
        PROFILE_SCOPE(&_profiler, "Main Tick");
        {
            PROFILE_SCOPE(&_profiler, "CPU");
            // CPU-exclusive workloads
            double deltaTime = _deltaTimer.GetDeltaTime();
            _timeSinceStartSeconds += deltaTime;
            _inputManager.Tick(deltaTime);
            TickContext tickData{&_mainCamera, deltaTime};
            tickData.profiler = &_profiler;
            drawImGui();
            flushEngineUBOStatic(_currentFrame);
            drawFrame(&tickData, _currentFrame);
            _currentFrame = (_currentFrame + 1) % NUM_FRAME_IN_FLIGHT;
        }
        {
            PROFILE_SCOPE(&_profiler, "GPU");
            vkDeviceWaitIdle(this->_device->logicalDevice);
        }
    }
    _lastProfilerData = _profiler.NewProfile();
    _numTicks++;
}

void VulkanEngine::framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    DEBUG("Window resized to {}x{}", width, height);
    auto app = reinterpret_cast<VulkanEngine*>(glfwGetWindowUserPointer(window));
    app->_framebufferResized = true;
}

void VulkanEngine::createDevice()
{
    VkPhysicalDevice physicalDevice = this->pickPhysicalDevice();
    this->_device = std::make_shared<VQDevice>(physicalDevice);
    this->_deletionStack.push([this]() { this->_device->Cleanup(); });
}

void VulkanEngine::setUpEvenOddFrame()
{
    _pFNvkGetSwapchainCounterEXT = reinterpret_cast<PFN_vkGetSwapchainCounterEXT>(
        vkGetInstanceProcAddr(_instance, "vkGetSwapchainCounterEXT")
    );
    if (_pFNvkGetSwapchainCounterEXT == nullptr) {
        PANIC("Failed to get function pointer to {}", "vkGetSwapchainCounterEXT");
    }
}

// check for hardware and software support for even-odd frame rendering.
// specifically, the GPU must provide a surface counter support --
// the counter ticks every time a vertical blanking period occurs,
// which we use to decide whether the next frame to present should be
// even or odd.
void VulkanEngine::checkHardwareEvenOddFrameSupport()
{
    // check instance extensions
    uint32_t numExtensions = 0;
    VkResult result = VK_SUCCESS;

    VK_CHECK_RESULT(vkEnumerateInstanceExtensionProperties(nullptr, &numExtensions, nullptr));
    std::vector<VkExtensionProperties> extensions(numExtensions);
    VK_CHECK_RESULT(
        vkEnumerateInstanceExtensionProperties(nullptr, &numExtensions, extensions.data())
    );

    std::unordered_set<std::string> evenOddExtensions = EVEN_ODD_INSTANCE_EXTENSIONS;

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

    DEBUG("Even-odd frame support check passed!");
}

void VulkanEngine::initVulkan()
{
    INFO("Initializing Vulkan...");
    this->createInstance();
    // this->createSurface();
    this->createDevice();
    this->initExclusiveDisplay(_mainProjectorDisplay);
    this->_device->InitQueueFamilyIndices(_mainProjectorDisplay.surface);
    this->_device->CreateLogicalDeviceAndQueue(getRequiredDeviceExtensions());
    this->_device->CreateGraphicsCommandPool();
    this->_device->CreateGraphicsCommandBuffer(NUM_FRAME_IN_FLIGHT);

    this->initSwapchains();
    this->createImageViews(_mainProjectorSwapchain);
    this->createMainRenderPass(_mainProjectorSwapchain);
    this->createDepthBuffer(_mainProjectorSwapchain);
    this->createSynchronizationObjects(_syncProjector);
    this->_imguiManager.InitializeRenderPass(
        this->_device->logicalDevice, _mainProjectorSwapchain.imageFormat
    );
    // NOTE: this has to go after ImGuiManager::InitializeRenderPass
    // because the function also creates imgui's frame buffer
    // TODO: maybe separate them?
    this->createFramebuffers(_mainProjectorSwapchain);
    { // init misc imgui resources
        this->_imguiManager.InitializeImgui();
        this->_imguiManager.InitializeDescriptorPool(NUM_FRAME_IN_FLIGHT, _device->logicalDevice);
        this->_imguiManager.BindVulkanResources(
            _window,
            _instance,
            _device->physicalDevice,
            _device->logicalDevice,
            _device->queueFamilyIndices.graphicsFamily.value(),
            _device->graphicsQueue,
            _mainProjectorSwapchain.frameBuffer.size()
        );
    }
    if (_evenOddMode) {
        DEBUG("Checking eve-odd frame device support...");
        checkHardwareEvenOddFrameSupport();
        setUpEvenOddFrame();
    }
    this->_deletionStack.push([this]() { this->_imguiManager.Cleanup(_device->logicalDevice); });

    INFO("Vulkan initialized.");
}

bool VulkanEngine::checkValidationLayerSupport()
{
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : DEFAULTS::Engine::VALIDATION_LAYERS) {
        bool layerFound = false;
        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }
        if (!layerFound) {
            return false;
        }
    }
    return true;
}

void VulkanEngine::createInstance()
{
    INFO("Creating Vulkan instance...");
    if (DEFAULTS::Engine::ENABLE_VALIDATION_LAYERS) {
        if (!this->checkValidationLayerSupport()) {
            ERROR("Validation layers requested, but not available!");
        }
    }
    // initialize and populate application info
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = DEFAULTS::Engine::APPLICATION_NAME;
    appInfo.applicationVersion = VK_MAKE_VERSION(
        DEFAULTS::Engine::APPLICATION_VERSION.major,
        DEFAULTS::Engine::APPLICATION_VERSION.minor,
        DEFAULTS::Engine::APPLICATION_VERSION.patch
    );
    appInfo.pEngineName = DEFAULTS::Engine::ENGINE_NAME;
    appInfo.engineVersion = VK_MAKE_VERSION(
        DEFAULTS::Engine::ENGINE_VERSION.major,
        DEFAULTS::Engine::ENGINE_VERSION.minor,
        DEFAULTS::Engine::ENGINE_VERSION.patch
    );
    appInfo.apiVersion = VK_API_VERSION_1_0;

    // initialize and populate createInfo, which contains the application
    // info
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    std::vector<const char*> instanceExtensions = DEFAULT_INSTANCE_EXTENSIONS;
    // get glfw Extensions
    {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        for (int i = 0; i < glfwExtensionCount; i++) {
            instanceExtensions.push_back(glfwExtensions[i]);
        }
    }
// https://stackoverflow.com/questions/72789012/why-does-vkcreateinstance-return-vk-error-incompatible-driver-on-macos-despite
#if __APPLE__
    // enable extensions for apple vulkan translation
    instanceExtensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    VkLayerSettingsCreateInfoEXT appleLayerSettings;
    {
        // metal argument buffer support
        instanceExtensions.push_back(VK_EXT_LAYER_SETTINGS_EXTENSION_NAME);
        appleLayerSettings = MoltenVKConfig::GetLayerSettingsCreatInfo();
        createInfo.pNext = &appleLayerSettings;
    }
#endif // __APPLE__
    if (_evenOddMode) {
        for (const std::string& evenOddExtensionName : EVEN_ODD_INSTANCE_EXTENSIONS) {
            instanceExtensions.push_back(evenOddExtensionName.c_str());
        }
    }
    createInfo.enabledExtensionCount = instanceExtensions.size();
    createInfo.ppEnabledExtensionNames = instanceExtensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo{};

    createInfo.enabledLayerCount = 0;
    if (DEFAULTS::Engine::ENABLE_VALIDATION_LAYERS) {
        createInfo.enabledLayerCount
            = static_cast<uint32_t>(DEFAULTS::Engine::VALIDATION_LAYERS.size());
        createInfo.ppEnabledLayerNames = DEFAULTS::Engine::VALIDATION_LAYERS.data();

        this->populateDebugMessengerCreateInfo(debugMessengerCreateInfo);
        debugMessengerCreateInfo.pNext = createInfo.pNext;
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)(&debugMessengerCreateInfo);
    } else {
        createInfo.enabledLayerCount = 0;
    }

#ifndef NDEBUG
    // enable debug printing
    // VkValidationFeatureEnableEXT enabled[] = {VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT};
    // VkValidationFeatureEnableEXT enabled[] = {};
    VkValidationFeaturesEXT features{VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT};
    features.disabledValidationFeatureCount = 0;
    features.enabledValidationFeatureCount = 0;
    features.pDisabledValidationFeatures = nullptr;
    features.pEnabledValidationFeatures = nullptr;

    features.pNext = createInfo.pNext;
    createInfo.pNext = &features;
#endif // NDEBUG

    VkResult result = vkCreateInstance(&createInfo, nullptr, &this->_instance);

    if (result != VK_SUCCESS) {
        FATAL("Failed to create Vulkan instance; Result: {}", string_VkResult(result));
    }
    _deletionStack.push([this]() { vkDestroyInstance(this->_instance, nullptr); });

    INFO("Vulkan instance created.");
}

void VulkanEngine::createGlfwWindowSurface()
{
    NEEDS_IMPLEMENTATION();
    // VkResult result
    //     = glfwCreateWindowSurface(this->_instance, this->_window, nullptr, &this->_surface);
    // if (result != VK_SUCCESS) {
    //     FATAL("Failed to create window surface.");
    // }
    // _deletionStack.push([this]() { vkDestroySurfaceKHR(this->_instance, this->_surface, nullptr); }
    // );
}

void VulkanEngine::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
                                 | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                                 | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                             | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                             | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
}

void VulkanEngine::setupDebugMessenger()
{
    VkDebugUtilsMessengerCreateInfoEXT createInfo;
    populateDebugMessengerCreateInfo(createInfo);
    { // create debug messenger
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT
        )vkGetInstanceProcAddr(_instance, "vkCreateDebugUtilsMessengerEXT");
        if (!func) {
            PANIC("Failed to find fnptr to vkCreateDebugUtilsMessengerEXT!");
        }
        if (func(_instance, &createInfo, nullptr, &_debugMessenger) != VK_SUCCESS) {
            FATAL("Failed to create debug util messenger!");
        }
    }
    _deletionStack.push([this]() {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT
        )vkGetInstanceProcAddr(_instance, "vkDestroyDebugUtilsMessengerEXT");
        if (!func) {
            PANIC("Failed to find fnptr to vkDestroyDebugUtilsMessengerEXT!");
        }
        func(
            _instance,       // instance
            _debugMessenger, // debug messenger
            nullptr          // allocator
        );
    });
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanEngine::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData
)
{
    DEBUG("{}", pCallbackData->pMessage);
    return VK_FALSE;
}

bool VulkanEngine::checkDeviceExtensionSupport(VkPhysicalDevice device)
{
    DEBUG("checking device extension support");
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(
        device, nullptr, &extensionCount, availableExtensions.data()
    );

    auto vRequiredExtensions = getRequiredDeviceExtensions();
    std::set<std::string> requiredExtensions(
        vRequiredExtensions.begin(), vRequiredExtensions.end()
    );

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

bool VulkanEngine::isDeviceSuitable(VkPhysicalDevice device)
{

    // check device properties and features
    VkPhysicalDeviceProperties deviceProperties;
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

    DEBUG("checking device {}", deviceProperties.deviceName);
    bool platformRequirements =
#if __APPLE__
        true;
#else
        deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
        && deviceFeatures.geometryShader && deviceFeatures.multiDrawIndirect;
#endif // __APPLE__

    // check queue families
    if (platformRequirements) {
        bool suitable = checkDeviceExtensionSupport(device);
        if (!suitable) {
            DEBUG("failed extension requirements");
        }
        return true;
    } else {
        DEBUG("failed platform requirements");
        return false;
    }
}

const std::vector<const char*> VulkanEngine::getRequiredDeviceExtensions() const
{
    std::vector<const char*> extensions = DEFAULT_DEVICE_EXTENSIONS;
    if (_evenOddMode) {
        for (auto extension : EVEN_ODD_DEVICE_EXTENSIONS) {
            extensions.push_back(extension);
        }
    }
    return extensions;
}

// pick a physical device that satisfies `isDeviceSuitable()`
VkPhysicalDevice VulkanEngine::pickPhysicalDevice()
{
    DEBUG("Picking physical device ");
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(this->_instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        FATAL("Failed to find any GPU!");
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(_instance, &deviceCount, devices.data());
    for (const auto& device : devices) {
        if (isDeviceSuitable(device)) {
            physicalDevice = device;
            break;
        }
    }
    if (physicalDevice == VK_NULL_HANDLE) {
        FATAL("Failed to find a suitable GPU!");
    }

    return physicalDevice;
}

void VulkanEngine::createSwapChain(VulkanEngine::SwapChainContext& ctx, const VkSurfaceKHR surface)
{
    DEBUG("creating swapchain...");
    ASSERT(_device);
    VQDevice::SwapChainSupport swapChainSupport = _device->GetSwapChainSupportForSurface(surface);
    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    DEBUG("present mode: {}", string_VkPresentModeKHR(presentMode));
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);
    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0
        && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    // createInfo.surface = _surface;
    createInfo.surface = surface;

    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    auto indices = _device->queueFamilyIndices;
    uint32_t queueFamilyIndices[]
        = {indices.graphicsFamily.value(), indices.presentationFamily.value()};

    if (indices.graphicsFamily != indices.presentationFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    createInfo.oldSwapchain = VK_NULL_HANDLE;

    VkSwapchainCounterCreateInfoEXT swapChainCounterCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_COUNTER_CREATE_INFO_EXT,
        .pNext = NULL,
        .surfaceCounters = VkSurfaceCounterFlagBitsEXT::VK_SURFACE_COUNTER_VBLANK_BIT_EXT};

    if (_evenOddMode) {
        DEBUG("swapchain created with counter support!");
        swapChainCounterCreateInfo.pNext = createInfo.pNext;
        createInfo.pNext = &swapChainCounterCreateInfo;
    }

    VkSwapchainKHR swapChain = VK_NULL_HANDLE;
    VK_CHECK_RESULT(
        vkCreateSwapchainKHR(this->_device->logicalDevice, &createInfo, nullptr, &swapChain)
    );

    ctx.chain = swapChain;
    ctx.surface = surface;
    ctx.extent = extent;
    vkGetSwapchainImagesKHR(this->_device->logicalDevice, ctx.chain, &imageCount, nullptr);
    ctx.image.resize(imageCount);
    ctx.imageView.resize(imageCount);
    ctx.frameBuffer.resize(imageCount);
    vkGetSwapchainImagesKHR(
        this->_device->logicalDevice, ctx.chain, &imageCount, ctx.image.data()
    );
    ctx.imageFormat = surfaceFormat.format;
    DEBUG("Swap chain created!");
}

void VulkanEngine::cleanupSwapChain(SwapChainContext& ctx)
{
    DEBUG("Cleaning up swap chain...");
    vkDestroyImageView(_device->logicalDevice, ctx.depthImageView, nullptr);
    vkDestroyImage(_device->logicalDevice, ctx.depthImage, nullptr);
    vkFreeMemory(_device->logicalDevice, ctx.depthImageMemory, nullptr);

    _imguiManager.DestroyFrameBuffers(_device->logicalDevice);
    for (VkFramebuffer framebuffer : ctx.frameBuffer) {
        vkDestroyFramebuffer(this->_device->logicalDevice, framebuffer, nullptr);
    }
    for (VkImageView imageView : ctx.imageView) {
        vkDestroyImageView(this->_device->logicalDevice, imageView, nullptr);
    }
    vkDestroySwapchainKHR(this->_device->logicalDevice, ctx.chain, nullptr);
}

void VulkanEngine::recreateSwapChain(SwapChainContext& ctx)
{
    // need to recreate render pass for HDR changing, we're not doing that
    // for now
    DEBUG("Recreating swap chain...");
    // handle window minimization
    int width = 0, height = 0;
    glfwGetFramebufferSize(_window, &width, &height);
    while (width == 0 || height == 0) { // when the window is minimized, wait for it to be restored
        glfwGetFramebufferSize(_window, &width, &height);
        glfwWaitEvents();
    }
    // wait for device to be idle
    vkDeviceWaitIdle(_device->logicalDevice);
    this->cleanupSwapChain(ctx);

    this->createSwapChain(ctx, ctx.surface);
    this->createImageViews(ctx);
    this->createDepthBuffer(ctx);
    this->createFramebuffers(ctx);
    DEBUG("Swap chain recreated.");
}

VkSurfaceFormatKHR VulkanEngine::chooseSwapSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& availableFormats
)
{
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB
            && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }

    return availableFormats[0];
}

VkPresentModeKHR VulkanEngine::chooseSwapPresentMode(
    const std::vector<VkPresentModeKHR>& availablePresentModes
)
{
    DEBUG("available present modes: ");
    for (const auto& availablePresentMode : availablePresentModes) {
        DEBUG("{}", string_VkPresentModeKHR(availablePresentMode));
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanEngine::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    } else {
        int width, height;
        glfwGetFramebufferSize(this->_window, &width, &height);

        VkExtent2D actualExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

        actualExtent.width = std::clamp(
            actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width
        );
        actualExtent.height = std::clamp(
            actualExtent.height,
            capabilities.minImageExtent.height,
            capabilities.maxImageExtent.height
        );

        return actualExtent;
    }
}

void VulkanEngine::createImageViews(SwapChainContext& ctx)
{
    for (size_t i = 0; i < ctx.image.size(); i++) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = _mainProjectorSwapchain.image[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = _mainProjectorSwapchain.imageFormat;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;
        if (vkCreateImageView(
                this->_device->logicalDevice,
                &createInfo,
                nullptr,
                &_mainProjectorSwapchain.imageView[i]
            )
            != VK_SUCCESS) {
            FATAL("Failed to create image views!");
        }
    }
    DEBUG("Image views created.");
}

void VulkanEngine::createSynchronizationObjects(std::array<SyncPrimitives, NUM_FRAME_IN_FLIGHT>& primitives)
{
    DEBUG("Creating synchronization objects...");
    ASSERT(primitives.size() == NUM_FRAME_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // create with a signaled
                                                    // bit so that the 1st frame
                                                    // can start right away
    for (size_t i = 0; i < NUM_FRAME_IN_FLIGHT; i++) {
        SyncPrimitives& primitive = primitives[i];
        if (vkCreateSemaphore(
                _device->logicalDevice, &semaphoreInfo, nullptr, &primitive.semaImageAvailable
            ) != VK_SUCCESS
            || vkCreateSemaphore(
                   _device->logicalDevice, &semaphoreInfo, nullptr, &primitive.semaRenderFinished
               ) != VK_SUCCESS
            || vkCreateFence(_device->logicalDevice, &fenceInfo, nullptr, &primitive.fenceInFlight)
                   != VK_SUCCESS) {
            FATAL("Failed to create synchronization objects for a frame!");
        }
    }
    this->_deletionStack.push([this, primitives]() {
        for (size_t i = 0; i < NUM_FRAME_IN_FLIGHT; i++) {
            const SyncPrimitives& primitive = primitives[i];
            vkDestroySemaphore(this->_device->logicalDevice, primitive.semaRenderFinished, nullptr);
            vkDestroySemaphore(this->_device->logicalDevice, primitive.semaImageAvailable, nullptr);
            vkDestroyFence(this->_device->logicalDevice, primitive.fenceInFlight, nullptr);
        }
    });
}

void VulkanEngine::Cleanup()
{
    INFO("Cleaning up...");
    _deletionStack.flush();
    INFO("Resource cleaned up.");
}

void VulkanEngine::createMainRenderPass(VulkanEngine::SwapChainContext& ctx)
{
    DEBUG("Creating render pass...");
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = ctx.imageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;

    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    // don't care about stencil
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    // colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // for imgui

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = VulkanUtils::findDepthFormat(_device->physicalDevice);
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    // set up subpass

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    // dependency to make sure that the render pass waits for the image to
    // be available before drawing
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                              | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                              | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask
        = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(_device->logicalDevice, &renderPassInfo, nullptr, &this->_mainRenderPass)
        != VK_SUCCESS) {
        FATAL("Failed to create render pass!");
    }

    _deletionStack.push([this]() {
        vkDestroyRenderPass(this->_device->logicalDevice, this->_mainRenderPass, nullptr);
    });
}

void VulkanEngine::createFramebuffers(SwapChainContext& ctx)
{
    DEBUG("Creating framebuffers..");
    // iterate through image views and create framebuffers
    if (_mainRenderPass == VK_NULL_HANDLE) {
        FATAL("Render pass is null!");
    }
    for (size_t i = 0; i < ctx.image.size(); i++) {
        VkImageView attachments[] = {ctx.imageView[i], ctx.depthImageView};
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = _mainRenderPass; // each framebuffer is associated with a
                                                      // render pass; they need to be compatible
                                                      // i.e. having same number of attachments and
                                                      // same formats
        framebufferInfo.attachmentCount = sizeof(attachments) / sizeof(VkImageView);
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = ctx.extent.width;
        framebufferInfo.height = ctx.extent.height;
        framebufferInfo.layers = 1; // number of layers in image arrays
        if (vkCreateFramebuffer(
                _device->logicalDevice,
                &framebufferInfo,
                nullptr,
                &_mainProjectorSwapchain.frameBuffer[i]
            )
            != VK_SUCCESS) {
            FATAL("Failed to create framebuffer!");
        }
    }
    _imguiManager.InitializeFrameBuffer(
        this->_mainProjectorSwapchain.image.size(),
        _device->logicalDevice,
        _mainProjectorSwapchain.imageView,
        _mainProjectorSwapchain.extent
    );
}

void VulkanEngine::createDepthBuffer(SwapChainContext& ctx)
{
    DEBUG("Creating depth buffer...");
    VkFormat depthFormat = VulkanUtils::findBestFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT,
        _device->physicalDevice
    );
    VulkanUtils::createImage(
        ctx.extent.width,
        ctx.extent.height,
        depthFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        ctx.depthImage,
        ctx.depthImageMemory,
        _device->physicalDevice,
        _device->logicalDevice
    );
    ctx.depthImageView = VulkanUtils::createImageView(
        ctx.depthImage, _device->logicalDevice, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT
    );
}

void VulkanEngine::flushEngineUBOStatic(uint8_t frame)
{
    VQBuffer& buf = _engineUBOStatic[frame];
    EngineUBOStatic ubo{
        _mainCamera.GetViewMatrix() // view
        // proj
    };
    getMainProjectionMatrix(ubo.proj);
    ubo.timeSinceStartSeconds = _timeSinceStartSeconds;
    ubo.sinWave = (sin(_timeSinceStartSeconds) + 1) / 2.f; // offset to [0, 1]
    ubo.flip = _numTicks % 2 == 0;
    memcpy(buf.bufferAddress, &ubo, sizeof(ubo));
}

#define POWER_ON_DISPLAY 0
void VulkanEngine::drawFrame(TickContext* ctx, uint8_t frame)
{
#if POWER_ON_DISPLAY
    static bool poweredOn = false;
    if (false && !poweredOn) { // power display on, seems unnecessary
        poweredOn = true;
        // power on display
        DEBUG("Turning display on...");
        VkDisplayPowerInfoEXT powerInfo{
            .sType = VK_STRUCTURE_TYPE_DISPLAY_POWER_INFO_EXT,
            .pNext = VK_NULL_HANDLE,
            .powerState = VkDisplayPowerStateEXT::VK_DISPLAY_POWER_STATE_ON_EXT};
        PFN_vkDisplayPowerControlEXT fnPtr = reinterpret_cast<PFN_vkDisplayPowerControlEXT>(
            vkGetInstanceProcAddr(_instance, "vkDisplayPowerControlEXT")
        );
        ASSERT(fnPtr);
        VK_CHECK_RESULT(fnPtr(_device->logicalDevice, _mainProjectorDisplay.display, &powerInfo));
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
#endif // POWER_ON_DISPLAY
    SyncPrimitives& sync = _syncProjector[frame];

    //  Wait for the previous frame to finish
    PROFILE_SCOPE(&_profiler, "Render Tick");
    vkWaitForFences(_device->logicalDevice, 1, &sync.fenceInFlight, VK_TRUE, UINT64_MAX);

    auto res = _pFNvkGetSwapchainCounterEXT(
        _device->logicalDevice,
        _mainProjectorSwapchain.chain,
        VkSurfaceCounterFlagBitsEXT::VK_SURFACE_COUNTER_VBLANK_EXT,
        &_surfaceCounterValue
    );

    //  Acquire an image from the swap chain
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(
        this->_device->logicalDevice,
        _mainProjectorSwapchain.chain,
        UINT64_MAX,
        sync.semaImageAvailable,
        VK_NULL_HANDLE,
        &imageIndex
    );
    [[unlikely]] if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        this->recreateSwapChain(_mainProjectorSwapchain);
        return;
    }
    else [[unlikely]] if (result != VK_SUCCESS)
    {
        const char* res = string_VkResult(result);
        PANIC("Failed to acquire swap chain image: {}", res);
    }

    // lock the fence
    vkResetFences(this->_device->logicalDevice, 1, &sync.fenceInFlight);

    VkFramebuffer FB = this->_mainProjectorSwapchain.frameBuffer[imageIndex];
    vk::CommandBuffer CB(_device->graphicsCommandBuffers[frame]);
    //  Record a command buffer which draws the scene onto that image
    CB.reset();
    { // update ctx->graphics field
        ctx->graphics.currentFrameInFlight = frame;
        ctx->graphics.currentSwapchainImageIndex = imageIndex;
        ctx->graphics.CB = CB;
        ctx->graphics.currentFB = FB;
        ctx->graphics.currentFBextend = _mainProjectorSwapchain.extent;
        getMainProjectionMatrix(ctx->graphics.mainProjectionMatrix);
    }

    { // begin command buffer
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;                  // Optional
        beginInfo.pInheritanceInfo = nullptr; // Optional
        CB.begin(vk::CommandBufferBeginInfo());
    }

    { // main render pass
        vk::Extent2D extend = _mainProjectorSwapchain.extent;
        // the main render pass renders the actual graphics of the game.
        { // begin main render pass
            vk::Rect2D renderArea(VkOffset2D{0, 0}, extend);
            std::array<vk::ClearValue, 2> clearValues{};
            clearValues[0].color = {0.f, 0.f, 0.f, 1.f};
            clearValues[1].depthStencil = vk::ClearDepthStencilValue(1.f, 0.f);
            vk::RenderPassBeginInfo renderPassBeginInfo(
                _mainRenderPass, FB, renderArea, clearValues.size(), clearValues.data(), nullptr
            );

            CB.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
        }
        { // set viewport and scissor
            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(extend.width);
            viewport.height = static_cast<float>(extend.height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(CB, 0, 1, &viewport);

            VkRect2D scissor{};
            scissor.offset = {0, 0};
            scissor.extent = extend;
            vkCmdSetScissor(CB, 0, 1, &scissor);
        }
        _renderer.Tick(ctx);
        CB.endRenderPass();
    }

    _imguiManager.RecordCommandBuffer(ctx);

    // end command buffer
    CB.end();

    //  Submit the recorded command buffer
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {sync.semaImageAvailable}; // use imageAvailable semaphore
                                                              // to make sure that the image
                                                              // is available before drawing
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    std::array<VkCommandBuffer, 1> submitCommandBuffers
        = {this->_device->graphicsCommandBuffers[frame]};

    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = static_cast<uint32_t>(submitCommandBuffers.size());
    submitInfo.pCommandBuffers = submitCommandBuffers.data();

    VkSemaphore signalSemaphores[] = {sync.semaRenderFinished};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    {
        // wait time tend to be long if framerate is hardware-capped.
        PROFILE_SCOPE(&_profiler, "wait: vkAcquireNextImageKHR");
        // the submission does not start until vkAcquireNextImageKHR
        // returns, and downs the corresponding _semaRenderFinished
        // semapohre once it's done.
        if (vkQueueSubmit(_device->graphicsQueue, 1, &submitInfo, sync.fenceInFlight)
            != VK_SUCCESS) {
            FATAL("Failed to submit draw command buffer!");
        }
    }

    //  Present the swap chain image
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    // wait for sync.semaRenderFinished upped by the previous render command
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {_mainProjectorSwapchain.chain};

    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex; // specify which image to present
    presentInfo.pResults = nullptr;          // Optional: can be used to check if
                                             // presentation was successful

    // the present doesn't happen until the render is finished, and the
    // semaphore is signaled(result of vkQueueSubimt)
    result = vkQueuePresentKHR(_device->presentationQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR
        || this->_framebufferResized) {
        [[unlikely]] this->recreateSwapChain(_mainProjectorSwapchain);
        this->_framebufferResized = false;
    }
    VK_CHECK_RESULT(result);
}

void VulkanEngine::initSwapchains()
{
    createSwapChain(_mainProjectorSwapchain, _mainProjectorDisplay.surface);
    this->_deletionStack.push([this]() { this->cleanupSwapChain(_mainProjectorSwapchain); });
}

void VulkanEngine::drawImGui()
{
    if (!_wantToDrawImGui) {
        return;
    }
    PROFILE_SCOPE(&_profiler, "ImGui Draw");
    _imguiManager.BeginImGuiContext();
    ImGui::SetNextWindowPos({0, 0});
    if (ImGui::Begin(DEFAULTS::Engine::APPLICATION_NAME)) {
        if (ImGui::BeginTabBar("Engine Tab")) {
            if (ImGui::BeginTabItem("General")) {
                ImGui::Text("Surface counter: %lu", _surfaceCounterValue);
                ImGui::SeparatorText("Camera");
                {
                    ImGui::Text(
                        "Position: (%f, %f, %f)",
                        _mainCamera.GetPosition().x,
                        _mainCamera.GetPosition().y,
                        _mainCamera.GetPosition().z
                    );
                    ImGui::Text(
                        "Yaw: %f Pitch: %f Roll: %f",
                        _mainCamera.GetRotation().y,
                        _mainCamera.GetRotation().x,
                        _mainCamera.GetRotation().z
                    );
                    ImGui::SliderFloat("FOV", &_FOV, 30, 120, "%.f");
                }
                if (ImGui::Button("Reset")) {
                    _mainCamera.SetPosition(0, 0, 0);
                }
                ImGui::SeparatorText("Cursor Lock(tab)");
                if (_lockCursor) {
                    ImGui::Text("Cursor Lock: Active");
                } else {
                    ImGui::Text("Cursor Lock: Deactive");
                }
                ImGui::SeparatorText("Engine UBO");
                _widgetUBOViewer.Draw(this);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Performance")) {
                _widgetPerfPlot.Draw(this);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Device")) {
                _widgetDeviceInfo.Draw(this);
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar(); // Engine Tab
        }
    }

    ImGui::End(); // VulkanEngine
    _imguiManager.EndImGuiContext();
}

void VulkanEngine::bindDefaultInputs()
{
    const int CAMERA_SPEED = 3;
    _inputManager.RegisterCallback(GLFW_KEY_W, InputManager::KeyCallbackCondition::HOLD, [this]() {
        _mainCamera.Move(_deltaTimer.GetDeltaTime() * CAMERA_SPEED, 0, 0);
    });
    _inputManager.RegisterCallback(GLFW_KEY_S, InputManager::KeyCallbackCondition::HOLD, [this]() {
        _mainCamera.Move(-_deltaTimer.GetDeltaTime() * CAMERA_SPEED, 0, 0);
    });
    _inputManager.RegisterCallback(GLFW_KEY_A, InputManager::KeyCallbackCondition::HOLD, [this]() {
        _mainCamera.Move(0, _deltaTimer.GetDeltaTime() * CAMERA_SPEED, 0);
    });
    _inputManager.RegisterCallback(GLFW_KEY_D, InputManager::KeyCallbackCondition::HOLD, [this]() {
        _mainCamera.Move(0, -_deltaTimer.GetDeltaTime() * CAMERA_SPEED, 0);
    });
    _inputManager.RegisterCallback(
        GLFW_KEY_LEFT_CONTROL,
        InputManager::KeyCallbackCondition::HOLD,
        [this]() { _mainCamera.Move(0, 0, -CAMERA_SPEED * _deltaTimer.GetDeltaTime()); }
    );
    _inputManager.RegisterCallback(
        GLFW_KEY_SPACE,
        InputManager::KeyCallbackCondition::HOLD,
        [this]() { _mainCamera.Move(0, 0, CAMERA_SPEED * _deltaTimer.GetDeltaTime()); }
    );

    _inputManager
        .RegisterCallback(GLFW_KEY_TAB, InputManager::KeyCallbackCondition::PRESS, [this]() {
            _lockCursor = !_lockCursor;
            if (_lockCursor) {
                glfwSetInputMode(_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                ImGui::GetIO().ConfigFlags
                    |= (ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoKeyboard);
            } else {
                glfwSetInputMode(_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
                ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoKeyboard;
            }
        });
    _inputManager.RegisterCallback(
        GLFW_KEY_ESCAPE,
        InputManager::KeyCallbackCondition::PRESS,
        [this]() { glfwSetWindowShouldClose(_window, GLFW_TRUE); }
    );
    // flip imgui draw with "`" key
    _inputManager.RegisterCallback(
        GLFW_KEY_GRAVE_ACCENT,
        InputManager::KeyCallbackCondition::PRESS,
        [this]() {
            _wantToDrawImGui = !_wantToDrawImGui;
            if (!_wantToDrawImGui) {
                _imguiManager.ClearImGuiElements();
            }
        }
    );
}

void VulkanEngine::getMainProjectionMatrix(glm::mat4& projectionMatrix)
{
    auto& extent = _mainProjectorSwapchain.extent;
    projectionMatrix = glm::perspective(
        glm::radians(_FOV),
        extent.width / static_cast<float>(extent.height),
        DEFAULTS::ZNEAR,
        DEFAULTS::ZFAR
    );
    projectionMatrix[1][1] *= -1; // invert for vulkan coord system
}
