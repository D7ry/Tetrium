// ImGui initialization and resource management
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"
#include "implot.h"
#include "misc/freetype/imgui_freetype.h"

#include "Tetrium.h"

namespace Tetrium_ImGui
{
// context arrays that gets populated after Tetrium::initImGuiContext() is called
// stores the same data as `Tetrium::_imguiCtx.ctxImGui` and `Tetrium::_imguiCtx.ctxImPlot`
static ImGuiContext* ctxImGui[ColorSpace::ColorSpaceSize] = {};
static ImPlotContext* ctxImPlot[ColorSpace::ColorSpaceSize] = {};

namespace GLFW
{
struct
{
    GLFWwindowfocusfun WindowFocus = nullptr;
    GLFWcursorenterfun CursorEnter = nullptr;
    GLFWcursorposfun CursorPos = nullptr;
    GLFWmousebuttonfun MouseButton = nullptr;
    GLFWscrollfun Scroll = nullptr;
    GLFWkeyfun Key = nullptr;
    GLFWcharfun Char = nullptr;
    GLFWmonitorfun Monitor = nullptr;
} prevCallbacks;

void ImGuiCustomWindowFocusCallback(GLFWwindow* window, int focused)
{
    for (int i = 0; i < ColorSpace::ColorSpaceSize; ++i) {
        ImGui::SetCurrentContext(ctxImGui[i]);
        ImPlot::SetCurrentContext(ctxImPlot[i]);
        ImGui_ImplGlfw_WindowFocusCallback(window, focused);
    }

    if (prevCallbacks.WindowFocus)
        prevCallbacks.WindowFocus(window, focused);
}

void ImGuiCustomCursorEnterCallback(GLFWwindow* window, int entered)
{
    for (int i = 0; i < ColorSpace::ColorSpaceSize; ++i) {
        ImGui::SetCurrentContext(ctxImGui[i]);
        ImPlot::SetCurrentContext(ctxImPlot[i]);
        ImGui_ImplGlfw_CursorEnterCallback(window, entered);
    }

    if (prevCallbacks.CursorEnter)
        prevCallbacks.CursorEnter(window, entered);
}

void ImGuiCustomCursorPosCallback(GLFWwindow* window, double xpos, double ypos)
{
    for (int i = 0; i < ColorSpace::ColorSpaceSize; ++i) {
        ImGui::SetCurrentContext(ctxImGui[i]);
        ImPlot::SetCurrentContext(ctxImPlot[i]);
        ImGui_ImplGlfw_CursorPosCallback(window, xpos, ypos);
    }

    if (prevCallbacks.CursorPos)
        prevCallbacks.CursorPos(window, xpos, ypos);
}

void ImGuiCustomMouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    for (int i = 0; i < ColorSpace::ColorSpaceSize; ++i) {
        ImGui::SetCurrentContext(ctxImGui[i]);
        ImPlot::SetCurrentContext(ctxImPlot[i]);
        ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
    }

    if (prevCallbacks.MouseButton)
        prevCallbacks.MouseButton(window, button, action, mods);
}

void ImGuiCustomScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    for (int i = 0; i < ColorSpace::ColorSpaceSize; ++i) {
        ImGui::SetCurrentContext(ctxImGui[i]);
        ImPlot::SetCurrentContext(ctxImPlot[i]);
        ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset);
    }

    if (prevCallbacks.Scroll)
        prevCallbacks.Scroll(window, xoffset, yoffset);
}

void ImGuiCustomKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    for (int i = 0; i < ColorSpace::ColorSpaceSize; ++i) {
        ImGui::SetCurrentContext(ctxImGui[i]);
        ImPlot::SetCurrentContext(ctxImPlot[i]);
        ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
    }

    if (prevCallbacks.Key)
        prevCallbacks.Key(window, key, scancode, action, mods);
}

