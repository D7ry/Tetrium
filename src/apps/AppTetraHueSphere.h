#pragma once

#include "lib/VQDeviceImage.h"
#include "App.h"

namespace TetriumApp
{

class AppTetraHueSphere : public App
{
  public:
    virtual void Init(TetriumApp::InitContext& ctx) override;
    virtual void Cleanup(TetriumApp::CleanupContext& ctx) override;

    virtual void TickVulkan(TetriumApp::TickContextVulkan& ctx) override;
    virtual void TickImGui(const TetriumApp::TickContextImGui& ctx) override;

  private:

    VQDeviceImage _depthImage;

    std::array<vk::ClearValue, 2> _clearValues; // [color, depthStencil]
    vk::RenderPass _renderPass = VK_NULL_HANDLE;

    // framebuffer to render the hue sphere onto
    struct VirtualFrameBuffer
    {
        // TODO: if we do enough amount of 3d rendering we may abstract it away into a separate
        // class
        vk::Framebuffer frameBuffer = VK_NULL_HANDLE;
        vk::Sampler sampler = VK_NULL_HANDLE;
        VQDeviceImage deviceImage;
        void* imguiTextureId = nullptr;
    };

    // context for one frame rendeirng
    struct RenderContext
    {
        VirtualFrameBuffer fb;
    };

    void initRenderContexts(RenderContext& ctx, TetriumApp::InitContext& initCtx);
    void initRenderPass(TetriumApp::InitContext& initCtx);

    void initDepthImage(TetriumApp::InitContext& initCtx);

    std::array<RenderContext, NUM_FRAME_IN_FLIGHT> _renderContexts;
};

} // namespace TetriumApp
