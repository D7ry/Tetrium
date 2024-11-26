#pragma once

#include "App.h"
#include "lib/VQDeviceImage.h"

#include "components/Camera.h" // FIXME: fix dependency hell
#include "ecs/component/TransformComponent.h" // FIXME: fix dependency hell

#include "app_components/TextureFrameBuffer.h"

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
    std::array<vk::ClearValue, 2> _clearValues; // [color, depthStencil]
    vk::RenderPass _renderPass = VK_NULL_HANDLE;

    // context for one frame rendering
    struct RenderContext
    {
        TextureFrameBuffer fb;
    };

    void initRenderContext(RenderContext& ctx, TetriumApp::InitContext& initCtx);
    void cleanupRenderContext(RenderContext& ctx, TetriumApp::CleanupContext& cleanupCtx);

    void initRenderPass(TetriumApp::InitContext& initCtx);

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
        int textureIndex; // [0, 1, 2, 3] -> [uglyRGB, uglyOCV, prettyRGB, prettyOCV]
    };

    struct Mesh
    {
        VQBuffer vertexBuffer;
        VQBufferIndex indexBuffer;
    };

    enum class RenderMeshType
    {
        UglySphere = 0,
        PrettySphere = 1
    };

    enum class ProjectionType
    {
        Perspective = 0,
        Orthographic = 1
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

        Mesh prettySphereMesh;
        Mesh uglySphereMesh;

        Camera camera;
        TransformComponent hueSpheretransform = TransformComponent::Identity();

        float sphereRotationSpeed = 1.f;


        RenderMeshType renderMeshType = RenderMeshType::PrettySphere;
        ProjectionType projectionType = ProjectionType::Perspective;

        float orthoWidth = 1.f;
        float perspectiveFOV = 90;
    } _rasterizationCtx;

};

} // namespace TetriumApp
