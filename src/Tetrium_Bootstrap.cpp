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
#include "components/VulkanUtils.h" // FIXME: this shouldn't be here
#include "imgui.h"

// Molten VK Config
#if __APPLE__
#include "MoltenVKConfig.h"
#endif // __APPLE__

// ecs
#include "ecs/component/TransformComponent.h"

#include "Tetrium.h"

#if __linux__
#endif // __linux__

#define SCHEDULE_DELETE(...) this->_deletionStack.push([this]() { __VA_ARGS__ });

#define VIRTUAL_VSYNC 0

// creates a cow for now
void Tetrium::createFunnyObjects()
{
    // lil cow
    // Entity* spot = new Entity("Spot");
    //
    // std::string textures[ColorSpaceSize]
    //     = {(DIRECTORIES::ASSETS + "textures/spot.png"), (DIRECTORIES::ASSETS +
    //     "textures/spot.png")
    //     };

    // auto meshInstance
    //     = _renderer.MakeMeshInstanceComponent(DIRECTORIES::ASSETS + "models/spot.obj", textures);

    // give the lil cow a mesh
    // spot->AddComponent(meshInstance);
    // give the lil cow a transform
    // spot->AddComponent(new TransformComponent());
    // _renderer.AddEntity(spot);
    // spot->GetComponent<TransformComponent>()->rotation.x = 90;
    // spot->GetComponent<TransformComponent>()->rotation.y = 90;
    // spot->GetComponent<TransformComponent>()->position.z = 0.05;
    // register lil cow
}

void Tetrium::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    Tetrium* pThis = reinterpret_cast<Tetrium*>(glfwGetWindowUserPointer(window));
    if (key == GLFW_KEY_P && action == GLFW_PRESS) {
        _paused = !_paused;
    }
    _inputManager.OnKeyInput(window, key, scancode, action, mods);

    // toggle cursor lock
    if (key == GLFW_KEY_TAB && action == GLFW_PRESS) {
        _windowFocused = !_windowFocused;
        if (_windowFocused) {
            glfwSetInputMode(_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        } else {
            glfwSetInputMode(_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
        // only activate input on cursor lock
        _inputManager.SetActive(_windowFocused);
    }
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(_window, GLFW_TRUE);
    }
}

void Tetrium::cursorPosCallback(GLFWwindow* window, double xpos, double ypos)
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
    if (_windowFocused && !_uiMode) {
        _mainCamera.ModRotation(deltaX, deltaY, 0);
    }
    prevX = xpos;
    prevY = ypos;
}

void Tetrium::initDefaultStates()
{
    // configure states

    // clear color
    _clearValues[0].color = {0.0f, 0.0f, 0.0f, 1.f};
    _clearValues[1].depthStencil = vk::ClearDepthStencilValue(1.f, 0.f);

    // camera location
    _mainCamera.SetPosition(-2, 0, 0);

    // input states
    // by default, unlock cursor, disable imgui inputs, disable input handling
    _windowFocused = false;
    _inputManager.SetActive(_windowFocused);
    _uiMode = false;

    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoKeyboard;
};

