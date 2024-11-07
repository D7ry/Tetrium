// Handles all RYGB transform logic

#include "Tetrium.h"
#include "components/ShaderUtils.h"

// internal context of RYGB->ROCV transform
namespace Tetrium_RYGB
{
enum class BindingLocation : unsigned int
{
    UBO_STATIC_ENGINE = 0,
    UBO_STATIC_RYGB = 1,
    FRAMEBUFFER_TEXTURE_SAMPLER = 2
};

struct SystemUBOStatic
{
    glm::mat4 transformMat; // 4x4 transform matrix
    int frameBufferIdx;     // which frame in the sampler array to choose from
    int toRGB;              // toOCV = !toRGB;
};

VkDescriptorSetLayout _descriptorSetLayout = VK_NULL_HANDLE;
VkDescriptorPool _descriptorPool = VK_NULL_HANDLE;
VkDevice _device = VK_NULL_HANDLE;
std::array<VQBuffer, NUM_FRAME_IN_FLIGHT> _systemUBO;

std::array<VkDescriptorSet, NUM_FRAME_IN_FLIGHT> descriptorSets;
VkPipeline pipeline = VK_NULL_HANDLE;
VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
std::vector<VkSampler> samplers;

void flushSystemUBO(uint32_t frame, int swapChainImageIndex, int isRGB)
{
    VQBuffer& buf = _systemUBO[frame];
    SystemUBOStatic ubo{
        .transformMat = glm::mat4(1), .frameBufferIdx = swapChainImageIndex, .toRGB = isRGB
    };
    memcpy(buf.bufferAddress, &ubo, sizeof(ubo));
}

void initSystemUBO(VQDevice* device)
{
    for (VQBuffer& systemUBO : _systemUBO) {
        device->CreateBufferInPlace(
            sizeof(SystemUBOStatic),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            systemUBO
        );
    }
}

void cleanupSystemUBO()
{
    for (VQBuffer& systemUBO : _systemUBO) {
        systemUBO.Cleanup();
    }
}

void createGraphicsPipeline(
    const VkRenderPass renderPass,
    const std::vector<VkImageView> rgybFrameBufferImageView,
    const InitContext* engineInitCtx
)
{
    /////  ---------- descriptor ---------- /////
    VkDescriptorSetLayoutBinding engineUBOStaticBinding{};
    VkDescriptorSetLayoutBinding systemUBOStaticBinding{};
    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    { // engine UBO static -- vertex
        engineUBOStaticBinding.binding = (int)BindingLocation::UBO_STATIC_ENGINE;
        engineUBOStaticBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        engineUBOStaticBinding.descriptorCount = 1; // number of values in the array
        engineUBOStaticBinding.stageFlags
            = VK_SHADER_STAGE_VERTEX_BIT;                    // only used in vertex shader
        engineUBOStaticBinding.pImmutableSamplers = nullptr; // Optional
    }
    { // system UBO static -- fragment
        systemUBOStaticBinding.binding = (int)BindingLocation::UBO_STATIC_RYGB;
        systemUBOStaticBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        systemUBOStaticBinding.descriptorCount = 1; // number of values in the array
        systemUBOStaticBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        systemUBOStaticBinding.pImmutableSamplers = nullptr;
    }
    { // framebuffer texture sampler -- fragment
        samplerLayoutBinding.binding = (int)BindingLocation::FRAMEBUFFER_TEXTURE_SAMPLER;
        samplerLayoutBinding.descriptorCount = 10;
        samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        samplerLayoutBinding.pImmutableSamplers = nullptr;
    }
    std::array<VkDescriptorSetLayoutBinding, 3> bindings
        = {engineUBOStaticBinding, systemUBOStaticBinding, samplerLayoutBinding};
    { // _descriptorSetLayout
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = bindings.size(); // number of bindings
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(
                engineInitCtx->device->logicalDevice, &layoutInfo, nullptr, &_descriptorSetLayout
            )
            != VK_SUCCESS) {
            FATAL("Failed to create descriptor set layout!");
        }
    }

