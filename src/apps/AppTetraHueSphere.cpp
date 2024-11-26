#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"

#include "lib/VQUtils.h"
#include "structs/Vertex.h"

#include "components/ShaderUtils.h"
#include "components/TextureManager.h" // FIXME: fix dependency hell
#include "lib/VulkanUtils.h"

#include "AppTetraHueSphere.h"

namespace TetriumApp
{

// localize scope
namespace
{
// FIXME: figure out a way to resize fb
// by recreating fb on imgui window resize callback
const int FB_WIDTH = 1024;
const int FB_HEIGHT = 1024;

static const VkFormat FB_IMAGE_FORMAT = VK_FORMAT_R8G8B8A8_SRGB;

const char* VERTEX_SHADER_PATH = "../assets/apps/AppTetraHueSphere/shader.vert.spv";
const char* FRAGMENT_SHADER_PATH = "../assets/apps/AppTetraHueSphere/shader.frag.spv";

// const char* HUE_SPHERE_MODEL_PATH = "../assets/apps/AppTetraHueSphere/ugly_sphere.obj";

const char* HUE_SPHERE_MODEL_PATH = "../assets/apps/AppTetraHueSphere/fibonacci_sampled.obj";
const char* HUE_SPHERE_TEXTURE_PATH_RGB = "../assets/apps/AppTetraHueSphere/RGB.png";
const char* HUE_SPHERE_TEXTURE_PATH_OCV = "../assets/apps/AppTetraHueSphere/OCV.png";
} // namespace

void AppTetraHueSphere::TickImGui(const TetriumApp::TickContextImGui& ctx)
{
    if (ImGui::Begin("TetraHueSphere")) {
        ImGui::Text("Hello, TetraHueSphere!");

        RenderContext& renderCtx = _renderContexts[ctx.currentFrameInFlight];
        // color & depth stencil clear value sliders
        ImGui::ColorEdit4("Clear Color", (float*)&_clearValues[0].color);
        ImGui::SliderFloat("Clear Depth", &_clearValues[1].depthStencil.depth, 0.f, 1.f);

        // camera control;
        glm::vec3 pos = _rasterizationCtx.camera.GetPosition();

        ImGui::Text("Camera Position: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);
        bool positionChanged = false;
        if (ImGui::SliderFloat("Camera X", &pos.x, -10.f, 10.f)) {
            positionChanged = true;
        }
        if (ImGui::SliderFloat("Camera Y", &pos.y, -10.f, 10.f)) {
            positionChanged = true;
        }
        if (ImGui::SliderFloat("Camera Z", &pos.z, -10.f, 10.f)) {
            positionChanged = true;
        }

        if (positionChanged) {
            _rasterizationCtx.camera.SetPosition(pos.x, pos.y, pos.z);
        }
        ImGui::SliderFloat("Camera FOV", &_rasterizationCtx.fov, 1.f, 130.f);

        // ImGui::Text("Hue Sphere Transform");
        //
        // // slider for hue sphere transform
        // glm::vec3 hueSpherePos = _rasterizationCtx.hueSpheretransform.position;
        // if (ImGui::SliderFloat("Hue Sphere X", &hueSpherePos.x, -10.f, 10.f)) {
        //     _rasterizationCtx.hueSpheretransform.position = hueSpherePos;
        // }
        // if (ImGui::SliderFloat("Hue Sphere Y", &hueSpherePos.y, -10.f, 10.f)) {
        //     _rasterizationCtx.hueSpheretransform.position = hueSpherePos;
        // }
        // if (ImGui::SliderFloat("Hue Sphere Z", &hueSpherePos.z, -10.f, 10.f)) {
        //     _rasterizationCtx.hueSpheretransform.position = hueSpherePos;
        // }

        ImGui::Image(renderCtx.fb.imguiTextureId, ImVec2(FB_WIDTH, FB_HEIGHT));

        if (ImGui::Button("Close")) {
            ctx.controls.wantExit = true;
        }
    }
    ImGui::End();

    // rotate hue sphere
    ImGuiIO& io = ImGui::GetIO();

    _rasterizationCtx.hueSpheretransform.rotation.z += io.DeltaTime * 10.f;
}

void AppTetraHueSphere::TickVulkan(TetriumApp::TickContextVulkan& ctx)
{
    // flush UBO
    {
        UBO* pUBO = reinterpret_cast<UBO*>(
            _rasterizationCtx.UBOBuffer.at(ctx.currentFrameInFlight).bufferAddress
        );

        pUBO->view = _rasterizationCtx.camera.GetViewMatrix();

        vk::Extent2D extent = vk::Extent2D(FB_WIDTH, FB_HEIGHT);

        glm::mat4 projectionMatrix = glm::perspective(
            glm::radians(_rasterizationCtx.fov),
            extent.width / static_cast<float>(extent.height),
            DEFAULTS::ZNEAR,
            DEFAULTS::ZFAR
        );
        projectionMatrix[1][1] *= -1; // invert for vulkan coord system
        pUBO->proj = projectionMatrix;
        pUBO->model = _rasterizationCtx.hueSpheretransform.GetModelMatrix();
        pUBO->isRGB = ctx.colorSpace == ColorSpace::RGB ? 1 : 0;
    }

    auto CB = ctx.commandBuffer;
    RenderContext& renderCtx = _renderContexts[ctx.currentFrameInFlight];
    // render to the correct framebuffer&texture

    CB.beginRenderPass(
        vk::RenderPassBeginInfo(
            _renderPass,
            renderCtx.fb.frameBuffer,
            vk::Rect2D({0, 0}, {FB_WIDTH, FB_HEIGHT}),
            _clearValues.size(),
            _clearValues.data()
        ),
        vk::SubpassContents::eInline
    );
    CB.bindPipeline(vk::PipelineBindPoint::eGraphics, _rasterizationCtx.pipeline);

    CB.setViewport(0, vk::Viewport(0.f, 0.f, FB_WIDTH, FB_HEIGHT, 0.f, 1.f));
    CB.setScissor(0, vk::Rect2D({0, 0}, {FB_WIDTH, FB_HEIGHT}));
    CB.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        _rasterizationCtx.pipelineLayout,
        0,
        _rasterizationCtx.descriptors.sets[ctx.currentFrameInFlight],
        nullptr
    );

    CB.bindVertexBuffers(0, vk::Buffer(_rasterizationCtx.hueSphereMesh.vertexBuffer.buffer), {0});
    CB.bindIndexBuffer(
        vk::Buffer(_rasterizationCtx.hueSphereMesh.indexBuffer.buffer),
        0,
        vk::IndexType(VQ_BUFFER_INDEX_TYPE)
    );

    CB.drawIndexed(_rasterizationCtx.hueSphereMesh.indexBuffer.numIndices, 1, 0, 0, 0);

    CB.endRenderPass();
}

void AppTetraHueSphere::Cleanup(TetriumApp::CleanupContext& ctx)
{
    DEBUG("Cleaning up TetraHueSphere...");
    cleanupRasterization(ctx);
    for (RenderContext& renderCtx : _renderContexts) {
        cleanupRenderContext(renderCtx, ctx);
    }

    cleanupDepthImage(ctx);

    vk::Device device = ctx.device.logicalDevice;
    device.destroyRenderPass(_renderPass);
};

void AppTetraHueSphere::Init(TetriumApp::InitContext& ctx)
{
    DEBUG("Initializing TetraHueSphere...");
    initDepthImage(ctx);
    initRenderPass(ctx);

    // create all render contexts,
    // NOTE: fb creation requires render pass to be created
    for (int i = 0; i < NUM_FRAME_IN_FLIGHT; i++) {
        initRenderContext(_renderContexts[i], ctx);
    }

    _clearValues[0].color = {0.0f, 0.0f, 0.0f, 1.f};
    _clearValues[1].depthStencil = vk::ClearDepthStencilValue(1.f, 0.f);

    initRasterization(ctx);
};

void AppTetraHueSphere::cleanupRenderContext(
    RenderContext& ctx,
    TetriumApp::CleanupContext& cleanupCtx
)
{
    vk::Device device = cleanupCtx.device.logicalDevice;
    device.destroyFramebuffer(ctx.fb.frameBuffer);
    device.destroySampler(ctx.fb.sampler);
    device.destroyImageView(ctx.fb.deviceImage.view);
    device.destroyImage(ctx.fb.deviceImage.image);
    device.freeMemory(ctx.fb.deviceImage.memory);
}

void AppTetraHueSphere::initRenderContext(RenderContext& ctx, TetriumApp::InitContext& initCtx)
{
    VirtualFrameBuffer& fb = ctx.fb;
    vk::Device device = initCtx.device.logicalDevice;

    // create image & image view
    {
        // TODO: do we need to use swapchain's image format??
        VkFormat imageFormat = FB_IMAGE_FORMAT;
        VulkanUtils::createImage(
            FB_WIDTH,
            FB_HEIGHT,
            imageFormat,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, // sampled by imgui
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            fb.deviceImage.image,
            fb.deviceImage.memory,
            initCtx.device.physicalDevice,
            initCtx.device.logicalDevice
        );
        fb.deviceImage.view = VulkanUtils::createImageView(
            fb.deviceImage.image,
            initCtx.device.logicalDevice,
            imageFormat,
            VK_IMAGE_ASPECT_COLOR_BIT
        );
    }

    // create fb
    {
        ASSERT(_renderPass != VK_NULL_HANDLE);
        std::array<vk::ImageView, 2> attachments = {fb.deviceImage.view, _depthImage.view};
        ASSERT(fb.deviceImage.view != VK_NULL_HANDLE);
        ASSERT(_depthImage.view != VK_NULL_HANDLE);

        vk::FramebufferCreateInfo fbCreateInfo(
            {}, _renderPass, attachments.size(), attachments.data(), FB_WIDTH, FB_HEIGHT, 1
        );

        fb.frameBuffer = device.createFramebuffer(fbCreateInfo);
    }

    // create sampler
    {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 0;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;

        fb.sampler = device.createSampler(samplerInfo);
    }

    // create imgui texture
    fb.imguiTextureId = ImGui_ImplVulkan_AddTexture(
        fb.sampler, fb.deviceImage.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );
}

void AppTetraHueSphere::initRenderPass(TetriumApp::InitContext& initCtx)
{
    vk::Device device = initCtx.device.logicalDevice;
    DEBUG("Creating render pass...");
    // set up color/depth layout
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // depth attachment ref for subpass, only used when `createDepthAttachment` is true
    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    // color, and optionally depth attachments
    VkAttachmentDescription attachments[2];
    uint32_t attachmentCount = 2;
    // create color attachment
    {
        VkAttachmentDescription& colorAttachment = attachments[0];
        colorAttachment = {};
        colorAttachment.format = FB_IMAGE_FORMAT;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;

        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

        // don't care about stencil
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        // paint to the virual fb that read as a texture by imgui pass
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    // add in depth attachment when `createDepthAttachment` is set to true
    // we have already specified above to have depthAttachmentRef for subpass creation
    {
        VkAttachmentDescription& depthAttachment = attachments[1];
        depthAttachment = {};
        depthAttachment = VkAttachmentDescription{};
        depthAttachment.format = static_cast<VkFormat>(initCtx.device.depthFormat);
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }

    VkSubpassDependency dependency = VkSubpassDependency{
        .srcSubpass = VK_SUBPASS_EXTERNAL, // wait for all previous subpasses
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // what to wait
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // what to not execute
        // we care about src and dst access masks only when there's a potential data race.
        .srcAccessMask = VK_ACCESS_NONE,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
    };

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.pAttachments = attachments;
    renderPassInfo.attachmentCount = attachmentCount;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;

    renderPassInfo.pDependencies = &dependency;

    _renderPass = device.createRenderPass(renderPassInfo, nullptr);

    ASSERT(_renderPass != VK_NULL_HANDLE);
    DEBUG("render pass created");
}

void AppTetraHueSphere::initDepthImage(TetriumApp::InitContext& initCtx)
{
    DEBUG("Creating depth buffer...");
    VkFormat depthFormat = static_cast<VkFormat>(initCtx.device.depthFormat);

    VulkanUtils::createImage(
        FB_WIDTH,
        FB_HEIGHT,
        depthFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        _depthImage.image,
        _depthImage.memory,
        initCtx.device.physicalDevice,
        initCtx.device.logicalDevice
    );

    VkDevice device = initCtx.device.logicalDevice;
    _depthImage.view = VulkanUtils::createImageView(
        _depthImage.image, device, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT
    );
}

void AppTetraHueSphere::cleanupDepthImage(TetriumApp::CleanupContext& cleanupCtx)
{
    vk::Device device = cleanupCtx.device.logicalDevice;
    device.destroyImageView(_depthImage.view);
    device.destroyImage(_depthImage.image);
    device.freeMemory(_depthImage.memory);
}

void AppTetraHueSphere::initRasterization(TetriumApp::InitContext& initCtx)
{
    vk::Device device = initCtx.device.logicalDevice;
    // allocate device memory for UBO
    {
        for (VQBuffer& buffer : _rasterizationCtx.UBOBuffer) {
            buffer = initCtx.device.CreateBuffer(
                sizeof(UBO),
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
            );
        }
    }

    // bind & allocate descriptor sets
    {
        const size_t UBO_DESCRIPTOR_COUNT = 1;
        const size_t SAMPLER_DESCRIPTOR_COUNT = 2; // 2 textures -- RGB / OCV of the sphere

        vk::DescriptorSetLayoutBinding uboLayoutBinding(
            (uint32_t)BindingLocation::UBO,
            vk::DescriptorType::eUniformBuffer,
            UBO_DESCRIPTOR_COUNT,
            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
        );

        vk::DescriptorSetLayoutBinding samplerLayoutBinding(
            (uint32_t)BindingLocation::TEXTURE_SAMPLER,
            vk::DescriptorType::eCombinedImageSampler,
            SAMPLER_DESCRIPTOR_COUNT,
            vk::ShaderStageFlagBits::eFragment
        );
        std::array<vk::DescriptorSetLayoutBinding, 2> bindings
            = {uboLayoutBinding, samplerLayoutBinding};

        vk::DescriptorSetLayoutCreateInfo layoutInfo({}, bindings.size(), bindings.data());

        vk::Device device = initCtx.device.logicalDevice;
        _rasterizationCtx.descriptors.setLayout = device.createDescriptorSetLayout(layoutInfo);

        // create descriptor pool
        vk::DescriptorPoolSize poolSizeUBO(
            vk::DescriptorType::eUniformBuffer, NUM_FRAME_IN_FLIGHT * UBO_DESCRIPTOR_COUNT
        );
        vk::DescriptorPoolSize poolSizeSampler(
            vk::DescriptorType::eCombinedImageSampler,
            NUM_FRAME_IN_FLIGHT * SAMPLER_DESCRIPTOR_COUNT
        );

        std::array<vk::DescriptorPoolSize, 2> poolSizes = {poolSizeUBO, poolSizeSampler};
        vk::DescriptorPoolCreateInfo poolCreateInfo(
            {}, NUM_FRAME_IN_FLIGHT, poolSizes.size(), poolSizes.data()
        );

        _rasterizationCtx.descriptors.pool = device.createDescriptorPool(poolCreateInfo);

        // allocate NUM_FRAME_IN_FLIGHT descriptor sets
        std::vector<vk::DescriptorSetLayout> layouts(
            NUM_FRAME_IN_FLIGHT, _rasterizationCtx.descriptors.setLayout
        );

        vk::DescriptorSetAllocateInfo allocInfo(
            _rasterizationCtx.descriptors.pool, NUM_FRAME_IN_FLIGHT, layouts.data()
        );

        _rasterizationCtx.descriptors.sets = device.allocateDescriptorSets(allocInfo);
        // must have allocated the correct number of descriptor sets
        ASSERT(_rasterizationCtx.descriptors.sets.size() == NUM_FRAME_IN_FLIGHT);

        // update descriptor sets to be pointing to the correct buffers
        for (int i = 0; i < NUM_FRAME_IN_FLIGHT; i++) {
            std::array<vk::WriteDescriptorSet, 2> descriptorWrites;

            // UBO
            vk::DescriptorBufferInfo bufferInfo(
                _rasterizationCtx.UBOBuffer[i].buffer, 0, sizeof(UBO)
            );

            descriptorWrites[0] = vk::WriteDescriptorSet(
                _rasterizationCtx.descriptors.sets[i],
                (uint32_t)BindingLocation::UBO,
                0,
                1,
                vk::DescriptorType::eUniformBuffer,
                nullptr,
                &bufferInfo,
                nullptr
            );

            // sampler
            VkDescriptorImageInfo imageInfoRGB;
            VkDescriptorImageInfo imageInfoOCV;
            initCtx.textureManager->GetDescriptorImageInfo(
                HUE_SPHERE_TEXTURE_PATH_RGB, imageInfoRGB
            );
            initCtx.textureManager->GetDescriptorImageInfo(
                HUE_SPHERE_TEXTURE_PATH_OCV, imageInfoOCV
            );

            std::array<vk::DescriptorImageInfo, SAMPLER_DESCRIPTOR_COUNT> imageInfos
                = {imageInfoRGB, imageInfoOCV};

            descriptorWrites[1] = vk::WriteDescriptorSet(
                _rasterizationCtx.descriptors.sets[i],
                (uint32_t)BindingLocation::TEXTURE_SAMPLER,
                0,
                imageInfos.size(),
                vk::DescriptorType::eCombinedImageSampler,
                imageInfos.data(),
                nullptr,
                nullptr
            );

            device.updateDescriptorSets(descriptorWrites, nullptr);
        }
    }

    // build graphics pipeline
    {
        // shader modules
        vk::ShaderModule vertShaderModule
            = ShaderCreation::createShaderModule(initCtx.device.logicalDevice, VERTEX_SHADER_PATH);
        vk::ShaderModule fragShaderModule = ShaderCreation::createShaderModule(
            initCtx.device.logicalDevice, FRAGMENT_SHADER_PATH
        );

        std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages
            = {vk::PipelineShaderStageCreateInfo(
                   {}, vk::ShaderStageFlagBits::eVertex, vertShaderModule, "main"
               ),
               vk::PipelineShaderStageCreateInfo(
                   {}, vk::ShaderStageFlagBits::eFragment, fragShaderModule, "main"
               )};

        // vertex input
        vk::VertexInputBindingDescription bindingDescription = Vertex::GetBindingDescription();
        auto attributeDescriptions = Vertex::GetAttributeDescriptions();

        vk::PipelineVertexInputStateCreateInfo vertexInputInfo(
            {}, 1, &bindingDescription, attributeDescriptions.size(), attributeDescriptions.data()
        );

        vk::PipelineInputAssemblyStateCreateInfo inputAssembly(
            {}, vk::PrimitiveTopology::eTriangleList, VK_FALSE
        );

        vk::PipelineDepthStencilStateCreateInfo depthStencil(
            {}, VK_TRUE, VK_TRUE, vk::CompareOp::eLess, VK_FALSE, VK_FALSE
        );

        // viewport + scissor
        std::vector<vk::DynamicState> dynamicStates
            = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};

        vk::PipelineDynamicStateCreateInfo dynamicState(
            {}, dynamicStates.size(), dynamicStates.data()
        );

        vk::Viewport viewport(0.f, 0.f, (float)FB_WIDTH, (float)FB_HEIGHT, 0.f, 1.f);
        vk::Rect2D scissor({0, 0}, {FB_WIDTH, FB_HEIGHT});
        vk::PipelineViewportStateCreateInfo viewportState({}, 1, &viewport, 1, &scissor);

        // rasterizer
        vk::PipelineRasterizationStateCreateInfo rasterizer(
            vk::PipelineRasterizationStateCreateFlags(),
            VK_FALSE, // depthClampEnable
            VK_FALSE, // rasterizerDiscardEnable
            vk::PolygonMode::eFill,
            vk::CullModeFlagBits::eNone,
            vk::FrontFace::eCounterClockwise,
            VK_FALSE, // depthBiasEnable
            0.0f,     // depthBiasConstantFactor
            0.0f,     // depthBiasClamp
            0.0f,     // depthBiasSlopeFactor
            1.0f      // lineWidth
        );

        vk::PipelineMultisampleStateCreateInfo multisampling(
            vk::PipelineMultisampleStateCreateFlags(),
            vk::SampleCountFlagBits::e1,
            VK_FALSE, // sampleShadingEnable
            1.0f,     // minSampleShading
            nullptr,  // pSampleMask
            VK_FALSE, // alphaToCoverageEnable
            VK_FALSE  // alphaToOneEnable
        );

        vk::PipelineColorBlendAttachmentState colorBlendAttachment(
            VK_FALSE, // blendEnable
            vk::BlendFactor::eOne,
            vk::BlendFactor::eZero,
            vk::BlendOp::eAdd,
            vk::BlendFactor::eOne,
            vk::BlendFactor::eZero,
            vk::BlendOp::eAdd,
            vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
                | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
        );

        vk::PipelineColorBlendStateCreateInfo colorBlending(
            vk::PipelineColorBlendStateCreateFlags(),
            VK_FALSE, // logicOpEnable
            vk::LogicOp::eCopy,
            1,
            &colorBlendAttachment,
            {0.0f, 0.0f, 0.0f, 0.0f} // blendConstants
        );

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo(
            vk::PipelineLayoutCreateFlags(), 1, &_rasterizationCtx.descriptors.setLayout, 0, nullptr
        );

        if (device.createPipelineLayout(
                &pipelineLayoutInfo, nullptr, &_rasterizationCtx.pipelineLayout
            )
            != vk::Result::eSuccess) {
            FATAL("Failed to create pipeline layout!");
        }

        vk::GraphicsPipelineCreateInfo pipelineInfo(
            vk::PipelineCreateFlags(),        // flags
            2,                                // stageCount
            shaderStages.data(),              // pStages
            &vertexInputInfo,                 // pVertexInputState
            &inputAssembly,                   // pInputAssemblyState
            nullptr,                          // pTessellationState
            &viewportState,                   // pViewportState
            &rasterizer,                      // pRasterizationState
            &multisampling,                   // pMultisampleState
            &depthStencil,                    // pDepthStencilState
            &colorBlending,                   // pColorBlendState
            &dynamicState,                    // pDynamicState
            _rasterizationCtx.pipelineLayout, // layout
            _renderPass,                      // renderPass
            0,                                // subpass
            vk::Pipeline(),                   // basePipelineHandle
            -1                                // basePipelineIndex
        );

        if (device.createGraphicsPipelines(
                nullptr, 1, &pipelineInfo, nullptr, &_rasterizationCtx.pipeline
            )
            != vk::Result::eSuccess) {
            FATAL("Failed to create graphics pipeline!");
        }

        device.destroyShaderModule(fragShaderModule, nullptr);
        device.destroyShaderModule(vertShaderModule, nullptr);
    }

    // load in hue sphere model
    {
        VQUtils::meshToBuffer(
            HUE_SPHERE_MODEL_PATH,
            initCtx.device,
            _rasterizationCtx.hueSphereMesh.vertexBuffer,
            _rasterizationCtx.hueSphereMesh.indexBuffer
        );
    }
}

void AppTetraHueSphere::cleanupRasterization(TetriumApp::CleanupContext& cleanupCtx)
{
    vk::Device device = cleanupCtx.device.logicalDevice;

    // Destroy pipeline
    if (_rasterizationCtx.pipeline) {
        device.destroyPipeline(_rasterizationCtx.pipeline);
    }

    // Destroy pipeline layout
    if (_rasterizationCtx.pipelineLayout) {
        device.destroyPipelineLayout(_rasterizationCtx.pipelineLayout);
    }

    // Destroy descriptor pool
    if (_rasterizationCtx.descriptors.pool) {
        device.destroyDescriptorPool(_rasterizationCtx.descriptors.pool);
    }

    // Destroy descriptor set layout
    if (_rasterizationCtx.descriptors.setLayout) {
        device.destroyDescriptorSetLayout(_rasterizationCtx.descriptors.setLayout);
    }

    // Destroy UBO buffers
    for (VQBuffer& buffer : _rasterizationCtx.UBOBuffer) {
        buffer.Cleanup();
    }

    // Destroy vertex and index buffers
    _rasterizationCtx.hueSphereMesh.vertexBuffer.Cleanup();
    _rasterizationCtx.hueSphereMesh.indexBuffer.Cleanup();
}

} // namespace TetriumApp
