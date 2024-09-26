#include "ImGuiManager.h"
#include "backends/imgui_impl_glfw.h"
#include "imgui.h"
#include "implot.h"

void ImGuiManager::setupImGuiStyle()
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

void ImGuiManager::InitializeImgui()
{
    INFO("Initializing imgui...");
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
    ImGui::StyleColorsDark();
#if __APPLE__
    ImFont* font = io.Fonts->AddFontFromFileTTF(
        "/System/Library/Fonts/Optima.ttc", DEFAULTS::ImGui::DEFAULT_FONT_SIZE
    );
#endif
    setupImGuiStyle();
    INFO("ImGui initialized.");
}

void ImGuiManager::BindVulkanResources(
    GLFWwindow* window,
    VkInstance instance,
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    uint32_t graphicsQueueFamilyIndex,
    VkQueue graphicsQueue,
    int imageCount
)
{
    if (this->_imGuiRenderPass == VK_NULL_HANDLE) {
        FATAL("Render pass must be initialized before binding vulkan resources!");
    }
    ImGui_ImplGlfw_InitForVulkan(window, true);

    ImGui_ImplVulkan_InitInfo initInfo = {};
    initInfo.Instance = instance;
    initInfo.PhysicalDevice = physicalDevice;
    initInfo.Device = device;
    initInfo.QueueFamily = graphicsQueueFamilyIndex;
    initInfo.Queue = graphicsQueue;
    initInfo.PipelineCache = VK_NULL_HANDLE;        // keeping it none is fine
    initInfo.DescriptorPool = _imguiDescriptorPool; // imgui custom descriptor pool
    initInfo.Allocator = VK_NULL_HANDLE;            // keeping it none is fine
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = imageCount;
    initInfo.CheckVkResultFn = nullptr;
    initInfo.RenderPass = this->_imGuiRenderPass;
    ImGui_ImplVulkan_Init(&initInfo);
};

void ImGuiManager::InitializeRenderPass(
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

    if (vkCreateRenderPass(logicalDevice, &renderPassInfo, nullptr, &this->_imGuiRenderPass)
        != VK_SUCCESS) {
        FATAL("Failed to create render pass!");
    }
}

void ImGuiManager::InitializeFonts() { ImGui_ImplVulkan_CreateFontsTexture(); }

void ImGuiManager::DestroyFrameBuffers(VkDevice device)
{
    DEBUG("Destroying imgui frame buffers...");
    for (auto framebuffer : {&_imGuiFramebuffers.RGB, &_imGuiFramebuffers.CMY}) {
        for (auto fb : *framebuffer) {
            vkDestroyFramebuffer(device, fb, nullptr);
        }
    }
}

void ImGuiManager::InitializeFrameBuffer(
    int bufferCount,
    VkDevice device,
    std::vector<VkImageView>& swapChainImageViewsRGB,
    std::vector<VkImageView>& swapChainImageViewsCMY,
    VkExtent2D extent
)
{
    DEBUG("Creating imgui frame buffers...");
    ASSERT(swapChainImageViewsRGB.size() == bufferCount);
    ASSERT(swapChainImageViewsCMY.size() == bufferCount);
    if (this->_imGuiRenderPass == VK_NULL_HANDLE) {
        FATAL("Render pass must be initialized before creating frame buffers!");
    }
    _imGuiFramebuffers.RGB.resize(bufferCount);
    _imGuiFramebuffers.CMY.resize(bufferCount);
    VkImageView attachment[1];
    VkFramebufferCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    info.renderPass = _imGuiRenderPass;
    info.attachmentCount = 1;
    info.pAttachments = attachment;
    info.width = extent.width;
    info.height = extent.height;
    info.layers = 1;

    for (uint32_t i = 0; i < bufferCount; i++) {
        attachment[0] = swapChainImageViewsRGB[i];
        vkCreateFramebuffer(device, &info, nullptr, &_imGuiFramebuffers.RGB[i]);
        attachment[0] = swapChainImageViewsCMY[i];
        vkCreateFramebuffer(device, &info, nullptr, &_imGuiFramebuffers.CMY[i]);
    }
    DEBUG("Imgui frame buffers created.");
}

void ImGuiManager::InitializeDescriptorPool(int frames_in_flight, VkDevice logicalDevice)
{
    // create a pool that will allocate to actual descriptor sets
    uint32_t descriptorSetCount = static_cast<uint32_t>(frames_in_flight);
    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, descriptorSetCount} // image sampler for imgui
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount
        = sizeof(poolSizes) / sizeof(VkDescriptorPoolSize); // number of pool sizes
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = descriptorSetCount; // number of descriptor sets, set to
                                           // the number of frames in flight
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    if (vkCreateDescriptorPool(logicalDevice, &poolInfo, nullptr, &_imguiDescriptorPool)
        != VK_SUCCESS) {
        FATAL("Failed to create descriptor pool!");
    }
}

void ImGuiManager::Cleanup(VkDevice logicalDevice)
{
    INFO("Cleaning up imgui...");
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    vkDestroyRenderPass(logicalDevice, _imGuiRenderPass, nullptr);
    vkDestroyDescriptorPool(logicalDevice, _imguiDescriptorPool, nullptr);
    DEBUG("cleaned up imgui");
}

void ImGuiManager::RecordCommandBufferCMY(
    vk::CommandBuffer cb,
    vk::Extent2D extent,
    int swapChainImageIndex
)
{
    // start render pass
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = _imGuiRenderPass;
    renderPassInfo.framebuffer = _imGuiFramebuffers.CMY[swapChainImageIndex];
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

void ImGuiManager::RecordCommandBufferRGB(
    vk::CommandBuffer cb,
    vk::Extent2D extent,
    int swapChainImageIndex
)
{
    // start render pass
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = _imGuiRenderPass;
    renderPassInfo.framebuffer = _imGuiFramebuffers.RGB[swapChainImageIndex];
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

void ImGuiManager::RecordCommandBuffer(
    vk::CommandBuffer cb,
    vk::Extent2D extent,
    int swapChainImageIndex
)
{
    // start render pass
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = _imGuiRenderPass;
    renderPassInfo.framebuffer = _imGuiFrameBuffer[swapChainImageIndex];
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

void ImGuiManager::BeginImGuiContext()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();

    ImGui::NewFrame();
}

void ImGuiManager::forceDisplaySize(ImVec2 size)
{
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = size;
    io.DisplayFramebufferScale = {1, 1};
    ImGui::GetMainViewport()->Size = size;
}

void ImGuiManager::EndImGuiContext() { ImGui::Render(); }

void ImGuiManager::ClearImGuiElements()
{
    BeginImGuiContext();
    EndImGuiContext();
}