    {                                         // _descriptorPool
        uint32_t numDescriptorPerType = 5000; // each render group has its own set of descriptors
        VkDescriptorPoolSize poolSizes[]
            = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, static_cast<uint32_t>(numDescriptorPerType)},
               {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                static_cast<uint32_t>(numDescriptorPerType)},
               {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                static_cast<uint32_t>(numDescriptorPerType)}};

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount
            = sizeof(poolSizes) / sizeof(VkDescriptorPoolSize); // number of pool sizes
        poolInfo.pPoolSizes = poolSizes;
        poolInfo.maxSets = NUM_FRAME_IN_FLIGHT * poolInfo.poolSizeCount;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

        if (vkCreateDescriptorPool(
                engineInitCtx->device->logicalDevice, &poolInfo, nullptr, &_descriptorPool
            )
            != VK_SUCCESS) {
            FATAL("Failed to create descriptor pool!");
        }
    }

    // build up graphics pipeline
    { // _descriptorSets
        // each frame in flight needs its own descriptorset
        std::vector<VkDescriptorSetLayout> layouts(NUM_FRAME_IN_FLIGHT, _descriptorSetLayout);

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = _descriptorPool;
        allocInfo.descriptorSetCount = NUM_FRAME_IN_FLIGHT;
        allocInfo.pSetLayouts = layouts.data();

        ASSERT(descriptorSets.size() == NUM_FRAME_IN_FLIGHT);

        if (vkAllocateDescriptorSets(
                engineInitCtx->device->logicalDevice, &allocInfo, descriptorSets.data()
            )
            != VK_SUCCESS) {
            FATAL("Failed to allocate descriptor sets!");
        }
    }

    // update descriptor sets
    for (size_t i = 0; i < NUM_FRAME_IN_FLIGHT; i++) {
        std::array<VkWriteDescriptorSet, 3> descriptorWrites{};
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = descriptorSets[i];
        descriptorWrites[0].dstBinding = (int)BindingLocation::UBO_STATIC_ENGINE;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &engineInitCtx->engineUBOStaticDescriptorBufferInfo.at(i);

        VkDescriptorBufferInfo systemUboInfo{};
        systemUboInfo.range = sizeof(SystemUBOStatic);
        systemUboInfo.buffer = _systemUBO[i].buffer;
        systemUboInfo.offset = 0;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = descriptorSets[i];
        descriptorWrites[1].dstBinding = (int)BindingLocation::UBO_STATIC_RYGB;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &systemUboInfo;

        std::vector<VkDescriptorImageInfo> imageInfo(rgybFrameBufferImageView.size());
        for (int i = 0; i < rgybFrameBufferImageView.size(); i++) {
            imageInfo[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo[i].imageView = rgybFrameBufferImageView[i];
            imageInfo[i].sampler = samplers[i];
        }

        descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet = descriptorSets[i];
        descriptorWrites[2].dstBinding = (int)BindingLocation::FRAMEBUFFER_TEXTURE_SAMPLER;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[2].descriptorCount = rgybFrameBufferImageView.size();
        descriptorWrites[2].pImageInfo = imageInfo.data();

        vkUpdateDescriptorSets(
            engineInitCtx->device->logicalDevice,
            descriptorWrites.size(),
            descriptorWrites.data(),
            0,
            nullptr
        );
    }

    /////  ---------- shader ---------- /////

    VkShaderModule vertShaderModule = ShaderCreation::createShaderModule(
        engineInitCtx->device->logicalDevice, "../shaders/rygb_to_rocv.vert.spv"
    );
    VkShaderModule fragShaderModule = ShaderCreation::createShaderModule(
        engineInitCtx->device->logicalDevice, "../shaders/rygb_to_rocv.frag.spv"
    );

    VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    // static stages(vertex and frag shader)
    VkPipelineShaderStageCreateInfo shaderStages[]
        = {vertShaderStageInfo, fragShaderStageInfo}; // put the 2 stages together.

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    { // set up vertex input info
        // we don't input vertex buffer so no need for descriptions
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 0;
        vertexInputInfo.pVertexBindingDescriptions = nullptr;
        vertexInputInfo.vertexAttributeDescriptionCount = 0;
        vertexInputInfo.pVertexAttributeDescriptions = 0;
    }

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly
        = {}; // describes what kind of geometry will be drawn from the
              // vertices and if primitive restart should be enabled.
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; // draw triangles
    inputAssembly.primitiveRestartEnable = VK_FALSE;              // don't restart primitives

    // depth and stencil state
    VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo{};
    depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilCreateInfo.depthTestEnable = VK_TRUE;
    depthStencilCreateInfo.depthWriteEnable = VK_TRUE;

    depthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS; // low depth == closer object ->
                                                                // cull far object

    depthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;
    depthStencilCreateInfo.minDepthBounds = 0.0f; // Optional
    depthStencilCreateInfo.maxDepthBounds = 1.0f; // Optional

    depthStencilCreateInfo.stencilTestEnable = VK_FALSE;
    depthStencilCreateInfo.front = {}; // Optional
    depthStencilCreateInfo.back = {};  // Optional
                                       //

    // dynamic states
    // specifying the dynamic states here so that they are not defined
    // during the pipeline creation, they can be changed at draw time.
    std::vector<VkDynamicState> dynamicStates
        = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    INFO("setting up viewport and scissors...");
    VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
    dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

    // viewport and scissors
    VkViewport viewport = {}; // describes the region of the framebuffer
                              // that the output will be rendered to.
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {}; // clips pixels outside the region

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.pScissors = &scissor;

    INFO("setting up rasterizer...");
    // rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer
        = {}; // does the actual rasterization and outputs fragments
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE; // if true, fragments beyond the near and far planes
                                            // are clamped instead of discarded
    rasterizer.rasterizerDiscardEnable
        = VK_FALSE; // if true, geometry never passes through the rasterizer
                    // stage(nothing it put into the frame buffer)
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL; // fill the area of the
                                                   // polygon with fragments
    rasterizer.lineWidth = 1.0f; // thickness of lines in terms of number of fragments
    // rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;    // cull back faces
    rasterizer.cullMode = VK_CULL_MODE_NONE; // don't cull any faces
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;     // depth biasing
    rasterizer.depthBiasConstantFactor = 0.0f; // optional
    rasterizer.depthBiasClamp = 0.0f;          // optional
    rasterizer.depthBiasSlopeFactor = 0.0f;    // optional

    INFO("setting up multisampling...");
    // Multi-sampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f;          // Optional
    multisampling.pSampleMask = nullptr;            // Optional
    multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
    multisampling.alphaToOneEnable = VK_FALSE;      // Optional

    // depth buffering
    // we're not there yet, so we'll disable it for now

    // color blending - combining color from the fragment shader with color
    // that is already in the framebuffer

    INFO("setting up color blending...");
    // controls configuration per attached frame buffer
    // https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkBlendOp.html
    // https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkBlendFactor.html
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                                          | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE; // set to true to enable blending
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;  // Optional
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;             // Optional
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;  // Optional
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;             // Optional

    // global color blending settings
    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;   // set to true to enable blending
    colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f; // Optional
    colorBlending.blendConstants[1] = 0.0f; // Optional
    colorBlending.blendConstants[2] = 0.0f; // Optional
    colorBlending.blendConstants[3] = 0.0f; // Optional

    INFO("setting up pipeline layout...");
    // pipeline layout - controlling uniform values
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &_descriptorSetLayout; // bind our own layout
    pipelineLayoutInfo.pushConstantRangeCount = 0;          // Optional
    pipelineLayoutInfo.pPushConstantRanges = nullptr;       // Optional

    if (vkCreatePipelineLayout(
            engineInitCtx->device->logicalDevice, &pipelineLayoutInfo, nullptr, &pipelineLayout
        )
        != VK_SUCCESS) {
        FATAL("Failed to create pipeline layout!");
    }

    // put things together
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = nullptr; // Optional
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicStateCreateInfo;
    pipelineInfo.pDepthStencilState = &depthStencilCreateInfo;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    // only need to specify when deriving from an existing pipeline
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
    pipelineInfo.basePipelineIndex = -1;              // Optional

    if (vkCreateGraphicsPipelines(
            engineInitCtx->device->logicalDevice,
            VK_NULL_HANDLE,
            1,
            &pipelineInfo,
            nullptr,
            &pipeline
        )
        != VK_SUCCESS) {
        FATAL("Failed to create graphics pipeline!");
    }

    vkDestroyShaderModule(engineInitCtx->device->logicalDevice, fragShaderModule, nullptr);
    vkDestroyShaderModule(engineInitCtx->device->logicalDevice, vertShaderModule, nullptr);
}