void Tetrium::Init(const Tetrium::InitOptions& options)
{
    // populate static config fields
    _tetraMode = options.tetraMode;
    if (_tetraMode == TetraMode::kDualProjector) {
        NEEDS_IMPLEMENTATION();
    }
#if __APPLE__
    MoltenVKConfig::Setup();
#endif // __APPLE__
    _window = initGLFW(options.tetraMode == TetraMode::kEvenOddSoftwareSync);
    glfwSetWindowUserPointer(_window, this);
    SCHEDULE_DELETE(glfwDestroyWindow(_window); glfwTerminate();)

    { // Input Handling
        auto keyCallback = [](GLFWwindow* window, int key, int scancode, int action, int mods) {
            Tetrium* pThis = reinterpret_cast<Tetrium*>(glfwGetWindowUserPointer(window));
            pThis->keyCallback(window, key, scancode, action, mods);
        };
        glfwSetKeyCallback(this->_window, keyCallback);

        auto cursorPosCallback = [](GLFWwindow* window, double xpos, double ypos) {
            Tetrium* pThis = reinterpret_cast<Tetrium*>(glfwGetWindowUserPointer(window));
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

    if (_tetraMode == TetraMode::kEvenOddHardwareSync
        || _tetraMode == TetraMode::kEvenOddSoftwareSync) {
        initEvenOdd();
    } else {
        NEEDS_IMPLEMENTATION()
    }

    // init applications
    InitContext initCtx;
    { // populate initData
        initCtx.device = this->_device.get();
        initCtx.textureManager = &_textureManager;
        initCtx.swapChainImageFormat = _swapChain.imageFormat;
        initCtx.rygbRenderPass = _renderContextRYGB.renderPass;
        initCtx.rocvTransformRenderPass = _rocvTransformRenderPass;
        for (int i = 0; i < _engineUBOStatic.size(); i++) {
            initCtx.engineUBOStaticDescriptorBufferInfo[i].range = sizeof(EngineUBOStatic);
            initCtx.engineUBOStaticDescriptorBufferInfo[i].buffer = _engineUBOStatic[i].buffer;
            initCtx.engineUBOStaticDescriptorBufferInfo[i].offset = 0;
        }
    }

    // _rgbyRenderers.imageDisplay.Init(&initCtx);
    // _deletionStack.push([this]() { _rgbyRenderers.imageDisplay.Cleanup(); });
    // _rgbyRenderers.imageDisplay.LoadTexture("../assets/textures/spot.png"); // just for testing

    initRYGB2ROCVTransform(&initCtx);
    SCHEDULE_DELETE(cleanupRYGB2ROCVTransform();)

    initDefaultStates();

    createFunnyObjects();

    _soundManager.LoadAllSounds();
    _soundManager.PlaySound(SoundManager::Sound::kProgramStart);
}

void Tetrium::framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    DEBUG("Window resized to {}x{}", width, height);
    auto app = reinterpret_cast<Tetrium*>(glfwGetWindowUserPointer(window));
    app->_framebufferResized = true;
}

void Tetrium::createDevice()
{
    VkPhysicalDevice physicalDevice = this->pickPhysicalDevice();
    this->_device = std::make_shared<VQDevice>(physicalDevice);
    this->_deletionStack.push([this]() { this->_device->Cleanup(); });
}

void Tetrium::initVulkan()
{
    VkSurfaceKHR mainWindowSurface = VK_NULL_HANDLE;
    INFO("Initializing Vulkan...");
    _instance = createInstance();
    SCHEDULE_DELETE(vkDestroyInstance(this->_instance, nullptr);)
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

    // set up context for RYGB off-screen rendering
    _renderContextRYGB.renderPass = createRenderPass(
        _device->logicalDevice,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        _swapChain.imageFormat,
        VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE,
        true
    );
    createVirtualFrameBuffer(
        _renderContextRYGB.renderPass, _swapChain, _renderContextRYGB.virtualFrameBuffer, _swapChain.numImages
    );
    SCHEDULE_DELETE(clearVirtualFrameBuffer(_renderContextRYGB.virtualFrameBuffer);)

    // set up render pass for rocv transfer
    // the rocv transfer pass directly paints onto swapchain's FB
    _rocvTransformRenderPass = createRenderPass(
        _device->logicalDevice,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // ImGui pass runs after
        //VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        _swapChain.imageFormat,
        VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE,
        true,
        VkSubpassDependency{
            .srcSubpass = VK_SUBPASS_EXTERNAL, // all previous submitted subpass, in this case it's
                                               // `_renderContextRYGB.renderPass`
            .dstSubpass = 0,                   // first subpass
            // wait until RYGB pass to paint to the FB to start up fragment shader that handles
            // RGYB -> RGB/OCV transform
            // https://www.reddit.com/r/vulkan/comments/s80reu/subpass_dependencies_what_are_those_and_why_do_i/
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT
        }
    );

    // create framebuffer for swapchain
    createSwapchainFrameBuffers(_swapChain, _rocvTransformRenderPass);
    _deletionStack.push([this] { cleanupSwapChain(_swapChain); });

    this->createSynchronizationObjects(_syncProjector);

    initImGuiRenderContext(_imguiCtx);
    this->_deletionStack.push([this]() { destroyImGuiContext(_imguiCtx); });

    INFO("Vulkan initialized.");
}

bool Tetrium::checkValidationLayerSupport()
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

VkInstance Tetrium::createInstance()
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
        for (const char* evenOddExtensionName : EVEN_ODD_HARDWARE_INSTANCE_EXTENSIONS) {
            instanceExtensions.push_back(evenOddExtensionName);
        }
        break;
    default:
        break;
    }

    INFO("Instance extensions:");
    for (const char* ext : instanceExtensions) {
        INFO("{}", ext);
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

    VkInstance instance;
    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);

    if (result != VK_SUCCESS) {
        FATAL("Failed to create Vulkan instance; Result: {}", string_VkResult(result));
    }

    INFO("Vulkan instance created.");
    return instance;
}

void Tetrium::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
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

void Tetrium::setupDebugMessenger()
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

VKAPI_ATTR VkBool32 VKAPI_CALL Tetrium::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData
)
{
    DEBUG("{}", pCallbackData->pMessage);
    return VK_FALSE;
}

