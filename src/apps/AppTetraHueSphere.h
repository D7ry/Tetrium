#pragma once

#include "App.h"
#include "lib/VQDeviceImage.h"

#include "components/Camera.h" // FIXME: fix dependency hell
#include "ecs/component/TransformComponent.h" // FIXME: fix dependency hell

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
        vk::Framebuffer frameBuffer = VK_NULL_HANDLE;
        vk::Sampler sampler = VK_NULL_HANDLE;
        VQDeviceImage deviceImage = {};
        void* imguiTextureId = nullptr;
    };

    // context for one frame rendeirng
    struct RenderContext
    {
        VirtualFrameBuffer fb;
    };

    void initRenderContext(RenderContext& ctx, TetriumApp::InitContext& initCtx);
    void cleanupRenderContext(RenderContext& ctx, TetriumApp::CleanupContext& cleanupCtx);

    void initRenderPass(TetriumApp::InitContext& initCtx);

    void cleanupDepthImage(TetriumApp::CleanupContext& cleanupCtx);
    void initDepthImage(TetriumApp::InitContext& initCtx);

    void initRasterization(TetriumApp::InitContext& initCtx);
    void cleanupRasterization(TetriumApp::CleanupContext& cleanupCtx);

    std::array<RenderContext, NUM_FRAME_IN_FLIGHT> _renderContexts;

    // rasterization
    enum class BindingLocation : uint32_t
    {
        UBO = 0,
        TEXTURE_SAMPLER = 1,
    };

    struct UBO
    {
        glm::mat4 view; // view matrix
        glm::mat4 proj; // proj matrix
        
        glm::mat4 model = glm::mat4(1); // model matrix
        int isRGB; // 1 -> RGB, 0 -> OCV
    };

    struct
    {
        std::array<VQBuffer, NUM_FRAME_IN_FLIGHT> UBOBuffer;
        vk::Pipeline pipeline = VK_NULL_HANDLE;
        vk::PipelineLayout pipelineLayout = VK_NULL_HANDLE;

        struct
        {
            vk::DescriptorSetLayout setLayout;
            vk::DescriptorPool pool;
            std::vector<vk::DescriptorSet> sets;
        } descriptors;

        struct 
        {
            VQBuffer vertexBuffer;
            VQBufferIndex indexBuffer;
        } hueSphereMesh;

        Camera camera;
        TransformComponent hueSpheretransform = TransformComponent::Identity();
    } _rasterizationCtx;

};

} // namespace TetriumApp
