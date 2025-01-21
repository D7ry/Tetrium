#include "HueSphere.h"
#include "Pathing.h"
#include "structs/Vertex.h"
#include "components/ShaderUtils.h"
#include "imgui.h"
#include <lib/VQUtils.h>

std::string VERTEX_SHADER_PATH = ASSETS_PATH + "apps/AppPainter/shaders/hue_sphere.vert.spv";
std::string FRAGMENT_SHADER_PATH = ASSETS_PATH + "apps/AppPainter/shaders/hue_sphere.frag.spv";

std::string HUE_SPHERE_UGLY_MODEL_PATH
    = ASSETS_PATH + "apps/AppTetraHueSphere/fibonacci_sampled.obj";
std::string HUE_SPHERE_UGLY_TEXTURE_PATH_RGB
    = ASSETS_PATH + "apps/AppTetraHueSphere/cubemaps/cubemap_RGB.png";
std::string HUE_SPHERE_UGLY_TEXTURE_PATH_OCV
    = ASSETS_PATH + "apps/AppTetraHueSphere/cubemaps/cubemap_OCV.png";

std::string HUE_SPHERE_PRETTY_MODEL_PATH = ASSETS_PATH + "apps/AppPainter/sphere.obj";

std::string HUE_SPHERE_PRETTY_TEXTURE_PATH_RGB = HUE_SPHERE_UGLY_TEXTURE_PATH_RGB;
std::string HUE_SPHERE_PRETTY_TEXTURE_PATH_OCV = HUE_SPHERE_UGLY_TEXTURE_PATH_OCV;

void HueSphere::DrawHuesphereInImGui(const TetriumApp::TickContextImGui& ctx, float scale, float hRotation, float vRotation)
{
    auto& fb = _renderContexts[ctx.colorSpace].fb;
    
    glm::vec3 defaultScale(1, 1, 1);
    _rasterizationCtx.hueSpheretransform.scale = defaultScale * (_scaleToSaturation ? scale : 1);
    //FIXME: fix rotation
    _rasterizationCtx.hueSpheretransform.rotation = glm::vec3(90 ,hRotation * 365,0);

    ImGui::Image(fb.GetImGuiTextureId(), ImVec2{(float)_fbWidth, (float)_fbHeight});

    auto io = ImGui::GetIO();

    if (ImGui::IsItemHovered()) {
        // if (io.MouseWheel != 0) {
        //     _rasterizationCtx.camera.Move(io.MouseWheel * 0.2, 0, 0);
        // };
        if (io.MouseDown[0]) {
            _rasterizationCtx.camera.ModRotation(-io.MouseDelta.x, -io.MouseDelta.y, 0);
        }
        // minecraft movement controls
        if (io.KeysDown[ImGuiKey_Space]) {
            _rasterizationCtx.camera.Move(0, 0, io.DeltaTime);
        }
        if (io.KeyShift) {
            _rasterizationCtx.camera.Move(0, 0, -io.DeltaTime);
        }
        if (io.KeysDown[ImGuiKey_W]) {
            _rasterizationCtx.camera.Move(io.DeltaTime, 0, 0);
        }
        if (io.KeysDown[ImGuiKey_S]) {
            _rasterizationCtx.camera.Move(-io.DeltaTime, 0, 0);
        }
        if (io.KeysDown[ImGuiKey_A]) {
            _rasterizationCtx.camera.Move(0, io.DeltaTime, 0);
        }
        if (io.KeysDown[ImGuiKey_D]) {
            _rasterizationCtx.camera.Move(0, -io.DeltaTime, 0);
        }
    }

    ImGui::SeparatorText("Settings");

    ImGui::Text("Projection Type");
    ImGui::RadioButton(
        "Orthographic",
        (int*)&_rasterizationCtx.projectionType,
        (int)ProjectionType::Orthographic
    );
    ImGui::SameLine();
    ImGui::RadioButton(
        "Perspective", (int*)&_rasterizationCtx.projectionType, (int)ProjectionType::Perspective
    );

    ImGui::SliderFloat("Orthographic Width", &_rasterizationCtx.orthoWidth, 0.1f, 10.f);
    ImGui::SliderFloat("Perspective FOV", &_rasterizationCtx.perspectiveFOV, 1.f, 130.f);

    ImGui::Checkbox("Scale To Saturation", &_scaleToSaturation);
}