bool Tetrium::checkDeviceExtensionSupport(VkPhysicalDevice device)
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

bool Tetrium::isDeviceSuitable(VkPhysicalDevice device)
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

const std::vector<const char*> Tetrium::getRequiredDeviceExtensions() const
{
    std::vector<const char*> extensions = DEFAULT_DEVICE_EXTENSIONS;
    if (_tetraMode == TetraMode::kEvenOddHardwareSync) {
        for (auto extension : EVEN_ODD_HARDWARE_DEVICE_EXTENSIONS) {
            extensions.push_back(extension);
        }
    }
    if (_tetraMode == TetraMode::kEvenOddSoftwareSync) {
        for (auto extension : EVEN_ODD_SOFTWARE_DEVICE_EXTENSIONS) {
            extensions.push_back(extension);
        }
    }
    return extensions;
}

// pick a physical device that satisfies `isDeviceSuitable()`
VkPhysicalDevice Tetrium::pickPhysicalDevice()
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

void Tetrium::createSwapChain(Tetrium::SwapChainContext& ctx, const VkSurfaceKHR surface)
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
    ctx.numImages = imageCount;
    DEBUG("Swap chain created!");
}

void Tetrium::cleanupSwapChain(SwapChainContext& ctx)
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

void Tetrium::recreateVirtualFrameBuffers()
{
    // FIXME: add RYGB framebuffer recreation

    // imgui's fb are associated with render contexts, so initialize them here
    reinitImGuiFrameBuffers(_imguiCtx);
}

void Tetrium::recreateSwapChain(SwapChainContext& ctx)
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
    this->createSwapchainFrameBuffers(ctx, _rocvTransformRenderPass);
    DEBUG("Swap chain recreated.");
}

VkSurfaceFormatKHR Tetrium::chooseSwapSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& availableFormats
)
{
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB
            && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            INFO("chosen B8G8R8A8 swapchain format");
            return availableFormat;
        }
    }

    return availableFormats[0];
}

VkPresentModeKHR Tetrium::chooseSwapPresentMode(
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

VkExtent2D Tetrium::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
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

void Tetrium::createImageViews(SwapChainContext& ctx)
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

void Tetrium::createSynchronizationObjects(
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
        VK_CHECK_RESULT(vkCreateFence(
            _device->logicalDevice, &fenceInfo, nullptr, &primitive.fenceRenderFinished
        ));

        // create vsync semahore as a timeline semaphore
        // VkSemaphoreTypeCreateInfo timelineCreateInfo{
        //     .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        //     .pNext = NULL,
        //     .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
        //     .initialValue = 0,
        // };
        // semaphoreInfo.pNext = &timelineCreateInfo;
        // VK_CHECK_RESULT(
        //     vkCreateSemaphore(_device->logicalDevice, &semaphoreInfo, nullptr,
        //     &primitive.semaVsync)
        // );
        semaphoreInfo.pNext = nullptr; // reset for next loop
    }
    this->_deletionStack.push([this, primitives]() {
        for (size_t i = 0; i < NUM_FRAME_IN_FLIGHT; i++) {
            const SyncPrimitives& primitive = primitives[i];
            for (auto& sema : {// primitive.semaVsync,
                               primitive.semaImageCopyFinished,
                               primitive.semaRenderFinished,
                               primitive.semaImageAvailable
                 }) {
                vkDestroySemaphore(this->_device->logicalDevice, sema, nullptr);
            }
            vkDestroyFence(this->_device->logicalDevice, primitive.fenceInFlight, nullptr);
            vkDestroyFence(this->_device->logicalDevice, primitive.fenceRenderFinished, nullptr);
        }
    });
}

void Tetrium::Cleanup()
{
    INFO("Cleaning up...");
    _deletionStack.flush();
    INFO("Resource cleaned up.");
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

void Tetrium::createVirtualFrameBuffer(
    VkRenderPass renderPass,
    const SwapChainContext& swapChain,
    VirtualFrameBuffer& vfb,
    uint32_t numFrameBuffers
)
{
    DEBUG("Creating framebuffers..");
    ASSERT(renderPass != VK_NULL_HANDLE);
    ASSERT(swapChain.numImages != 0);

    ASSERT(numFrameBuffers != 0);
    vfb.frameBuffer.resize(numFrameBuffers);
    vfb.image.resize(numFrameBuffers);
    vfb.imageView.resize(numFrameBuffers);
    vfb.imageMemory.resize(numFrameBuffers); // Add this line for image memory
    for (size_t i = 0; i < numFrameBuffers; i++) {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = swapChain.extent.width;
        imageInfo.extent.height = swapChain.extent.height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = swapChain.imageFormat;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
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
        viewInfo.format = swapChain.imageFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(_device->logicalDevice, &viewInfo, nullptr, &vfb.imageView[i])
            != VK_SUCCESS) {
            FATAL("Failed to create custom image view!");
        }
        VkImageView attachments[] = {vfb.imageView[i], swapChain.depthImageView};
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass; // each framebuffer is associated with a
                                                 // render pass; they need to be compatible
                                                 // i.e. having same number of attachments
                                                 // and same formats
        framebufferInfo.attachmentCount = sizeof(attachments) / sizeof(VkImageView);
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapChain.extent.width;
        framebufferInfo.height = swapChain.extent.height;
        framebufferInfo.layers = 1; // number of layers in image arrays
        if (vkCreateFramebuffer(
                _device->logicalDevice, &framebufferInfo, nullptr, &vfb.frameBuffer[i]
            )
            != VK_SUCCESS) {
            FATAL("Failed to create framebuffer!");
        }
    }
}

