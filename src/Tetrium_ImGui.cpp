// ImGui initialization and resource management


#include "imgui.h"
#include "implot.h"
#include "misc/freetype/imgui_freetype.h"

#if defined(WIN32)
#include "backends/imgui_impl_win32.h"
#else 
#include "backends/imgui_impl_glfw.h"
#endif // WIN32

#include "backends/imgui_impl_vulkan.h"

#include "lib/ImGuiUtils.h"
#include "Tetrium.h"

#include "Pathing.h"

// TODO: localize contexts

#pragma region ImGui Contexts

namespace Tetrium_ImGui
{
void InitializeFrameBuffer(
    VkDevice device,
    VkExtent2D extent,
    VkRenderPass renderPass,
    const std::vector<VkImageView>& imageView,
    std::vector<VkFramebuffer>& framebuffer
)
{
    DEBUG("Creating imgui frame buffers...");
    int bufferCount = imageView.size();
    // FIXME: cleanup
    framebuffer.resize(bufferCount);
    VkImageView attachment[1];
    VkFramebufferCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    info.renderPass = renderPass, info.attachmentCount = 1;
    info.pAttachments = attachment;
    info.width = extent.width;
    info.height = extent.height;
    info.layers = 1;

    for (uint32_t i = 0; i < bufferCount; i++) {
        attachment[0] = imageView[i];
        VK_CHECK_RESULT(vkCreateFramebuffer(device, &info, nullptr, &framebuffer[i]));
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
        std::string(ASSETS_PATH + "fonts/seguiemj.ttf").c_str(), DEFAULTS::ImGui::DEFAULT_FONT_SIZE, &cfg, ranges
    );

    atlas->Build();
    ImGui_ImplVulkan_CreateFontsTexture();
}

} // namespace Tetrium_ImGui

void Tetrium::reinitImGuiFrameBuffers(Tetrium::ImGuiRenderContext& ctx)
{
    NEEDS_IMPLEMENTATION()
    Tetrium_ImGui::InitializeFrameBuffer(
        _device->Get(),
        _swapChain.extent,
        ctx.renderPass,
        // paint directly to swapchain images
        _swapChain.imageView,
        _swapChain.frameBuffer
        // _renderContextRYGB.virtualFrameBuffer.imageView,
        // _renderContextRYGB.virtualFrameBuffer.frameBuffer
    );
}

void Tetrium::destroyImGuiContext(Tetrium::ImGuiRenderContext& ctx)
{
    // NOTE: current imgui impl does not support vulkan multi-context shutdown;
    // not a big problem for now since we only shut down at very end, but
    // it leads to ugly validation errors
    ImGui_ImplVulkan_Shutdown();
#if defined(WIN32)
    ImGui_ImplWin32_Shutdown();
#else
    ImGui_ImplGlfw_Shutdown();
#endif
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    for (VkFramebuffer fb : ctx.frameBuffer) {
        vkDestroyFramebuffer(_device->logicalDevice, fb, nullptr);
    }

    vkDestroyRenderPass(_device->logicalDevice, ctx.renderPass, nullptr);
    vkDestroyDescriptorPool(_device->logicalDevice, ctx.descriptorPool, nullptr);
}