void ImGuiCustomCharCallback(GLFWwindow* window, unsigned int c)
{
    for (int i = 0; i < ColorSpace::ColorSpaceSize; ++i) {
        ImGui::SetCurrentContext(ctxImGui[i]);
        ImPlot::SetCurrentContext(ctxImPlot[i]);
        ImGui_ImplGlfw_CharCallback(window, c);
    }

    if (prevCallbacks.Char)
        prevCallbacks.Char(window, c);
}

void ImGuiCustomMonitorCallback(GLFWmonitor* monitor, int event)
{
    for (int i = 0; i < ColorSpace::ColorSpaceSize; ++i) {
        ImGui::SetCurrentContext(ctxImGui[i]);
        ImPlot::SetCurrentContext(ctxImPlot[i]);
        ImGui_ImplGlfw_MonitorCallback(monitor, event);
    }

    if (prevCallbacks.Monitor)
        prevCallbacks.Monitor(monitor, event);
}

} // namespace GLFW

// Set up custom callback functions that invokes on both RGB
// and OCV imgui contexts; Naively binding all callbacks through
// GLFW doesn't work, as the callbacks do not handle context switching.
void setupCustomCallbacks(GLFWwindow* window)
{
    // Store previous callbacks and set new ones
    GLFW::prevCallbacks.WindowFocus
        = glfwSetWindowFocusCallback(window, GLFW::ImGuiCustomWindowFocusCallback);
    GLFW::prevCallbacks.CursorEnter
        = glfwSetCursorEnterCallback(window, GLFW::ImGuiCustomCursorEnterCallback);
    GLFW::prevCallbacks.CursorPos
        = glfwSetCursorPosCallback(window, GLFW::ImGuiCustomCursorPosCallback);
    GLFW::prevCallbacks.MouseButton
        = glfwSetMouseButtonCallback(window, GLFW::ImGuiCustomMouseButtonCallback);
    GLFW::prevCallbacks.Scroll = glfwSetScrollCallback(window, GLFW::ImGuiCustomScrollCallback);
    GLFW::prevCallbacks.Key = glfwSetKeyCallback(window, GLFW::ImGuiCustomKeyCallback);
    GLFW::prevCallbacks.Char = glfwSetCharCallback(window, GLFW::ImGuiCustomCharCallback);
    GLFW::prevCallbacks.Monitor = glfwSetMonitorCallback(GLFW::ImGuiCustomMonitorCallback);
}