void Tetrium::clearVirtualFrameBuffer(VirtualFrameBuffer& vfb)
{
    int numFrameBuffers = vfb.frameBuffer.size();
    ASSERT(vfb.imageView.size() == numFrameBuffers);
    ASSERT(vfb.frameBuffer.size() == numFrameBuffers);
    ASSERT(vfb.image.size() == numFrameBuffers);
    ASSERT(vfb.imageMemory.size() == numFrameBuffers);

    for (size_t i = 0; i < numFrameBuffers; i++) {
        vkDestroyFramebuffer(_device->logicalDevice, vfb.frameBuffer[i], NULL);
        vkDestroyImageView(_device->logicalDevice, vfb.imageView[i], NULL);
        vkDestroyImage(_device->logicalDevice, vfb.image[i], NULL);
        vkFreeMemory(_device->logicalDevice, vfb.imageMemory[i], NULL);
    }
}

void Tetrium::createSwapchainFrameBuffers(SwapChainContext& ctx, VkRenderPass renderPass)
{
    DEBUG("Creating framebuffers..");
    // iterate through image views and create framebuffers
    for (size_t i = 0; i < ctx.image.size(); i++) {
        VkImageView attachments[] = {ctx.imageView[i], ctx.depthImageView};
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        // NOTE: framebuffer DOES NOT need to have a dedicated render pass,
        // just need to be compatible. Therefore we pass either RGB&OCV pass
        framebufferInfo.renderPass = renderPass;
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

void Tetrium::createDepthBuffer(SwapChainContext& ctx)
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

// FIXME: glfw calls from a differnt thread; may need to add critical sections
// currently for perf reasons we're leaving it as is.
void Tetrium::bindDefaultInputs()
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
                clearImGuiDrawData();
            }
        }
    );
}

// https://www.saschawillems.de/blog/2018/07/19/vulkan-input-attachments-and-sub-passes/
VkRenderPass Tetrium::createRenderPass(
    VkDevice logicalDevice,
    VkImageLayout initialLayout,
    VkImageLayout finalLayout,
    VkFormat colorFormat,
    VkAttachmentLoadOp colorLoadOp,
    VkAttachmentStoreOp colorStoreOp,
    bool createDepthAttachment,
    VkSubpassDependency dependency
)
{
    INFO("Creating render pass...");

    // set up color/depth layout
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // depth attachment ref for subpass, only used when `createDepthAttachment` is true
    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    if (createDepthAttachment) {
        subpass.pDepthStencilAttachment = &depthAttachmentRef;
    }

    // color, and optionally depth attachments
    VkAttachmentDescription attachments[2];
    uint32_t attachmentCount = 1; // by default only have color attachment
    // create color attachment
    {
        VkAttachmentDescription& colorAttachment = attachments[0];
        colorAttachment = {};
        colorAttachment.format = colorFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;

        colorAttachment.loadOp = colorLoadOp;
        colorAttachment.storeOp = colorStoreOp;

        // don't care about stencil
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

        colorAttachment.initialLayout = initialLayout;
        colorAttachment.finalLayout = finalLayout;
    }
    // add in depth attachment when `createDepthAttachment` is set to true
    // we have already specified above to have depthAttachmentRef for subpass creation
    if (createDepthAttachment) {
        VkAttachmentDescription& depthAttachment = attachments[1];
        depthAttachment = {};
        depthAttachment = VkAttachmentDescription{};
        depthAttachment.format = VulkanUtils::findDepthFormat(_device->physicalDevice);
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachmentCount = 2; // update attachment count
    }

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.pAttachments = attachments;
    renderPassInfo.attachmentCount = attachmentCount;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    VkRenderPass renderPass;
    if (vkCreateRenderPass(logicalDevice, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        FATAL("Failed to create render pass!");
    }
    INFO("render pass created");
    return renderPass;
}
