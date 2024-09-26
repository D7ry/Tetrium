#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
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
#include "lib/ImGuiUtils.h"

#include "lib/Utils.h"

// Molten VK Config
#if __APPLE__
#include "MoltenVKConfig.h"
#endif // __APPLE__

// ecs
#include "ecs/component/TransformComponent.h"

#include "Quarkolor.h"

#if __linux__
#include <xf86drm.h>
#include <xf86drmMode.h>
#endif // __linux__

#define VIRTUAL_VSYNC 0

// creates a cow for now
void Quarkolor::createFunnyObjects()
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
    _renderer.AddEntity(spot);
    spot->GetComponent<TransformComponent>()->rotation.x = 90;
    spot->GetComponent<TransformComponent>()->rotation.y = 90;
    spot->GetComponent<TransformComponent>()->position.z = 0.05;
    // register lil cow
}

// in CLI pop up a monitor selection interface, that lists
// monitor names and properties
// the user would input a number to select the right monitor.
std::pair<GLFWmonitor*, GLFWvidmode> Quarkolor::cliMonitorModeSelection()
{
    std::cout << "---------- Please Select Monitor and Video Mode ----------" << std::endl;

    int numMonitors;
    GLFWmonitor** monitors = glfwGetMonitors(&numMonitors);
    if (!monitors || numMonitors == 0) {
        std::cerr << "No monitors detected!" << std::endl;
        return {nullptr, GLFWvidmode{}};
    }

    std::vector<std::pair<GLFWmonitor*, GLFWvidmode>> monitorModes;

    for (int monitorIdx = 0; monitorIdx < numMonitors; monitorIdx++) {
        GLFWmonitor* monitor = monitors[monitorIdx];
        if (!monitor)
            continue;

        const char* name = glfwGetMonitorName(monitor);
        if (!name)
            name = "Unknown";

        int modeCount;
        const GLFWvidmode* modes = glfwGetVideoModes(monitor, &modeCount);
        if (!modes || modeCount == 0)
            continue;

        fmt::println("Monitor {}: {}", monitorIdx, name);
        for (int modeIdx = 0; modeIdx < modeCount; modeIdx++) {
            const GLFWvidmode& mode = modes[modeIdx];
            fmt::println(
                "  {}: {} x {}, {} Hz",
                monitorModes.size(),
                mode.width,
                mode.height,
                mode.refreshRate
            );
            monitorModes.emplace_back(monitor, mode);
        }
    }

    if (monitorModes.empty()) {
        std::cerr << "No valid monitor modes found!" << std::endl;
        return {nullptr, GLFWvidmode{}};
    }

    std::cout << "-----------------------------------------------------" << std::endl;

    int selectedModeIdx = -1;
    do {
        std::cout << "Enter the number of the desired mode: ";
        std::string input;
        std::getline(std::cin, input);
        try {
            selectedModeIdx = std::stoi(input);
            if (selectedModeIdx >= 0 && selectedModeIdx < static_cast<int>(monitorModes.size())) {
                break;
            }
        } catch (const std::exception&) {
            // Invalid input, will prompt again
        }
        std::cout << "Invalid input. Please enter a number between 0 and "
                  << monitorModes.size() - 1 << std::endl;
    } while (true);

    return monitorModes[selectedModeIdx];
}