void cleanupGraphicsPipeline()
{
    vkDestroyPipeline(_device, pipeline, nullptr);
    vkDestroyPipelineLayout(_device, pipelineLayout, nullptr);

    // clean up descriptors
    vkDestroyDescriptorSetLayout(_device, _descriptorSetLayout, nullptr);
    // descriptor sets automatically cleaned up with pool
    vkDestroyDescriptorPool(_device, _descriptorPool, nullptr);
}

void createFramebufferSampler(int numFrameBuffers)
{
    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplers.resize(numFrameBuffers);
    for (int i = 0; i < numFrameBuffers; i++) {
        vkCreateSampler(_device, &samplerInfo, nullptr, &samplers.at(i));
    }
}

void cleanupFramebufferSampler()
{
    for (int i = 0; i < samplers.size(); i++) {
        vkDestroySampler(_device, samplers.at(i), NULL);
    }
}

void Init(const InitContext* ctx, const std::vector<VkImageView>& rgybFrameBufferImageView)
{
    _device = ctx->device->logicalDevice;
    initSystemUBO(ctx->device);

    // NOTE: sampler must be created before pipeline as it's a part of descriptor layout
    createFramebufferSampler(rgybFrameBufferImageView.size());

    createGraphicsPipeline(ctx->rocvTransformRenderPass, rgybFrameBufferImageView, ctx);
}