void HueSphere::TickVulkan(TetriumApp::TickContextVulkan& ctx)
{
    // flush UBO
    {
        UBO* pUBO = reinterpret_cast<UBO*>(
            _rasterizationCtx.UBOBuffer.at(ctx.currentFrameInFlight).bufferAddress
        );

        pUBO->view = _rasterizationCtx.camera.GetViewMatrix();

        vk::Extent2D extent = vk::Extent2D(_fbWidth, _fbHeight);

        float aspectRatio = static_cast<float>(extent.width) / static_cast<float>(extent.height);
        float orthoWidth = _rasterizationCtx.orthoWidth;
        float orthoHeight = orthoWidth / aspectRatio; // Calculate height based on aspect ratio

        glm::mat4 projectionMatrix = _rasterizationCtx.projectionType == ProjectionType::Perspective
                                         ? glm::perspective(
                                               glm::radians(_rasterizationCtx.perspectiveFOV),
                                               aspectRatio,
                                               DEFAULTS::ZNEAR,
                                               DEFAULTS::ZFAR
                                           )
                                         : glm::ortho(
                                               -orthoWidth / 2.0f,  // left
                                               orthoWidth / 2.0f,   // right
                                               -orthoHeight / 2.0f, // bottom
                                               orthoHeight / 2.0f,  // top
                                               DEFAULTS::ZNEAR,     // near
                                               DEFAULTS::ZFAR       // far
                                           );

        projectionMatrix[1][1] *= -1; // invert for vulkan coord system
        pUBO->proj = projectionMatrix;
        pUBO->model = _rasterizationCtx.hueSpheretransform.GetModelMatrix();

        // [0, 1, 2, 3] -> [uglyRGB, uglyOCV, prettyRGB, prettyOCV]
        int texIndex = 0;
        if (ctx.colorSpace == ColorSpace::OCV) {
            texIndex += 1;
        }
        pUBO->textureIndex = texIndex;
    }

    auto CB = ctx.commandBuffer;
    RenderContext& renderCtx = _renderContexts[ctx.currentFrameInFlight];
    // render to the correct framebuffer&texture

    CB.beginRenderPass(
        vk::RenderPassBeginInfo(
            _renderPass,
            renderCtx.fb.GetFrameBuffer(),
            vk::Rect2D({0, 0}, {_fbWidth, _fbHeight}),
            _clearValues.size(),
            _clearValues.data()
        ),
        vk::SubpassContents::eInline
    );
    CB.bindPipeline(vk::PipelineBindPoint::eGraphics, _rasterizationCtx.pipeline);

    CB.setViewport(0, vk::Viewport(0.f, 0.f, _fbWidth, _fbHeight, 0.f, 1.f));
    CB.setScissor(0, vk::Rect2D({0, 0}, {_fbWidth, _fbHeight}));
    CB.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        _rasterizationCtx.pipelineLayout,
        0,
        _rasterizationCtx.descriptors.sets[ctx.currentFrameInFlight],
        nullptr
    );

    Mesh& mesh = _rasterizationCtx.prettySphereMesh;
    CB.bindVertexBuffers(0, vk::Buffer(mesh.vertexBuffer.buffer), {0});
    CB.bindIndexBuffer(vk::Buffer(mesh.indexBuffer.buffer), 0, vk::IndexType(VQ_BUFFER_INDEX_TYPE));

    CB.drawIndexed(mesh.indexBuffer.numIndices, 1, 0, 0, 0);

    CB.endRenderPass();
}

void HueSphere::Cleanup(TetriumApp::CleanupContext& ctx)
{
    DEBUG("Cleaning up Huesphere...");
    cleanupRasterization(ctx);
    for (RenderContext& renderCtx : _renderContexts) {
        cleanupRenderContext(renderCtx, ctx);
    }

    vk::Device device = ctx.device.logicalDevice;
    device.destroyRenderPass(_renderPass);
    //cleanupCubemapTexture(ctx);
};

void HueSphere::Init(TetriumApp::InitContext& ctx, uint32_t fbWidth, uint32_t fbHeight, uint32_t cubemapSize, TextureFrameBuffer& cubemapTexture)
{
    DEBUG("Initializing Huesphere...");
    _fbWidth = fbWidth;
    _fbHeight = fbHeight;
    //initCubemapTexture(ctx, cubemapSize);

    initRenderPass(ctx);

    // create all render contexts,
    // NOTE: fb creation requires render pass to be created
    for (int i = 0; i < NUM_FRAME_IN_FLIGHT; i++) {
        initRenderContext(_renderContexts[i], ctx);
    }

    _clearValues[0].color = {0.0f, 0.0f, 0.0f, 0.f};
    _clearValues[1].depthStencil = vk::ClearDepthStencilValue(1.f, 0.f);

    initRasterization(ctx, cubemapTexture);

    _rasterizationCtx.camera.SetPosition(-2, 0, 0);
};

void HueSphere::cleanupRenderContext(
    RenderContext& ctx,
    TetriumApp::CleanupContext& cleanupCtx
)
{
    ctx.fb.Cleanup();
}

void HueSphere::initRenderContext(RenderContext& ctx, TetriumApp::InitContext& initCtx)
{
    ctx.fb.Init(
        initCtx.device.logicalDevice,
        initCtx.device.physicalDevice,
        _renderPass,
        _fbWidth,
        _fbHeight,
        VK_FORMAT_R8G8B8A8_SRGB,
        initCtx.device.depthFormat
    );
}

void HueSphere::initRenderPass(TetriumApp::InitContext& initCtx)
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
        colorAttachment.format = VK_FORMAT_R8G8B8A8_SRGB;
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

