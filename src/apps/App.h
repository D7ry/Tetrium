#pragma once

#include "structs/ImGuiTexture.h"
#include "structs/SharedEngineStructs.h"

namespace TetriumApp
{

struct InitContext
{
    vk::Device device; // same as CleanupContext::device
};

struct CleanupContext
{
    vk::Device device; // same as InitContext::device
};

struct TickContextImGui
{
    ColorSpace colorSpace;

    struct
    {
        std::function<ImGuiTexture(const std::string&)> GetImGuiTexture;
        std::function<void(const std::string&)> UnloadImGuiTexture;
        std::function<void(Sound)> PlaySound;
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
    // semaphores to wait for before the engine performs vulkan ImGui rendering
    // TickVulkan() should push all semaphores that need to be waited on to this
    // std::vector<vk::Semaphore>& waitSemaphores;
};

// ImGui-based application interface
class App
{
  public:
    virtual void Init(TetriumApp::InitContext& ctx) = 0;
    virtual void Cleanup(TetriumApp::CleanupContext& ctx) = 0;
    virtual void OnClose() {};
    virtual void OnOpen() {};
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