#if __linux__
// select exclusive display using DRM
void Quarkolor::selectDisplayDRM(DisplayContext& ctx)
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
void Quarkolor::selectDisplayXlib(DisplayContext& ctx)
{
    using namespace fmt;
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

    println("============== Choose Display ===============");

    for (size_t i = 0; i < displays.size(); ++i) {
        VkDisplayPropertiesKHR displayProperties;
        println("[{}] {}", i, displayNames[i]);
    }

    // Prompt user for selection
    size_t selectedIndex = 0; // defaults to 1 for vulkan configurator
    do {
        print("Display({}-{}):", 0, displays.size() - 1);
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
#endif // __linux__

// take complete control over a physical display
// the display must be directly connected to the GPU through x11
// prompts the user to select the display and resolution in an ImGui window
void Quarkolor::initExclusiveDisplay(Quarkolor::DisplayContext& ctx)
{
#if __linux__
    selectDisplayXlib(ctx);
#elif __APPLE__
    NEEDS_IMPLEMENTATION();
#elif WIN32
    NEEDS_IMPLEMENTATION();
#else
    NEEDS_IMPLEMENTATION();
#endif

    ASSERT(ctx.display != VK_NULL_HANDLE);

    auto device = _device->physicalDevice;
    uint32_t modeCount = 0;
    vkGetDisplayModePropertiesKHR(device, ctx.display, &modeCount, nullptr);
    std::vector<VkDisplayModePropertiesKHR> modeProperties(modeCount);
    vkGetDisplayModePropertiesKHR(device, ctx.display, &modeCount, modeProperties.data());

    using namespace fmt;
    // List all modes for the selected display
    println("\n============== Available Display Modes ==============");
    println("{:<6} {:<20} {:<15}", "Index", "Resolution", "Refresh Rate");
    println("{:<6} {:<20} {:<15}", "-----", "----------", "------------");

    for (uint32_t i = 0; i < modeCount; ++i) {
        const auto& mode = modeProperties[i].parameters;
        println(
            "{:<6} {:<20} {:<15.2f} Hz",
            i,
            fmt::format("{}x{}", mode.visibleRegion.width, mode.visibleRegion.height),
            static_cast<float>(mode.refreshRate) / 1000.0f
        );
    }

    // Get user input for mode selection
    uint32_t selectedModeIndex = 0;
    do {
        println("\nPlease enter the index of the mode you want to use (0-{}):", modeCount - 1);
        print("> ");
        std::cin >> selectedModeIndex;

        if (selectedModeIndex >= modeCount) {
            println("Invalid selection. Please try again.");
        }
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
    ctx.refreshrate = modeProperties[selectedModeIndex].parameters.refreshRate;
    DEBUG("display extent: {} {}", ctx.extent.width, ctx.extent.height);
}

void Quarkolor::initGLFW(const InitOptions& options)
{
    glfwInit();
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE); // hide window at beginning
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
        monitor = glfwGetPrimaryMonitor();
        if (options.manualMonitorSelection) {
            auto ret = cliMonitorModeSelection();
            monitor = ret.first;
            auto mode = ret.second;
            glfwWindowHint(GLFW_RED_BITS, mode.redBits);
            glfwWindowHint(GLFW_GREEN_BITS, mode.greenBits);
            glfwWindowHint(GLFW_BLUE_BITS, mode.blueBits);
            glfwWindowHint(GLFW_REFRESH_RATE, mode.refreshRate);
            width = mode.width;
            height = mode.height;
        }
        fmt::println("Selected {} as full-screen monitor.", glfwGetMonitorName(monitor));
    }

    this->_window
        = glfwCreateWindow(width, height, DEFAULTS::Engine::APPLICATION_NAME, monitor, nullptr);
    if (this->_window == nullptr) {
        FATAL("Failed to initialize GLFW windlw!");
    }
    glfwSetWindowUserPointer(_window, this);
    _deletionStack.push([this]() {
        glfwDestroyWindow(this->_window);
        glfwTerminate();
    });
}

void Quarkolor::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    Quarkolor* pThis = reinterpret_cast<Quarkolor*>(glfwGetWindowUserPointer(window));
    if (key == GLFW_KEY_P && action == GLFW_PRESS) {
        _paused = !_paused;
    }
    _inputManager.OnKeyInput(window, key, scancode, action, mods);

    // toggle cursor lock
    if (key == GLFW_KEY_TAB && action == GLFW_PRESS) {
        _lockCursor = !_lockCursor;
        if (_lockCursor) {
            glfwSetInputMode(_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        } else {
            glfwSetInputMode(_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
        // only activate input on cursor lock
        _inputManager.SetActive(_lockCursor);
    }
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(_window, GLFW_TRUE);
    }
}

void Quarkolor::cursorPosCallback(GLFWwindow* window, double xpos, double ypos)
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
    if (_lockCursor && !_uiMode) {
        _mainCamera.ModRotation(deltaX, deltaY, 0);
    }
    prevX = xpos;
    prevY = ypos;
}

void Quarkolor::initDefaultStates()
{
    // configure states

    // clear color
    _clearValues[0].color = {0.5f, 0.3f, 0.1f, 1.f};
    _clearValues[1].depthStencil = vk::ClearDepthStencilValue(1.f, 0.f);

    // camera location
    _mainCamera.SetPosition(-2, 0, 0);

    // input states
    // by default, unlock cursor, disable imgui inputs, disable input handling
    _lockCursor = false;
    _inputManager.SetActive(_lockCursor);
    _uiMode = false;

    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoKeyboard;
};

void Quarkolor::Init(const Quarkolor::InitOptions& options)
{
    // populate static config fields
    _tetraMode = options.tetraMode;
    if (_tetraMode == TetraMode::kDualProjector) {
        NEEDS_IMPLEMENTATION();
    }
    if (_tetraMode == TetraMode::kEvenOddSoftwareSync && options.fullScreen == false) {
        PANIC("Even-odd software sync requires full-screen glfw window.! ");
    }
#if __APPLE__
    MoltenVKConfig::Setup();
#endif // __APPLE__
    initGLFW(options);
    ASSERT(_window);
    { // Input Handling
        auto keyCallback = [](GLFWwindow* window, int key, int scancode, int action, int mods) {
            Quarkolor* pThis = reinterpret_cast<Quarkolor*>(glfwGetWindowUserPointer(window));
            pThis->keyCallback(window, key, scancode, action, mods);
        };
        glfwSetKeyCallback(this->_window, keyCallback);

        auto cursorPosCallback = [](GLFWwindow* window, double xpos, double ypos) {
            Quarkolor* pThis = reinterpret_cast<Quarkolor*>(glfwGetWindowUserPointer(window));
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
        initCtx.swapChainImageFormat = this->_renderContexts.RGB.swapchain->imageFormat;
        initCtx.renderPass.RGB = _renderContexts.RGB.renderPass;
        initCtx.renderPass.CMY = _renderContexts.CMY.renderPass;
        for (int i = 0; i < _engineUBOStatic.size(); i++) {
            initCtx.engineUBOStaticDescriptorBufferInfo[i].range = sizeof(EngineUBOStatic);
            initCtx.engineUBOStaticDescriptorBufferInfo[i].buffer = _engineUBOStatic[i].buffer;
            initCtx.engineUBOStaticDescriptorBufferInfo[i].offset = 0;
        }
    }

    _renderer.Init(&initCtx);
    _deletionStack.push([this]() { _renderer.Cleanup(); });

    initDefaultStates();

    createFunnyObjects();
}

void Quarkolor::Run()
{
    ASSERT(_window);
    glfwShowWindow(_window);
    while (!glfwWindowShouldClose(_window)) {
        glfwPollEvents();
        Tick();
    }
}

void Quarkolor::Tick()
{
    if (_paused) {
        std::this_thread::yield();
        return;
    }
    _deltaTimer.Tick();
    {
        PROFILE_SCOPE(&_profiler, "Main Tick");
        {
            PROFILE_SCOPE(&_profiler, "CPU");
            // CPU-exclusive workloads
            double deltaTime = _deltaTimer.GetDeltaTime();
            _timeSinceStartSeconds += deltaTime;
            _timeSinceStartNanoSeconds += _deltaTimer.GetDeltaTimeNanoSeconds();
            _inputManager.Tick(deltaTime);
            TickContext tickData{&_mainCamera, deltaTime};
            tickData.profiler = &_profiler;
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

void Quarkolor::framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    DEBUG("Window resized to {}x{}", width, height);
    auto app = reinterpret_cast<Quarkolor*>(glfwGetWindowUserPointer(window));
    app->_framebufferResized = true;
}

VkSurfaceKHR Quarkolor::createGlfwWindowSurface(GLFWwindow* window)
{
    VkSurfaceKHR surface;
    VkResult result = glfwCreateWindowSurface(this->_instance, this->_window, nullptr, &surface);
    if (result != VK_SUCCESS) {
        FATAL("Failed to create window surface.");
    }
    _deletionStack.push([this, surface]() {
        vkDestroySurfaceKHR(this->_instance, surface, nullptr);
    });

    return surface;
}

void Quarkolor::createDevice()
{
    VkPhysicalDevice physicalDevice = this->pickPhysicalDevice();
    this->_device = std::make_shared<VQDevice>(physicalDevice);
    this->_deletionStack.push([this]() { this->_device->Cleanup(); });
}

void Quarkolor::setupSoftwareEvenOddFrame()
{
    DEBUG("Setting up even-odd frame resources...");
    auto& ctx = _softwareEvenOddCtx;
    ctx.timeEngineStart = std::chrono::steady_clock::now();

    // get refresh cycle
    VkRefreshCycleDurationGOOGLE refreshCycleDuration;
    ASSERT(_renderContexts.RGB.swapchain->chain);
    ASSERT(_renderContexts.CMY.swapchain->chain);
    ASSERT(_device->logicalDevice);

    PFN_vkGetRefreshCycleDurationGOOGLE ptr = reinterpret_cast<PFN_vkGetRefreshCycleDurationGOOGLE>(
        vkGetInstanceProcAddr(_instance, "vkGetRefreshCycleDurationGOOGLE")
    );
    ASSERT(ptr);
    VK_CHECK_RESULT(
        ptr(_device->logicalDevice, _renderContexts.RGB.swapchain->chain, &refreshCycleDuration)
    );
    ctx.nanoSecondsPerFrame = refreshCycleDuration.refreshDuration;
    ASSERT(ctx.nanoSecondsPerFrame != 0);
}

void Quarkolor::checkSoftwareEvenOddFrameSupport() { return; }

void Quarkolor::setupHardwareEvenOddFrame()
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
void Quarkolor::checkHardwareEvenOddFrameSupport()
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

    std::unordered_set<std::string> evenOddExtensions = EVEN_ODD_HARDWARE_INSTANCE_EXTENSIONS;

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

void Quarkolor::initVulkan()
{
    VkSurfaceKHR mainWindowSurface = VK_NULL_HANDLE;
    INFO("Initializing Vulkan...");
    this->createInstance();
    this->createDevice();
    switch (_tetraMode) {
    case TetraMode::kEvenOddHardwareSync:
        this->initExclusiveDisplay(_mainProjectorDisplay);
        mainWindowSurface = _mainProjectorDisplay.surface;
        break;
    case TetraMode::kEvenOddSoftwareSync:
        mainWindowSurface = createGlfwWindowSurface(_window);
        break;
    default:
        NEEDS_IMPLEMENTATION();
    };

    ASSERT(mainWindowSurface);

    this->_device->InitQueueFamilyIndices(mainWindowSurface);
    this->_device->CreateLogicalDeviceAndQueue(getRequiredDeviceExtensions());
    this->_device->CreateGraphicsCommandPool();
    this->_device->CreateGraphicsCommandBuffer(NUM_FRAME_IN_FLIGHT);

    createSwapChain(_swapChain, mainWindowSurface);
    createImageViews(_swapChain);
    ASSERT(_swapChain.imageFormat);
    createDepthBuffer(_swapChain);

    // create context for rgb and cmy rendering
    for (RenderContext* ctx : {&_renderContexts.RGB, &_renderContexts.CMY}) {
        ctx->swapchain = &_swapChain;
        ctx->renderPass = createRenderPass(ctx->swapchain->imageFormat);
        createVirtualFrameBuffers(*ctx);
        _deletionStack.push([this, ctx] {
            vkDestroyRenderPass(_device->logicalDevice, ctx->renderPass, NULL);
            clearVirtualFrameBuffers(*ctx);
        });
    }

    // create framebuffer for swapchain
    createSwapchainFrameBuffers(_swapChain, _renderContexts.RGB.renderPass);

    _deletionStack.push([this] { cleanupSwapChain(_swapChain); });

    this->createSynchronizationObjects(_syncProjector);

    // initial layout comes from separate render passes,
    // final layout depends on tetra mode.
    VkImageLayout imguiInitialLayout, imguiFinalLayout;
    imguiInitialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    imguiFinalLayout
        = _tetraMode == TetraMode::kDualProjector
              ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR // dual project's two passes directly present
              : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

    this->_imguiManager.InitializeRenderPass(
        this->_device->logicalDevice, _swapChain.imageFormat, imguiInitialLayout, imguiFinalLayout
    );
    _imguiManager.InitializeFrameBuffer(
        _swapChain.image.size(),
        _device->logicalDevice,
        _renderContexts.RGB.virtualFrameBuffer.imageView,
        _renderContexts.CMY.virtualFrameBuffer.imageView,
        _swapChain.extent
    );
    this->_imguiManager.InitializeImgui();
    this->_imguiManager.InitializeDescriptorPool(
        DEFAULTS::ImGui::TEXTURE_DESCRIPTOR_POOL_SIZE, _device->logicalDevice
    );
    this->_imguiManager.BindVulkanResources(
        _window,
        _instance,
        _device->physicalDevice,
        _device->logicalDevice,
        _device->queueFamilyIndices.graphicsFamily.value(),
        _device->graphicsQueue,
        _renderContexts.RGB.virtualFrameBuffer.frameBuffer.size(
        ) // doesn't matter if it's RGB or CMY
    );

    // even-odd specific resource checkup and setup
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
        NEEDS_IMPLEMENTATION();
    }
    this->_deletionStack.push([this]() { this->_imguiManager.Cleanup(_device->logicalDevice); });

    INFO("Vulkan initialized.");
}

bool Quarkolor::checkValidationLayerSupport()
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

void Quarkolor::createInstance()
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
    switch (_tetraMode) {
    case TetraMode::kEvenOddHardwareSync:
        for (const std::string& evenOddExtensionName : EVEN_ODD_HARDWARE_INSTANCE_EXTENSIONS) {
            instanceExtensions.push_back(evenOddExtensionName.c_str());
        }
        break;
    default:
        break;
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

void Quarkolor::createGlfwWindowSurface()
{
    NEEDS_IMPLEMENTATION();
    // VkResult result
    //     = glfwCreateWindowSurface(this->_instance, this->_window, nullptr, &this->_surface);
    // if (result != VK_SUCCESS) {
    //     FATAL("Failed to create window surface.");
    // }
    // _deletionStack.push([this]() { vkDestroySurfaceKHR(this->_instance, this->_surface, nullptr);
    // }
    // );
}

void Quarkolor::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
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

void Quarkolor::setupDebugMessenger()
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

VKAPI_ATTR VkBool32 VKAPI_CALL Quarkolor::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData
)
{
    DEBUG("{}", pCallbackData->pMessage);
    return VK_FALSE;
}

bool Quarkolor::checkDeviceExtensionSupport(VkPhysicalDevice device)
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

bool Quarkolor::isDeviceSuitable(VkPhysicalDevice device)
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

const std::vector<const char*> Quarkolor::getRequiredDeviceExtensions() const
{
    std::vector<const char*> extensions = DEFAULT_DEVICE_EXTENSIONS;
    if (_tetraMode == TetraMode::kEvenOddHardwareSync) {
        for (auto extension : EVEN_ODD_HARDWARE_DEVICE_EXTENSIONS) {
            extensions.push_back(extension);
        }
    }
    return extensions;
}

// pick a physical device that satisfies `isDeviceSuitable()`
VkPhysicalDevice Quarkolor::pickPhysicalDevice()
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

void Quarkolor::createSwapChain(Quarkolor::SwapChainContext& ctx, const VkSurfaceKHR surface)
{
    DEBUG("creating swapchain...");
    ASSERT(_device);
    VQDevice::SwapChainSupport swapChainSupport = _device->GetSwapChainSupportForSurface(surface);
    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    DEBUG("present mode: {}", string_VkPresentModeKHR(presentMode));
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);
    DEBUG("extent: {} {}", extent.width, extent.height);
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
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

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
        .surfaceCounters = VkSurfaceCounterFlagBitsEXT::VK_SURFACE_COUNTER_VBLANK_BIT_EXT
    };

    if (_tetraMode == TetraMode::kEvenOddHardwareSync) {
#if __linux__
        DEBUG("swapchain created with counter support!");
        swapChainCounterCreateInfo.pNext = createInfo.pNext;
        createInfo.pNext = &swapChainCounterCreateInfo;
#endif // __linux__
#if WIN32
        NEEDS_IMPLEMENTATION();
#endif // WIN32
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
    vkGetSwapchainImagesKHR(this->_device->logicalDevice, ctx.chain, &imageCount, ctx.image.data());
    ctx.imageFormat = surfaceFormat.format;
    DEBUG("Swap chain created!");
}

void Quarkolor::cleanupSwapChain(SwapChainContext& ctx)
{
    DEBUG("Cleaning up swap chain...");
    vkDestroyImageView(_device->logicalDevice, ctx.depthImageView, nullptr);
    vkDestroyImage(_device->logicalDevice, ctx.depthImage, nullptr);
    vkFreeMemory(_device->logicalDevice, ctx.depthImageMemory, nullptr);

    for (VkFramebuffer framebuffer : ctx.frameBuffer) {
        vkDestroyFramebuffer(this->_device->logicalDevice, framebuffer, nullptr);
    }
    for (VkImageView imageView : ctx.imageView) {
        vkDestroyImageView(this->_device->logicalDevice, imageView, nullptr);
    }
    vkDestroySwapchainKHR(this->_device->logicalDevice, ctx.chain, nullptr);
}

void Quarkolor::recreateVirtualFrameBuffers()
{
    clearVirtualFrameBuffers(_renderContexts.RGB);
    clearVirtualFrameBuffers(_renderContexts.CMY);
    createVirtualFrameBuffers(_renderContexts.RGB);
    createVirtualFrameBuffers(_renderContexts.CMY);
    _imguiManager.DestroyFrameBuffers(_device->logicalDevice);
    _imguiManager.InitializeFrameBuffer(
        _swapChain.image.size(),
        _device->logicalDevice,
        _renderContexts.RGB.virtualFrameBuffer.imageView,
        _renderContexts.CMY.virtualFrameBuffer.imageView,
        _swapChain.extent
    );
}

void Quarkolor::recreateSwapChain(SwapChainContext& ctx)
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
    this->createSwapchainFrameBuffers(ctx, _renderContexts.RGB.renderPass);
    DEBUG("Swap chain recreated.");
}

VkSurfaceFormatKHR Quarkolor::chooseSwapSurfaceFormat(
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

VkPresentModeKHR Quarkolor::chooseSwapPresentMode(
    const std::vector<VkPresentModeKHR>& availablePresentModes
)
{
#if VIRTUAL_VSYNC
    return VK_PRESENT_MODE_IMMEDIATE_KHR; // force immediate mode
#endif                                    // VIRTUAL_VSYNC
    INFO("available present modes: ");
    for (const auto& availablePresentMode : availablePresentModes) {
        INFO("{}", string_VkPresentModeKHR(availablePresentMode));
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Quarkolor::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
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

void Quarkolor::createImageViews(SwapChainContext& ctx)
{
    for (size_t i = 0; i < ctx.image.size(); i++) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = ctx.image[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = ctx.imageFormat;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;
        if (vkCreateImageView(this->_device->logicalDevice, &createInfo, nullptr, &ctx.imageView[i])
            != VK_SUCCESS) {
            FATAL("Failed to create image views!");
        }
    }
    DEBUG("Image views created.");
}

void Quarkolor::createSynchronizationObjects(
    std::array<SyncPrimitives, NUM_FRAME_IN_FLIGHT>& primitives
)
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
        for (VkSemaphore* sema :
             {&primitive.semaImageAvailable,
              &primitive.semaRenderFinished,
              &primitive.semaImageCopyFinished}) {
            VK_CHECK_RESULT(vkCreateSemaphore(_device->logicalDevice, &semaphoreInfo, nullptr, sema)
            );
        }
        VK_CHECK_RESULT(
            vkCreateFence(_device->logicalDevice, &fenceInfo, nullptr, &primitive.fenceInFlight)
        );

        // create vsync semahore as a timeline semaphore
        VkSemaphoreTypeCreateInfo timelineCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
            .pNext = NULL,
            .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
            .initialValue = 0,
        };
        semaphoreInfo.pNext = &timelineCreateInfo;
        VK_CHECK_RESULT(
            vkCreateSemaphore(_device->logicalDevice, &semaphoreInfo, nullptr, &primitive.semaVsync)
        );
        semaphoreInfo.pNext = nullptr; // reset for next loop
    }
    this->_deletionStack.push([this, primitives]() {
        for (size_t i = 0; i < NUM_FRAME_IN_FLIGHT; i++) {
            const SyncPrimitives& primitive = primitives[i];
            for (auto& sema :
                 {primitive.semaVsync,
                  primitive.semaImageCopyFinished,
                  primitive.semaRenderFinished,
                  primitive.semaImageAvailable}) {
                vkDestroySemaphore(this->_device->logicalDevice, sema, nullptr);
            }
            vkDestroyFence(this->_device->logicalDevice, primitive.fenceInFlight, nullptr);
        }
    });
}

void Quarkolor::Cleanup()
{
    INFO("Cleaning up...");
    _deletionStack.flush();
    INFO("Resource cleaned up.");
}

// create a render pass. The render pass will be pushed onto
// the deletion stack.
vk::RenderPass Quarkolor::createRenderPass(const VkFormat imageFormat)
{
    DEBUG("Creating render pass...");
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = imageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;

    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    // don't care about stencil
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

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

    VkRenderPass pass = VK_NULL_HANDLE;
    VK_CHECK_RESULT(vkCreateRenderPass(_device->logicalDevice, &renderPassInfo, nullptr, &pass));

    return vk::RenderPass(pass);
}

uint32_t findMemoryType(
    VkPhysicalDevice physicalDevice,
    uint32_t typeFilter,
    VkMemoryPropertyFlags properties
)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i))
            && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}