void Cleanup()
{
    INFO("Cleanup RYGB");
    cleanupSystemUBO();
    cleanupGraphicsPipeline();
    cleanupFramebufferSampler();
}

} // namespace Tetrium_RYGB

void Tetrium::initRYGB2ROCVTransform(InitContext* ctx)
{
    Tetrium_RYGB::Init(ctx, _renderContextRYGB.virtualFrameBuffer.imageView);
}

void Tetrium::cleanupRYGB2ROCVTransform() { Tetrium_RYGB::Cleanup(); }

// Given the content in rgyb frame buffer,
// transform the content onto either RGB or OCV representation, and paint onto
// the swapchain's frame buffer.
//
// Internally, the remapping is done by post-processing a full-screen quad:
// the full-screen quad samples the rgyb FB's texture and performs matmul on the color

void Tetrium::transformToROCVframeBuffer(
    VirtualFrameBuffer& rgybFrameBuffer,
    SwapChainContext& rocvSwapChain,
    uint32_t frameIdx,
    uint32_t swapChainImageIndex,
    ColorSpace colorSpace,
    vk::CommandBuffer CB,
    bool skip
)
{
    Tetrium_RYGB::flushSystemUBO(frameIdx, swapChainImageIndex, colorSpace == RGB ? 1 : 0);

    // transition image to be sampled by shader
    // TODO: may not necessary, after adding imgui pass to be last pass among
    // RGYB passes -- can set the final layout of imgui pass to be shader_read_only
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = rgybFrameBuffer.image[swapChainImageIndex];
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    // vkCmdPipelineBarrier(
    //     CB,
    //     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    //     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    //     0,
    //     0,
    //     nullptr,
    //     0,
    //     nullptr,
    //     1,
    //     &barrier
    // );

    vk::Extent2D extend = _swapChain.extent;
    vk::Rect2D renderArea(VkOffset2D{0, 0}, extend);
    vk::RenderPassBeginInfo renderPassBeginInfo(
        _rocvTransformRenderPass,
        rocvSwapChain.frameBuffer[swapChainImageIndex],
        renderArea,
        _clearValues.size(),
        _clearValues.data(),
        nullptr
    );

    CB.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);

    if (!skip) {
        CB.bindPipeline(vk::PipelineBindPoint::eGraphics, Tetrium_RYGB::pipeline);
        vkCmdBindDescriptorSets(
            CB,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            Tetrium_RYGB::pipelineLayout,
            0,
            1,
            &Tetrium_RYGB::descriptorSets[frameIdx],
            0,
            nullptr
        );

        vkCmdDraw(CB, 3, 1, 0, 0);
    }

    CB.endRenderPass();
}