VkRenderPass createRenderPass(
    VkDevice logicalDevice,
    VkFormat swapChainImageFormat,
    VkImageLayout initialLayout,
    VkImageLayout finalLayout
)
{
    INFO("Creating render pass...");
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;

    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; // load from previou pass
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    // don't care about stencil
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    colorAttachment.initialLayout = initialLayout;
    colorAttachment.finalLayout = finalLayout;

    // set up subpass
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = initialLayout;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    // dependency to make sure that the render pass waits for the image to be
    // available before drawing
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    VkRenderPass renderPass;
    if (vkCreateRenderPass(logicalDevice, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        FATAL("Failed to create render pass!");
    }
    return renderPass;
}

void InitializeFrameBuffer(
    VkDevice device,
    VkExtent2D extent,
    VkRenderPass renderPass,
    int bufferCount,
    const std::vector<VkImageView>& swapChainImageViewsRGB,
    const std::vector<VkImageView>& swapChainImageViewsOCV,
    std::vector<VkFramebuffer>& framebufferRGB,
    std::vector<VkFramebuffer>& framebufferOCV
)
{
    DEBUG("Creating imgui frame buffers...");
    ASSERT(swapChainImageViewsRGB.size() == bufferCount);
    ASSERT(swapChainImageViewsOCV.size() == bufferCount);

    framebufferRGB.resize(bufferCount);
    framebufferOCV.resize(bufferCount);
    VkImageView attachment[1];
    VkFramebufferCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    info.renderPass = renderPass, info.attachmentCount = 1;
    info.pAttachments = attachment;
    info.width = extent.width;
    info.height = extent.height;
    info.layers = 1;

    for (uint32_t i = 0; i < bufferCount; i++) {
        attachment[0] = swapChainImageViewsRGB[i];
        VK_CHECK_RESULT(vkCreateFramebuffer(device, &info, nullptr, &framebufferRGB[i]));
        attachment[0] = swapChainImageViewsOCV[i];
        VK_CHECK_RESULT(vkCreateFramebuffer(device, &info, nullptr, &framebufferOCV[i]));
    }
    DEBUG("Imgui frame buffers created.");
}

VkDescriptorPool createDescriptorPool(int poolSize, VkDevice logicalDevice)
{
    ASSERT(poolSize >= NUM_FRAME_IN_FLIGHT);
    // create a pool that will allocate to actual descriptor sets
    uint32_t descriptorSetCount = static_cast<uint32_t>(poolSize);
    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, descriptorSetCount}, // image sampler for imgui
        {VK_DESCRIPTOR_TYPE_SAMPLER, descriptorSetCount}                 // sampler for imgui
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount
        = sizeof(poolSizes) / sizeof(VkDescriptorPoolSize); // number of pool sizes
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = descriptorSetCount; // number of descriptor sets, set to
                                           // the number of frames in flight
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    VkDescriptorPool descriptorPool;
    if (vkCreateDescriptorPool(logicalDevice, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        FATAL("Failed to create descriptor pool!");
    }
    return descriptorPool;
}

void setupImGuiStyle()
{
    auto& style = ImGui::GetStyle();
    auto& colors = style.Colors;
    // Theme from https://github.com/ArranzCNL/ImprovedCameraSE-NG
    // style.WindowTitleAlign = ImVec2(0.5, 0.5);
    // style.FramePadding = ImVec2(4, 4);

    // Rounded slider grabber
    style.GrabRounding = 12.0f;

    // Window
    colors[ImGuiCol_WindowBg] = ImVec4{0.118f, 0.118f, 0.118f, 0.784f};
    colors[ImGuiCol_ResizeGrip] = ImVec4{0.2f, 0.2f, 0.2f, 0.5f};
    colors[ImGuiCol_ResizeGripHovered] = ImVec4{0.3f, 0.3f, 0.3f, 0.75f};
    colors[ImGuiCol_ResizeGripActive] = ImVec4{0.15f, 0.15f, 0.15f, 1.0f};

    // Header
    colors[ImGuiCol_Header] = ImVec4{0.2f, 0.2f, 0.2f, 1.0f};
    colors[ImGuiCol_HeaderHovered] = ImVec4{0.3f, 0.3f, 0.3f, 1.0f};
    colors[ImGuiCol_HeaderActive] = ImVec4{0.15f, 0.15f, 0.15f, 1.0f};

    // Title
    colors[ImGuiCol_TitleBg] = ImVec4{0.15f, 0.15f, 0.15f, 1.0f};
    colors[ImGuiCol_TitleBgActive] = ImVec4{0.15f, 0.15f, 0.15f, 1.0f};
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4{0.15f, 0.15f, 0.15f, 1.0f};

    // Frame Background
    colors[ImGuiCol_FrameBg] = ImVec4{0.2f, 0.2f, 0.2f, 1.0f};
    colors[ImGuiCol_FrameBgHovered] = ImVec4{0.3f, 0.3f, 0.3f, 1.0f};
    colors[ImGuiCol_FrameBgActive] = ImVec4{0.15f, 0.15f, 0.15f, 1.0f};

    // Button
    colors[ImGuiCol_Button] = ImVec4{0.2f, 0.2f, 0.2f, 1.0f};
    colors[ImGuiCol_ButtonHovered] = ImVec4{0.3f, 0.3f, 0.3f, 1.0f};
    colors[ImGuiCol_ButtonActive] = ImVec4{0.15f, 0.15f, 0.15f, 1.0f};

    // Tab
    colors[ImGuiCol_Tab] = ImVec4{0.15f, 0.15f, 0.15f, 1.0f};
    colors[ImGuiCol_TabHovered] = ImVec4{0.38f, 0.38f, 0.38f, 1.0f};
    colors[ImGuiCol_TabActive] = ImVec4{0.28f, 0.28f, 0.28f, 1.0f};
    colors[ImGuiCol_TabUnfocused] = ImVec4{0.15f, 0.15f, 0.15f, 1.0f};
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4{0.2f, 0.2f, 0.2f, 1.0f};
}

void initFonts()
{
    auto io = ImGui::GetIO();
    ImFontAtlas* atlas = ImGui::GetIO().Fonts;
    atlas->FontBuilderIO = ImGuiFreeType::GetBuilderForFreeType();
    atlas->FontBuilderFlags
        = ImGuiFreeTypeBuilderFlags_LightHinting | ImGuiFreeTypeBuilderFlags_LoadColor;

    const ImWchar ranges[] = {0x1, 0x1FFFF, 0};
    ImFontConfig cfg;
    cfg.MergeMode = false;
    cfg.OversampleH = cfg.OversampleV = 1;
    cfg.FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_LoadColor;

    atlas->AddFontFromFileTTF(
        "../assets/fonts/seguiemj.ttf", DEFAULTS::ImGui::DEFAULT_FONT_SIZE, &cfg, ranges
    );

    atlas->Build();
    ImGui_ImplVulkan_CreateFontsTexture();
}

} // namespace Tetrium_ImGui