void Quarkolor::createVirtualFrameBuffers(RenderContext& ctx)
{
    DEBUG("Creating framebuffers..");
    // iterate through image views and create framebuffers
    if (ctx.renderPass == VK_NULL_HANDLE) {
        FATAL("Render pass is null!");
    }
    size_t numFrameBuffers = ctx.swapchain->image.size();
    ASSERT(numFrameBuffers != 0);
    VirtualFrameBuffer& vfb = ctx.virtualFrameBuffer;
    vfb.frameBuffer.resize(numFrameBuffers);
    vfb.image.resize(numFrameBuffers);
    vfb.imageView.resize(numFrameBuffers);
    vfb.imageMemory.resize(numFrameBuffers); // Add this line for image memory
    SwapChainContext& swapchainContext = *ctx.swapchain;
    for (size_t i = 0; i < numFrameBuffers; i++) {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = swapchainContext.extent.width;
        imageInfo.extent.height = swapchainContext.extent.height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = swapchainContext.imageFormat;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(_device->logicalDevice, &imageInfo, nullptr, &vfb.image[i])
            != VK_SUCCESS) {
            FATAL("Failed to create custom image!");
        }

        // Allocate memory for the image
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(_device->logicalDevice, vfb.image[i], &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(
            _device->physicalDevice,
            memRequirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        if (vkAllocateMemory(_device->logicalDevice, &allocInfo, nullptr, &vfb.imageMemory[i])
            != VK_SUCCESS) {
            FATAL("Failed to allocate image memory!");
        }

        vkBindImageMemory(_device->logicalDevice, vfb.image[i], vfb.imageMemory[i], 0);

        // Create image view
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = vfb.image[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = swapchainContext.imageFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(_device->logicalDevice, &viewInfo, nullptr, &vfb.imageView[i])
            != VK_SUCCESS) {
            FATAL("Failed to create custom image view!");
        }
        VkImageView attachments[] = {vfb.imageView[i], swapchainContext.depthImageView};
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = ctx.renderPass; // each framebuffer is associated with a
                                                     // render pass; they need to be compatible
                                                     // i.e. having same number of attachments
                                                     // and same formats
        framebufferInfo.attachmentCount = sizeof(attachments) / sizeof(VkImageView);
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapchainContext.extent.width;
        framebufferInfo.height = swapchainContext.extent.height;
        framebufferInfo.layers = 1; // number of layers in image arrays
        if (vkCreateFramebuffer(
                _device->logicalDevice, &framebufferInfo, nullptr, &vfb.frameBuffer[i]
            )
            != VK_SUCCESS) {
            FATAL("Failed to create framebuffer!");
        }
    }
}

void Quarkolor::clearVirtualFrameBuffers(RenderContext& ctx)
{
    size_t numFrameBuffers = ctx.swapchain->frameBuffer.size();
    VirtualFrameBuffer& vfb = ctx.virtualFrameBuffer;
    for (size_t i = 0; i < numFrameBuffers; i++) {
        vkDestroyFramebuffer(_device->logicalDevice, vfb.frameBuffer[i], NULL);
        vkDestroyImageView(_device->logicalDevice, vfb.imageView[i], NULL);
        vkDestroyImage(_device->logicalDevice, vfb.image[i], NULL);
        vkFreeMemory(_device->logicalDevice, vfb.imageMemory[i], NULL);
    }
}

void Quarkolor::createSwapchainFrameBuffers(SwapChainContext& ctx, VkRenderPass rgbOrCmyPass)
{
    DEBUG("Creating framebuffers..");
    // iterate through image views and create framebuffers
    for (size_t i = 0; i < ctx.image.size(); i++) {
        VkImageView attachments[] = {ctx.imageView[i], ctx.depthImageView};
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        // NOTE: framebuffer DOES NOT need to have a dedicated render pass,
        // just need to be compatible. Therefore we pass either RGB&CMY pass
        framebufferInfo.renderPass = rgbOrCmyPass;
        framebufferInfo.attachmentCount = sizeof(attachments) / sizeof(VkImageView);
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = ctx.extent.width;
        framebufferInfo.height = ctx.extent.height;
        framebufferInfo.layers = 1; // number of layers in image arrays
        if (vkCreateFramebuffer(
                _device->logicalDevice, &framebufferInfo, nullptr, &ctx.frameBuffer[i]
            )
            != VK_SUCCESS) {
            FATAL("Failed to create framebuffer!");
        }
    }
}

void Quarkolor::createDepthBuffer(SwapChainContext& ctx)
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

void Quarkolor::flushEngineUBOStatic(uint8_t frame)
{
    VQBuffer& buf = _engineUBOStatic[frame];
    EngineUBOStatic ubo{
        _mainCamera.GetViewMatrix() // view
        // proj
    };
    getMainProjectionMatrix(ubo.proj);
    ubo.timeSinceStartSeconds = _timeSinceStartSeconds;
    ubo.sinWave = (sin(_timeSinceStartSeconds) + 1) / 2.f; // offset to [0, 1]
    ubo.isEvenFrame = isEvenFrame();
    memcpy(buf.bufferAddress, &ubo, sizeof(ubo));
}

void Quarkolor::drawFrame(TickContext* ctx, uint8_t frame)
{
    SyncPrimitives& sync = _syncProjector[frame];

    //  Wait for the previous frame to finish
    PROFILE_SCOPE(&_profiler, "Render Tick");
    vkWaitForFences(_device->logicalDevice, 1, &sync.fenceInFlight, VK_TRUE, UINT64_MAX);

    //  Acquire an image from the swap chain
    uint32_t swapchainImageIndex;
    VkResult result = vkAcquireNextImageKHR(
        this->_device->logicalDevice,
        _swapChain.chain,
        UINT64_MAX,
        sync.semaImageAvailable,
        VK_NULL_HANDLE,
        &swapchainImageIndex
    );
    [[unlikely]] if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        this->recreateSwapChain(_swapChain);
        recreateVirtualFrameBuffers();
        return;
    } else [[unlikely]] if (result != VK_SUCCESS) {
        const char* res = string_VkResult(result);
        PANIC("Failed to acquire swap chain image: {}", res);
    }

    // lock the fence
    vkResetFences(this->_device->logicalDevice, 1, &sync.fenceInFlight);

    vk::CommandBuffer CB1(_device->graphicsCommandBuffers[frame]);
    //  Record a command buffer which draws the scene onto that image
    CB1.reset();
    { // update ctx->graphics field
        ctx->graphics.currentFrameInFlight = frame;
        ctx->graphics.currentSwapchainImageIndex = swapchainImageIndex;
        ctx->graphics.CB = CB1;
        // ctx->graphics.currentFB = FB;
        ctx->graphics.currentFBextend = _swapChain.extent;
        getMainProjectionMatrix(ctx->graphics.mainProjectionMatrix);
    }

    { // begin command buffer
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;                  // Optional
        beginInfo.pInheritanceInfo = nullptr; // Optional
        CB1.begin(vk::CommandBufferBeginInfo());
    }

    { // main render pass
        vk::Extent2D extend = _swapChain.extent;
        vk::Rect2D renderArea(VkOffset2D{0, 0}, extend);
        vk::RenderPassBeginInfo renderPassBeginInfo(
            {}, {}, renderArea, _clearValues.size(), _clearValues.data(), nullptr
        );

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(extend.width);
        viewport.height = static_cast<float>(extend.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = extend;

        // two-pass rendering: render RGB and CMY colors onto two virtual FBs
        {
            renderPassBeginInfo.renderPass = _renderContexts.RGB.renderPass;
            renderPassBeginInfo.framebuffer
                = _renderContexts.RGB.virtualFrameBuffer
                      .frameBuffer[swapchainImageIndex]; // which frame buffer in the
                                                         // swapchain do the pass i.e.
                                                         // all draw calls render to?
            CB1.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
            vkCmdSetViewport(CB1, 0, 1, &viewport);
            vkCmdSetScissor(CB1, 0, 1, &scissor);
            _renderer.TickRGB(ctx);
            CB1.endRenderPass();

            drawImGui(ColorSpace::RGB);
            _imguiManager.RecordCommandBufferRGB(CB1, extend, swapchainImageIndex);
        }
        {
            renderPassBeginInfo.renderPass = _renderContexts.CMY.renderPass;
            renderPassBeginInfo.framebuffer
                = _renderContexts.CMY.virtualFrameBuffer
                      .frameBuffer[swapchainImageIndex]; // which frame buffer in the
                                                         // swapchain do the pass i.e.
                                                         // all draw calls render to?
            CB1.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
            vkCmdSetViewport(CB1, 0, 1, &viewport);
            vkCmdSetScissor(CB1, 0, 1, &scissor);
            _renderer.TickCMY(ctx);
            CB1.endRenderPass();

            drawImGui(ColorSpace::CMY);
            _imguiManager.RecordCommandBufferCMY(CB1, extend, swapchainImageIndex);
        }
    }

    CB1.end();

    VkSubmitInfo submitInfo{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO};

    VkSemaphore waitSemaphores[] = {}; // don't need semaphore since we're drawing on virtual fb
    VkPipelineStageFlags waitStages[]
        = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT}; // wait for color to be available
    std::array<VkCommandBuffer, 1> submitCommandBuffers = {CB1};

    submitInfo.waitSemaphoreCount = 0;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = static_cast<uint32_t>(submitCommandBuffers.size());
    submitInfo.pCommandBuffers = submitCommandBuffers.data();

    VkSemaphore signalSemaphores[] = {sync.semaRenderFinished};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(_device->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        FATAL("Failed to submit draw command buffer!");
    }

    vk::CommandBuffer CB2(_device->graphicsCommandBuffers2[frame]);
    CB2.reset();
    CB2.begin(vk::CommandBufferBeginInfo());

    // choose whether to render the even/odd frame buffer, discarding the other
    bool isEven = isEvenFrame();
    VkImage virtualFramebufferImage
        = isEven ? _renderContexts.RGB.virtualFrameBuffer.image[swapchainImageIndex]
                 : _renderContexts.CMY.virtualFrameBuffer.image[swapchainImageIndex];
    VkImage swapchainFramebufferImage = _swapChain.image[swapchainImageIndex];
    Utils::ImageTransfer::CmdCopyToFB(
        CB2, virtualFramebufferImage, swapchainFramebufferImage, _swapChain.extent
    );

    if (isEven != _evenOddDebugCtx.currShouldBeEven) {
        _evenOddDebugCtx.numDroppedFrames++;
    }
    _evenOddDebugCtx.currShouldBeEven = !isEven; // advance to next frame

    // virtual FB has been copied onto the physical FB, paint ImGui now.
    ctx->graphics.CB = CB2;
    CB2.end();

    // submit CB2
    VkSubmitInfo submitInfo2{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO};
    std::array<VkCommandBuffer, 1> submitCommandBuffers2 = {CB2};
    // for the image transfer, two semas need to be uppsed:
    // 1. the render from the previous CB has to finish for 2 virtual FBs to be available
    // 2. the actual FB needs to be available for copying
    VkSemaphore waitSemaphores2[] = {sync.semaImageAvailable, sync.semaRenderFinished};
    VkPipelineStageFlags waitStages2[] // both sema need to pass before the image transfer happens
        = {VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT};

    submitInfo2.waitSemaphoreCount = 2;
    submitInfo2.pWaitSemaphores = waitSemaphores2;
    submitInfo2.pWaitDstStageMask = waitStages2;
    submitInfo2.commandBufferCount = static_cast<uint32_t>(submitCommandBuffers2.size());
    submitInfo2.pCommandBuffers = submitCommandBuffers2.data();

    VkSemaphore signalSemaphores2[]
        = {sync.semaImageCopyFinished}; // signal this semaphore
                                        // for the presentation to happen
    submitInfo2.signalSemaphoreCount = 1;
    submitInfo2.pSignalSemaphores = signalSemaphores2;

    if (vkQueueSubmit(_device->graphicsQueue, 1, &submitInfo2, sync.fenceInFlight) != VK_SUCCESS) {
        FATAL("Failed to submit draw command buffer!");
    }

    //  Present the swap chain image
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    // wait for sync.semaRenderFinished upped by the previous render command
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores2; // semaImageCopyFinished, virtual fb
                                                     // is copied over to actual fb, can render

    VkSwapchainKHR swapChains[] = {_swapChain.chain};

    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &swapchainImageIndex; // specify which image to present
    presentInfo.pResults = nullptr;                   // Optional: can be used to check if
                                                      // presentation was successful
                                                      //

    uint64_t time = 0;
#if VIRTUAL_VSYNC
    if (_softwareEvenOddCtx.mostRecentPresentFinish) {
        time = _softwareEvenOddCtx.mostRecentPresentFinish
               + _softwareEvenOddCtx.nanoSecondsPerFrame * _softwareEvenOddCtx.vsyncFrameOffset;
    }
#endif // VIRTUAL_VSYNC

    // label each frame with the tick number
    // this is useful for calculating the virtual frame counter,
    // as we can take the max(frame.id) to get the number of presented frames.
    VkPresentTimeGOOGLE presentTime{(uint32_t)_numTicks, time};

    VkPresentTimesInfoGOOGLE presentTimeInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_TIMES_INFO_GOOGLE,
        .pNext = VK_NULL_HANDLE,
        .swapchainCount = 1,
        .pTimes = &presentTime
    };

    presentInfo.pNext = &presentTimeInfo;

    result = vkQueuePresentKHR(_device->presentationQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR
        || this->_framebufferResized) {
        ASSERT(
            _tetraMode != TetraMode::kEvenOddHardwareSync
        ); // exclusive window does not resize its swapchain
        this->recreateSwapChain(_swapChain);
        recreateVirtualFrameBuffers();
        this->_framebufferResized = false;
    }
}

