#pragma once

#include "lib/VQDevice.h"
#include "structs/ImGuiTexture.h"
#include "structs/SharedEngineStructs.h"

namespace TetriumApp
{

struct InitContext
{
    VQDevice& device;

    struct
    {
        vk::Format imageFormat;
        vk::Extent2D extent;
    } swapchain;

    struct
    {
        std::function<vk::DescriptorImageInfo(const std::string&)>
            LoadAndGetTextureDescriptorImageInfo;

        std::function<uint32_t(const std::string&)> LoadTexture;
        std::function<uint32_t(const std::string&)> LoadCubemapTexture;

        std::function<vk::DescriptorImageInfo(uint32_t)> GetTextureDescriptorImageInfo;
        std::function<ImGuiTexture(uint32_t)> InitImGuiTexture;

    } api;
};

struct CleanupContext
{
    VQDevice& device;

    struct
    {
        std::function<void(uint32_t)> UnloadTexture;
    } api;
};

struct TickContextImGui
{
    int currentFrameInFlight;
    ColorSpace colorSpace;

    struct
    {
        std::function<void(Sound)> PlaySound;
        std::function<uint32_t(const std::string&)> LoadTexture;
        std::function<ImGuiTexture(uint32_t)> InitImGuiTexture;
        std::function<void(uint32_t)> UnloadTexture;
    } apis;

    mutable struct
    {
        bool wantExit = false;
        std::optional<Sound> musicOverride;
    } controls;
};

struct TickContextOffScreen
{
    ColorSpace colorSpace;
};

struct TickContextVulkan
{
    int currentFrameInFlight;
    ColorSpace colorSpace;
    vk::CommandBuffer commandBuffer;
};

// ImGui-based application interface
class App
{
  public:
    virtual void Init(TetriumApp::InitContext& ctx) = 0;
    virtual void Cleanup(TetriumApp::CleanupContext& ctx) = 0;

    // Called when the app is opened i.e. becomes the primary app
    virtual void OnOpen(){};
    // Called when the app is closed i.e. no longer the primary app
    virtual void OnClose(){};

    virtual ~App() {}

    // TickImGui() and TickVulkan() are only called if the app is active
    // i.e. we expect the app to render to the screen
    // TickOffScreen() is always called

    // ImGui Tick() function,
    // the function executes in imgui context
    virtual void TickImGui(const TetriumApp::TickContextImGui& ctx) {}

    // Vulkan Tick() function,
    // record all render & compute commands within this pass,
    // push all semaphores that need to be waited on to r_waitSemaphores
    virtual void TickVulkan(TetriumApp::TickContextVulkan& ctx) {}

    // Off-screen Tick() function,
    // function runs regardless of the window being visible
    virtual void TickOffScreen(TetriumApp::TickContextOffScreen& ctx) {}
};
}; // namespace TetriumApp