void Tetrium::reinitImGuiFrameBuffers(Tetrium::ImGuiRenderContexts& ctx)
{
    for (auto framebuffer : {&ctx.frameBuffers[RGB], &ctx.frameBuffers[OCV]}) {
        for (auto fb : *framebuffer) {
            vkDestroyFramebuffer(_device->logicalDevice, fb, nullptr);
        }
    }
    Tetrium_ImGui::InitializeFrameBuffer(
        _device->Get(),
        _swapChain.extent,
        ctx.renderPass,
        _swapChain.numImages,
        _renderContexts[RGB].virtualFrameBuffer.imageView,
        _renderContexts[OCV].virtualFrameBuffer.imageView,
        ctx.frameBuffers[RGB],
        ctx.frameBuffers[OCV]
    );
}

void Tetrium::destroyImGuiContext(Tetrium::ImGuiRenderContexts& ctx)
{
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    for (auto framebuffer : {&ctx.frameBuffers[RGB], &ctx.frameBuffers[OCV]}) {
        for (auto fb : *framebuffer) {
            vkDestroyFramebuffer(_device->logicalDevice, fb, nullptr);
        }
    }

    vkDestroyRenderPass(_device->logicalDevice, ctx.renderPass, nullptr);
    vkDestroyDescriptorPool(_device->logicalDevice, ctx.descriptorPool, nullptr);
}