void Quarkolor::drawImGui(ColorSpace colorSpace)
{
    if (!_wantToDrawImGui) {
        return;
    }
    // note that drawImGui is called twice per tick for RGB and CMY space,
    // so we need different profiler ID for them.
    const char* profileId = colorSpace == ColorSpace::RGB ? "ImGui Draw RGB" : "ImGui Draw CMY";
    PROFILE_SCOPE(&_profiler, profileId);
    _imguiManager.BeginImGuiContext();

    // imgui is associated with the glfw window to handle inputs,
    // but its actual fb is associated with the projector display;
    // so we need to manually re-adjust the display size for the scissors/
    // viewports/clipping to be consistent
    bool imguiDisplaySizeOverride = _tetraMode == TetraMode::kEvenOddHardwareSync;
    if (imguiDisplaySizeOverride) {
        ImVec2 projectorDisplaySize{
            static_cast<float>(_mainProjectorDisplay.extent.width),
            static_cast<float>(_mainProjectorDisplay.extent.height)
        };
        _imguiManager.forceDisplaySize(projectorDisplaySize);
    }

    if (!_lockCursor) {
        ImGuiU::DrawCenteredText("Press Tab to enable input", ImVec4(0, 0, 0, 0.8));
    } else if (_uiMode) {
        ImGuiIO& io = ImGui::GetIO();
        ImDrawList* drawList = ImGui::GetForegroundDrawList();

        ImVec2 mousePos = io.MousePos;

        ImU32 color = IM_COL32(255, 255, 255, 200);
        drawList->AddCircleFilled(mousePos, 10, color);
    }

    if (ImGui::Begin(DEFAULTS::Engine::APPLICATION_NAME)) {
        if (ImGui::BeginTabBar("Engine Tab")) {
            if (ImGui::BeginTabItem("General")) {
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
                if (_uiMode) {
                    ImGui::Text("UI Mode: Active");
                } else {
                    ImGui::Text("UI Mode: Deactive");
                }
                ImGui::SeparatorText("Engine UBO");
                _widgetUBOViewer.Draw(this, colorSpace);
                ImGui::SeparatorText("Graphics Pipeline");
                _widgetGraphicsPipeline.Draw(this, colorSpace);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Performance")) {
                _widgetPerfPlot.Draw(this, colorSpace);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Device")) {
                _widgetDeviceInfo.Draw(this, colorSpace);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Even-Odd")) {
                _widgetEvenOdd.Draw(this, colorSpace);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Tetra Viewer")) {
                _widgetTetraViewerDemo.Draw(this, colorSpace);
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar(); // Engine Tab
        }
    }

    ImGui::End();
    _imguiManager.EndImGuiContext();
}

// FIXME: glfw calls from a differnt thread; may need to add critical sections
// currently for perf reasons we're leaving it as is.
void Quarkolor::bindDefaultInputs()
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
    // ui mode toggle
    _inputManager.RegisterCallback(GLFW_KEY_U, InputManager::KeyCallbackCondition::PRESS, [this]() {
        _uiMode = !_uiMode;
        ImGuiIO& io = ImGui::GetIO();
        if (_uiMode) {
            io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
            io.ConfigFlags &= ~ImGuiConfigFlags_NoKeyboard;
            io.MousePos = ImVec2{
                static_cast<float>(_swapChain.extent.width) / 2,
                static_cast<float>(_swapChain.extent.height) / 2
            };
            io.WantSetMousePos = true;
        } else {
            io.ConfigFlags |= (ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoKeyboard);
        }
        // io.WantSetMousePos = _uiMode;
    });
    _inputManager.RegisterCallback(
        GLFW_KEY_ESCAPE, InputManager::KeyCallbackCondition::PRESS, [this]() {}
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

void Quarkolor::getMainProjectionMatrix(glm::mat4& projectionMatrix)
{
    auto& extent = _swapChain.extent;
    projectionMatrix = glm::perspective(
        glm::radians(_FOV),
        extent.width / static_cast<float>(extent.height),
        DEFAULTS::ZNEAR,
        DEFAULTS::ZFAR
    );
    projectionMatrix[1][1] *= -1; // invert for vulkan coord system
}

// whether to use the new virtual frame counter
// the old counter uses a nanosecond-precision timer,
// the new counter takes the max of the image id so far presented.
// TODO: profile precision of the new counter vs. old counter
#define NEW_VIRTUAL_FRAMECOUNTER 0

uint64_t Quarkolor::getSurfaceCounterValue()
{
    uint64_t surfaceCounter;
    switch (_tetraMode) {
    case TetraMode::kEvenOddSoftwareSync: {
        uint32_t imageCount;
#if NEW_VIRTUAL_FRAMECOUNTER
        vkGetPastPresentationTimingGOOGLE(
            _device->logicalDevice, _renderContexts.RGB.swapchain->chain, &imageCount, nullptr
        );
        std::vector<VkPastPresentationTimingGOOGLE> images(imageCount);

        vkGetPastPresentationTimingGOOGLE(
            _device->logicalDevice, _renderContexts.RGB.swapchain->chain, &imageCount, images.data()
        );
        for (int i = 0; i < imageCount; i++) {
            _softwareEvenOddCtx.lastPresentedImageId
                = std::max(_softwareEvenOddCtx.lastPresentedImageId, images.at(i).presentID);
            _softwareEvenOddCtx.mostRecentPresentFinish = std::max(
                _softwareEvenOddCtx.mostRecentPresentFinish, images.at(i).actualPresentTime
            );
            auto& img = images.at(i);
            // INFO(
            //     "{} : expected: {} actual: {}",
            //     img.presentID,
            //     img.desiredPresentTime,
            //     img.actualPresentTime
            // );
        }
        surfaceCounter = _softwareEvenOddCtx.lastPresentedImageId;
#else
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
        VK_CHECK_RESULT(_hardWareEvenOddCtx.vkGetSwapchainCounterEXT(
            _device->logicalDevice,
            _mainWindowSwapChain.chain,
            VkSurfaceCounterFlagBitsEXT::VK_SURFACE_COUNTER_VBLANK_EXT,
            &surfaceCounter
        ))
#endif
        break;
    default:
        surfaceCounter = 0;
    }

    return surfaceCounter;
}

bool Quarkolor::isEvenFrame()
{
    bool isEven = getSurfaceCounterValue() % 2 == 0;

    if (_flipEvenOdd) {
        isEven = !isEven;
    }

    return isEven;
}