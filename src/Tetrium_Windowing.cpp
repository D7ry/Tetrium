// direct display utilities
#ifdef __linux__
// clang-format off
#include "X11/Xlib.h"
#include <X11/extensions/Xrandr.h>
#include "vulkan/vulkan_xlib.h"
#include "vulkan/vulkan_xlib_xrandr.h"
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <fcntl.h> // `O_RDWR`
// clang-foramt on
#endif

#include <iostream>

#include "Tetrium.h"

#if __linux__
// select exclusive display using DRM
void Tetrium::selectDisplayDRM(DisplayContext& ctx)
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
void Tetrium::selectDisplayXlib(DisplayContext& ctx)
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

    if (displays.empty()) {
        PANIC("No external display fonud! Make sure to have GPU connected to at least one external "
              "display.");
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

    VkResult result = vkAcquireXlibDisplayEXT(device, xDisplay, ctx.display);
    if (result != VK_SUCCESS) {
        PANIC("Failed to find acquire exclusive access to xlib display, make sure to disable "
              "display in OS and graphics drivers.");
    }
}
#endif // __linux__



// take complete control over a physical display
// the display must be directly connected to the GPU through x11
// prompts the user to select the display and resolution in an ImGui window
void Tetrium::initExclusiveDisplay(Tetrium::DisplayContext& ctx)
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
    VkExtent2D extent = modeProperties[selectedModeIndex].parameters.visibleRegion;
#define FLIP_VERTICAL_HORIZONTAL 0 // flip extent for portrait mode
#if FLIP_VERTICAL_HORIZONTAL
    std::swap(extent.width, extent.height);
#endif // FLIP_VERTICAL_HORIZONTAL

    // Create display surface
    VkDisplaySurfaceCreateInfoKHR surfaceCreateInfo = {};
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.displayMode = displayMode;
    surfaceCreateInfo.planeIndex = ctx.planeIndex;
    surfaceCreateInfo.planeStackIndex = stackIndex;
    surfaceCreateInfo.transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    surfaceCreateInfo.globalAlpha = 1.0f;
    surfaceCreateInfo.alphaMode = VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR;
    surfaceCreateInfo.imageExtent = extent;

    VK_CHECK_RESULT(
        vkCreateDisplayPlaneSurfaceKHR(_instance, &surfaceCreateInfo, nullptr, &ctx.surface)
    );
    ASSERT(ctx.surface != VK_NULL_HANDLE);

    _deletionStack.push([this, ctx]() { vkDestroySurfaceKHR(_instance, ctx.surface, nullptr); });

    // Store the display properties for later use
    ctx.extent = extent;

    ctx.refreshrate = modeProperties[selectedModeIndex].parameters.refreshRate;
    DEBUG("display extent: {} {}", ctx.extent.width, ctx.extent.height);
}

// in CLI pop up a monitor selection interface, that lists
// monitor names and properties
// the user would input a number to select the right monitor.
std::pair<GLFWmonitor*, GLFWvidmode> Tetrium::cliMonitorModeSelection()
{
    std::cout << "---------- Please Select Monitor and Video Mode ----------" << std::endl;

    int numMonitors;
    GLFWmonitor** monitors = glfwGetMonitors(&numMonitors);
    if (!monitors || numMonitors == 0) {
        PANIC("No monitor detected!");
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

VkSurfaceKHR Tetrium::createGlfwWindowSurface(GLFWwindow* window)
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

void Tetrium::initGLFW(const InitOptions& options)
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

    // software sync pops up a monitor selection window for full-screen
    // glfw window selection
    // hardware sync skips the step; the GLFW window is in windowed mode 
    // and is used only as a controller window
    if (options.tetraMode == TetraMode::kEvenOddSoftwareSync) {
        auto ret = cliMonitorModeSelection();
        monitor = ret.first;
        auto mode = ret.second;
        glfwWindowHint(GLFW_RED_BITS, mode.redBits);
        glfwWindowHint(GLFW_GREEN_BITS, mode.greenBits);
        glfwWindowHint(GLFW_BLUE_BITS, mode.blueBits);
        glfwWindowHint(GLFW_REFRESH_RATE, mode.refreshRate);
        width = mode.width;
        height = mode.height;
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