void Tetrium::initImGuiRenderContext(Tetrium::ImGuiRenderContexts& ctx)
{
    // create render pass
    VkImageLayout imguiInitialLayout, imguiFinalLayout;
    imguiInitialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    imguiFinalLayout
        = _tetraMode == TetraMode::kDualProjector
              ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR       // dual project's two passes directly present
              : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL; // virtual fb to be finally transferred to
                                                      // swapchain
    ctx.renderPass = Tetrium_ImGui::createRenderPass(
        _device->Get(), _swapChain.imageFormat, imguiInitialLayout, imguiFinalLayout
    );

    ctx.descriptorPool = Tetrium_ImGui::createDescriptorPool(
        DEFAULTS::ImGui::TEXTURE_DESCRIPTOR_POOL_SIZE, _device->Get()
    );
    Tetrium_ImGui::InitializeFrameBuffer(
        _device->Get(),
        _swapChain.extent,
        ctx.renderPass,
        _swapChain.numImages,
        _renderContexts[RGB].virtualFrameBuffer.imageView,
        _renderContexts[OCV].virtualFrameBuffer.imageView,
        ctx.frameBuffers[RGB],
        ctx.frameBuffers[OCV]
    );

    ImGui_ImplVulkan_InitInfo initInfo = {};
    initInfo.Instance = _instance;
    initInfo.PhysicalDevice = _device->physicalDevice;
    initInfo.Device = _device->logicalDevice;
    initInfo.QueueFamily = _device->queueFamilyIndices.graphicsFamily.value();
    initInfo.Queue = _device->graphicsQueue;
    initInfo.PipelineCache = VK_NULL_HANDLE;
    initInfo.DescriptorPool = ctx.descriptorPool;
    initInfo.Allocator = VK_NULL_HANDLE; // keeping it none is fine
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = _swapChain.numImages;
    initInfo.CheckVkResultFn = nullptr;
    initInfo.RenderPass = ctx.renderPass;

    IMGUI_CHECKVERSION();

    // initialize imgui contexts for both
    for (ColorSpace cs : {RGB, OCV}) {
        ctx.ctxImGui[cs] = ImGui::CreateContext();
        ctx.ctxImPlot[cs] = ImPlot::CreateContext();
        ImGui::SetCurrentContext(ctx.ctxImGui[cs]);
        ImPlot::SetCurrentContext(ctx.ctxImPlot[cs]);

        ImGui_ImplGlfw_InitForVulkan(_window, false);
        ImGui_ImplVulkan_Init(&initInfo);

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
        ImGui::StyleColorsDark();

        Tetrium_ImGui::setupImGuiStyle();
        Tetrium_ImGui::initFonts();

        // store the context pointers
        Tetrium_ImGui::ctxImGui[cs] = ctx.ctxImGui[cs];
        Tetrium_ImGui::ctxImPlot[cs] = ctx.ctxImPlot[cs];
    }

    Tetrium_ImGui::setupCustomCallbacks(_window);
    DEBUG("imgui context initialized");
}

void Tetrium::recordImGuiDrawCommandBuffer(
    Tetrium::ImGuiRenderContexts& ctx,
    ColorSpace colorSpace,
    vk::CommandBuffer cb,
    vk::Extent2D extent,
    int swapChainImageIndex
)
{
    // assuming drawImGui() has been invoked for both colorSpace
    ImGui::SetCurrentContext(_imguiCtx.ctxImGui[colorSpace]);
    ImPlot::SetCurrentContext(_imguiCtx.ctxImPlot[colorSpace]);

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = ctx.renderPass;
    renderPassInfo.framebuffer = ctx.frameBuffers[colorSpace][swapChainImageIndex];
    renderPassInfo.renderArea.extent = extent;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.clearValueCount = 0;
    renderPassInfo.pClearValues = nullptr;

    vkCmdBeginRenderPass(cb, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    ImDrawData* drawData = ImGui::GetDrawData();
    if (drawData == nullptr) {
        FATAL("Draw data is null!");
    }
    ImGui_ImplVulkan_RenderDrawData(drawData, cb);
    vkCmdEndRenderPass(cb);
}

const ImGuiTexture& Tetrium::getOrLoadImGuiTexture(
    Tetrium::ImGuiRenderContexts& ctx,
    const std::string& texture
)
{
    auto it = ctx.textures.find(texture);
    if (it != ctx.textures.end()) {
        return it->second;
    }

    TextureManager::Texture t = _textureManager.GetTexture(texture);
    VkDescriptorSet descriptor = ImGui_ImplVulkan_AddTexture(
        t.sampler, t.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

    ImGuiTexture tex{.id = descriptor, .width = t.width, .height = t.height};

    auto res = ctx.textures.insert({texture, tex});
    ASSERT(res.second);
    return res.first->second;
}

void Tetrium::clearImGuiDrawData()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();

    ImGui::NewFrame();
    ImGui::Render();
}