void HueSphere::initRasterization(TetriumApp::InitContext& initCtx, TextureFrameBuffer& cubemapTexture)
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
        const size_t SAMPLER_DESCRIPTOR_COUNT = 1; // [uglyRGB, uglyOCV, prettyRGB, prettyOCV]

        vk::DescriptorSetLayoutBinding uboLayoutBinding(
            (uint32_t)BindingLocation::UBO,
            vk::DescriptorType::eUniformBuffer,
            UBO_DESCRIPTOR_COUNT,
            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
        );

        vk::DescriptorSetLayoutBinding samplerLayoutBinding(
            (uint32_t)BindingLocation::TEXTURE_SAMPLER,
            vk::DescriptorType::eCombinedImageSampler,
            1,
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


            vk::DescriptorImageInfo cubemapTextureInfo = cubemapTexture.GetDescriptorImageInfo();

            descriptorWrites[1] = vk::WriteDescriptorSet(
                _rasterizationCtx.descriptors.sets[i],
                (uint32_t)BindingLocation::TEXTURE_SAMPLER,
                0,
                1,
                vk::DescriptorType::eCombinedImageSampler,
                &cubemapTextureInfo,
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
            = ShaderCreation::createShaderModule(initCtx.device.logicalDevice, VERTEX_SHADER_PATH.c_str());
        vk::ShaderModule fragShaderModule = ShaderCreation::createShaderModule(
            initCtx.device.logicalDevice, FRAGMENT_SHADER_PATH.c_str()
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

        vk::Viewport viewport(0.f, 0.f, (float)_fbWidth, (float)_fbHeight, 0.f, 1.f);
        vk::Rect2D scissor({0, 0}, {_fbWidth, _fbHeight});
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
            HUE_SPHERE_PRETTY_MODEL_PATH.c_str(),
            initCtx.device,
            _rasterizationCtx.prettySphereMesh.vertexBuffer,
            _rasterizationCtx.prettySphereMesh.indexBuffer
        );
    }

    _rasterizationCtx.hueSpheretransform.rotation = glm::vec3(90,0, 0);
}

void HueSphere::cleanupRasterization(TetriumApp::CleanupContext& cleanupCtx)
{
    // clean up texture
    for (auto handle : _rasterizationCtx.loadedTextures) {
        cleanupCtx.api.UnloadTexture(handle);
    }
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
    _rasterizationCtx.prettySphereMesh.vertexBuffer.Cleanup();
    _rasterizationCtx.prettySphereMesh.indexBuffer.Cleanup();
}

static uint32_t findMemoryType(
    VkPhysicalDevice physicalDevice,
    uint32_t typeFilter,
    VkMemoryPropertyFlags properties
)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i))
            && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    PANIC("Failed to find suitable memory type!");
}

void HueSphere::initCubemapTexture(TetriumApp::InitContext& ctx, uint32_t faceSize)
{
    auto& device = ctx.device;
    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    imageCreateInfo.extent.width = faceSize;
    imageCreateInfo.extent.height = faceSize;
    imageCreateInfo.extent.depth = 1;
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 6;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VK_CHECK(vkCreateImage(ctx.device.Get(), &imageCreateInfo, nullptr, &_cubemapTexture.image));
    
    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(ctx.device.Get(), _cubemapTexture.image, &memRequirements);
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(
        device.physicalDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    VK_CHECK(
        vkAllocateMemory(device.logicalDevice, &allocInfo, nullptr, &_cubemapTexture.imageMemory)
    )
    VK_CHECK(vkBindImageMemory(
        device.logicalDevice, _cubemapTexture.image, _cubemapTexture.imageMemory, 0
    ))

    VkImageViewCreateInfo viewCreateInfo = {};
    viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCreateInfo.image = _cubemapTexture.image;
    viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    viewCreateInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewCreateInfo.subresourceRange.baseMipLevel = 0;
    viewCreateInfo.subresourceRange.levelCount = 1;
    viewCreateInfo.subresourceRange.baseArrayLayer = 0;
    viewCreateInfo.subresourceRange.layerCount = 6;

    VK_CHECK(
        vkCreateImageView(device.logicalDevice, &viewCreateInfo, nullptr, &_cubemapTexture.view)
    );

    // Create sampler
    VkSamplerCreateInfo samplerCreateInfo = {};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.anisotropyEnable = VK_FALSE;
    samplerCreateInfo.maxAnisotropy = 0;
    // samplerCreateInfo.maxAnisotropy = 16;
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
    samplerCreateInfo.compareEnable = VK_FALSE;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    VK_CHECK(vkCreateSampler(
        device.logicalDevice, &samplerCreateInfo, nullptr, &_cubemapTexture.sampler
    ));
}

void HueSphere::cleanupCubemapTexture(TetriumApp::CleanupContext& ctx)
{
    vkDestroySampler(ctx.device.logicalDevice, _cubemapTexture.sampler, nullptr);
    vkDestroyImageView(ctx.device.logicalDevice, _cubemapTexture.view, nullptr);
    vkDestroyImage(ctx.device.logicalDevice, _cubemapTexture.image, nullptr);
    vkFreeMemory(ctx.device.logicalDevice, _cubemapTexture.imageMemory, nullptr);
}
