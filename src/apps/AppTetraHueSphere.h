#pragma once

#include "App.h"

namespace TetriumApp
{

class AppTetraHueSphere : public App
{
  public:
    virtual void Init(TetriumApp::InitContext& ctx) override {
        DEBUG("Initializing TetraHueSphere...");

        // create feamebuffer and bind to texture
        // ImGui_ImplVulkan_AddTexture

    };
    virtual void Cleanup(TetriumApp::CleanupContext& ctx) override {
        DEBUG("Cleaning up TetraHueSphere...");

    };

    virtual void TickVulkan(TetriumApp::TickContextVulkan& ctx) override {
        // render to the correct framebuffer&texture
        

        // push render semaphore

    }

    virtual void TickImGui(const TetriumApp::TickContextImGui& ctx) override;

  private:
};

} // namespace TetriumApp