void Tetrium::initImGuiRenderContext(Tetrium::ImGuiRenderContext& ctx)
{
    // create render pass
    VkImageLayout imguiInitialLayout, imguiFinalLayout;
    imguiInitialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // for first pass
    // imguiFinalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // for RYGB conversion pass, if
    // run imgui pass before
    imguiFinalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // if painting to physical fb

    ctx.renderPass = createRenderPass(
        _device->Get(),
        imguiInitialLayout,
        imguiFinalLayout,
        _swapChain.imageFormat,
        VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE,
        false // imgui FB has no depth attachment
    );

    ctx.descriptorPool = Tetrium_ImGui::createDescriptorPool(
        DEFAULTS::ImGui::TEXTURE_DESCRIPTOR_POOL_SIZE, _device->Get()
    );
    Tetrium_ImGui::InitializeFrameBuffer(
        _device->Get(),
        _swapChain.extent,
        ctx.renderPass,
        //_renderContextRYGB.virtualFrameBuffer.imageView,
        _swapChain.imageView, // render directly to swapchain
        ctx.frameBuffer
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

    ctx.backendImGuiContext = ImGui::CreateContext();
    ctx.backendImPlotContext = ImPlot::CreateContext();
    ImGui::SetCurrentContext(ctx.backendImGuiContext);
    ImPlot::SetCurrentContext(ctx.backendImPlotContext);

    bool installCallbacks = true;
#if defined(WIN32)
    ImGui_ImplWin32_Init(_swapChain.chainDXGI->m_hWnd);
#else
    ImGui_ImplGlfw_InitForVulkan(_window, installCallbacks);
#endif
    ImGui_ImplVulkan_Init(&initInfo);

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
    ImGui::StyleColorsDark();

    Tetrium_ImGui::setupImGuiStyle();
    Tetrium_ImGui::initFonts();

    DEBUG("imgui context initialized");
}

void Tetrium::recordImGuiDrawCommandBuffer(
    Tetrium::ImGuiRenderContext& ctx,
    vk::CommandBuffer cb,
    vk::Extent2D extent,
    int swapChainImageIndex,
    ColorSpace colorSpace
)
{

    vk::RenderPassBeginInfo renderPassInfo(
        ctx.renderPass,
        ctx.frameBuffer[swapChainImageIndex],
        vk::Rect2D({0, 0}, extent),
        _clearValues.size(),
        _clearValues.data()
    );

    cb.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

    ImDrawData* drawData = ImGui::GetDrawData();
    if (drawData == nullptr) {
        FATAL("Draw data is null!");
    }

    bool shouldPaintImGUI = true;
    switch (_rocvPresentMode) {
    case ROCVPresentMode::kNormal:
        break;
    case ROCVPresentMode::kRGBOnly:
        if (colorSpace == ColorSpace::OCV) {
            shouldPaintImGUI = false;
        }
        break;
    case ROCVPresentMode::kOCVOnly:
        if (colorSpace == ColorSpace::RGB) {
            shouldPaintImGUI = false;
        }
        break;
    }

    if (shouldPaintImGUI) {
        ImGui_ImplVulkan_RenderDrawData(drawData, cb);
    }

    cb.endRenderPass();
}

void Tetrium::clearImGuiDrawData()
{
    ImGui_ImplVulkan_NewFrame();
#if defined(WIN32)
    ImGui_ImplWin32_NewFrame();
#else
    ImGui_ImplGlfw_NewFrame();
#endif

    ImGui::NewFrame();
    ImGui::Render();
}

#pragma endregion

#pragma region ImGui Draw
void Tetrium::drawAppsImGui(ColorSpace colorSpace, int currentFrameInFlight)
{
    if (_primaryApp.has_value()) {

        TetriumApp::App* app = _primaryApp.value();
        TetriumApp::TickContextImGui ctxImGui{
            .currentFrameInFlight = currentFrameInFlight,
            .colorSpace = colorSpace,
            .apis = {
                .PlaySound = [this](Sound sound) { _soundManager.PlaySound(sound); },
                .LoadTexture = [this](const std::string& path) { return _textureManager.LoadTexture(path); },
                .InitImGuiTexture = [this](uint32_t textureHandle) {
                    _textureManager.LoadImGuiTexture(textureHandle);
                    return _textureManager.GetImGuiTexture(textureHandle);
                },
                .UnloadTexture = [this](uint32_t textureHandle) { _textureManager.UnLoadTexture(textureHandle); }
            },
            .controls = {.wantExit = false, .musicOverride = std::nullopt}
        };

        app->TickImGui(ctxImGui);

        if (ctxImGui.controls.wantExit) {
            _primaryApp.value()->OnClose();
            _primaryApp = std::nullopt;
            _soundManager.DisableMusic();
        } else {
            if (ctxImGui.controls.musicOverride.has_value()) {
                _soundManager.SetMusic(ctxImGui.controls.musicOverride.value());
            }
        }
    }
}

void Tetrium::drawMainMenu(ColorSpace colorSpace)
{
    int fullScreenFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
                          | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

    if (ImGui::Begin(DEFAULTS::Engine::APPLICATION_NAME, NULL, fullScreenFlags)) {
        if (ImGui::BeginTabBar("Engine Tab")) {
            if (ImGui::BeginTabItem("🛸Even-Odd")) {
                _widgetEvenOdd.Draw(this, colorSpace);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("🎨Apps")) {
                // show all apps
                for (auto& [appName, app] : _appMap) {
                    if (ImGui::Button(appName.c_str())) {
                        if (_primaryApp.has_value() && _primaryApp.value() != app) {
                            _primaryApp.value()->OnClose();
                        }
                        _primaryApp = app;
                        app->OnOpen();
                    }
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("🚀Performance")) {
                _widgetPerfPlot.Draw(this, colorSpace);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("💻Device")) {
                _widgetDeviceInfo.Draw(this, colorSpace);
                ImGui::EndTabItem();
            }


            if (ImGui::BeginTabItem("Color Tile")) {
                _widgetColorTile.Draw(this, colorSpace);
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar(); // Engine Tab
        }
    }

    ImGui::End();
}

void Tetrium::drawImGui(ColorSpace colorSpace, int currentFrameInFlight)
{

    // ---------- Prologue ----------
    PROFILE_SCOPE(&_profiler, "ImGui Draw");

    ImGui_ImplVulkan_NewFrame();
#if defined(WIN32)
    ImGui_ImplWin32_NewFrame();
#else
    ImGui_ImplGlfw_NewFrame();
#endif
    ImGui::NewFrame();

    // imgui is associated with the glfw window to handle inputs,
    // but its actual fb is associated with the projector display;
    // so we need to manually re-adjust the display size for the scissors/
    // viewports/clipping to be consistent
    bool imguiDisplaySizeOverride = _tetraMode == TetraMode::kEvenOddHardwareSync;
    if (imguiDisplaySizeOverride) {
        ImVec2 projectorDisplaySize{
#if defined(WIN32)
            static_cast<float>(_swapChain.extent.width),
            static_cast<float>(_swapChain.extent.height)
#else
            static_cast<float>(_mainProjectorDisplay.extent.width),
            static_cast<float>(_mainProjectorDisplay.extent.height)
#endif
        };
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = projectorDisplaySize;
        io.DisplayFramebufferScale = {1, 1};
        ImGui::GetMainViewport()->Size = projectorDisplaySize;
    }

    if (_captureCursor) {
#if defined(WIN32)
#else
        glfwSetInputMode(_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        ImGuiTexture imguiTexture = _engineTextures[(int)EngineTexture::kCursor].second;
        ImGuiU::DrawCursor(imguiTexture);
#endif // WIN32
    } else {
#if defined(WIN32)
#else
        glfwSetInputMode(_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
#endif // WIN32
        ImGuiU::DrawCenteredText("Press Tab to enable input", ImVec4(0, 0, 0, 0.8));
    }

    std::string footnoteText = (const char*)u8"🧩 Tetrium 0.9a";
    switch (_rocvPresentMode) {
        case ROCVPresentMode::kNormal:
            footnoteText += " | Normal Mode";
            break;
        case ROCVPresentMode::kRGBOnly:
            footnoteText += " | RGB Only Mode";
            break;
        case ROCVPresentMode::kOCVOnly:
            footnoteText += " | OCV Only Mode";
            break;
    }

#ifndef NDEBUG
#if defined(WIN32)
    footnoteText += "| Dropped Frames: " + std::to_string(_swapChain.chainDXGI->GetNumDroppedFrames());
#endif
#endif
    ImGuiU::DrawFootNote(footnoteText.c_str());

    if (_primaryApp.has_value()) {
        drawAppsImGui(colorSpace, currentFrameInFlight);
    } else {
        drawMainMenu(colorSpace);
    }

    ImGui::Render();
}
#pragma endregion
