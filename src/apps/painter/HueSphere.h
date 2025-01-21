#pragma once
#include <apps/App.h>
#include <apps/app_components/TextureFrameBuffer.h>
#include <apps/app_components/Transform.h>

#include "components/Camera.h" // FIXME: fix dependency hell

class HueSphere
{
    public:
    void Init(TetriumApp::InitContext& ctx, uint32_t fbWidth, uint32_t fbHeight, uint32_t cubemapSize);
    void Cleanup(TetriumApp::CleanupContext& ctx);

    void TickVulkan(TetriumApp::TickContextVulkan& ctx);
    void DrawHuesphereInImGui(const TetriumApp::TickContextImGui& ctx);
    private:

    std::array<vk::ClearValue, 2> _clearValues; // [color, depthStencil]
    vk::RenderPass _renderPass = VK_NULL_HANDLE;

    struct RenderContext
    {
        TextureFrameBuffer fb;
    };

    std::array<RenderContext, NUM_FRAME_IN_FLIGHT> _renderContexts;

    void initRenderContext(RenderContext& ctx, TetriumApp::InitContext& initCtx);
    void cleanupRenderContext(RenderContext& ctx, TetriumApp::CleanupContext& cleanupCtx);

    void initRenderPass(TetriumApp::InitContext& initCtx);

    void initRasterization(TetriumApp::InitContext& initCtx);
    void cleanupRasterization(TetriumApp::CleanupContext& cleanupCtx);

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
        Transform hueSpheretransform = Transform::Identity();

        float sphereRotationSpeed = 1.f;

        ProjectionType projectionType = ProjectionType::Perspective;

        float orthoWidth = 1.f;
        float perspectiveFOV = 90;

        std::vector<uint32_t> loadedTextures;
    } _rasterizationCtx;

    uint32_t _fbWidth;
    uint32_t _fbHeight;
};
