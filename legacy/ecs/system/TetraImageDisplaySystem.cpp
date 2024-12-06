#if 0
#include "components/ShaderUtils.h"
#include "lib/VQDevice.h"

#include "components/TextureManager.h"

#include "TetraImageDisplaySystem.h"

void TetraImageDisplaySystem::Init(const InitContext* ctx)
{
    _device = ctx->device;
    _textureManager = ctx->textureManager;

    // FIXME: add cleanup functions
    for (VQBuffer& systemUBO : _renderCtx._systemUBOStatic) {
        _device->CreateBufferInPlace(
            sizeof(SystemUBOStatic),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            systemUBO
        );
    }
    createGraphicsPipeline(ctx->rygbRenderPass, ctx);
}

void TetraImageDisplaySystem::Cleanup()
{
    DEBUG("Cleaning up phong rendering system");

    // clean up pipeline
    vkDestroyPipeline(_device->logicalDevice, _renderCtx._pipeline, nullptr);
    vkDestroyPipelineLayout(_device->logicalDevice, _renderCtx._pipelineLayout, nullptr);

    // clean up descriptors
    vkDestroyDescriptorSetLayout(_device->logicalDevice, _descriptorSetLayout, nullptr);
    // descriptor sets automatically cleaned up
    vkDestroyDescriptorPool(_device->logicalDevice, _descriptorPool, nullptr);

    DEBUG("TetraImageDisplaySystem cleaned up");
    // note: texture is handled by TextureManager so no need to clean that up
}

void TetraImageDisplaySystem::buildPipelineForContext(
    const VkRenderPass pass,
    RenderSystemContext& ctx,
    const std::array<VkDescriptorBufferInfo, NUM_FRAME_IN_FLIGHT>& engineUboInfo
)
{

    { // _descriptorSets
        // each frame in flight needs its own descriptorset
        std::vector<VkDescriptorSetLayout> layouts(NUM_FRAME_IN_FLIGHT, this->_descriptorSetLayout);

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = this->_descriptorPool;
        allocInfo.descriptorSetCount = NUM_FRAME_IN_FLIGHT;
        allocInfo.pSetLayouts = layouts.data();

        ASSERT(ctx._descriptorSets.size() == NUM_FRAME_IN_FLIGHT);

        if (vkAllocateDescriptorSets(
                this->_device->logicalDevice, &allocInfo, ctx._descriptorSets.data()
            )
            != VK_SUCCESS) {
            FATAL("Failed to allocate descriptor sets!");
        }
    }

    std::array<VkDescriptorBufferInfo, NUM_FRAME_IN_FLIGHT> systemUboInfo;
    for (int i = 0; i < ctx._systemUBOStatic.size(); i++) {
        systemUboInfo[i].range = sizeof(SystemUBOStatic);
        systemUboInfo[i].buffer = ctx._systemUBOStatic[i].buffer;
        systemUboInfo[i].offset = 0;
    }
    // update descriptor sets
    // note here we only update descriptor sets for static ubo
    // texture array is updated through `updateTextureDescriptorSet`
    for (size_t i = 0; i < NUM_FRAME_IN_FLIGHT; i++) {
        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = ctx._descriptorSets[i];
        descriptorWrites[0].dstBinding = (int)BindingLocation::UBO_STATIC_ENGINE;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &engineUboInfo.at(i);

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = ctx._descriptorSets[i];
        descriptorWrites[1].dstBinding = (int)BindingLocation::UBO_STATIC_SYSTEM;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &systemUboInfo.at(i);

        vkUpdateDescriptorSets(
            _device->logicalDevice, descriptorWrites.size(), descriptorWrites.data(), 0, nullptr
        );
    }

    /////  ---------- shader ---------- /////

    VkShaderModule vertShaderModule = ShaderCreation::createShaderModule(
        _device->logicalDevice, "../shaders/tetra_image.vert.spv"
    );
    VkShaderModule fragShaderModule = ShaderCreation::createShaderModule(
        _device->logicalDevice, "../shaders/tetra_image.frag.spv"
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
    // viewport.x = 0.0f;
    // viewport.y = 0.0f;
    // configure viewport resolution to match the swapchain
    // viewport.width = static_cast<float>(swapchainExtent.width);
    // viewport.height = static_cast<float>(swapchainExtent.height);
    // don't know what they are for, but keep them at 0.0f and 1.0f
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {}; // clips pixels outside the region
    // scissor.offset = {0, 0};
    // scissor.extent
    //         = swapchainExtent; // we don't want to clip anything, so we
    //         use the swapchain extent

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
            _device->logicalDevice, &pipelineLayoutInfo, nullptr, &ctx._pipelineLayout
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
    pipelineInfo.layout = ctx._pipelineLayout;
    pipelineInfo.renderPass = pass;
    pipelineInfo.subpass = 0;

    // only need to specify when deriving from an existing pipeline
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
    pipelineInfo.basePipelineIndex = -1;              // Optional

    if (vkCreateGraphicsPipelines(
            _device->logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &ctx._pipeline
        )
        != VK_SUCCESS) {
        FATAL("Failed to create graphics pipeline!");
    }

    vkDestroyShaderModule(_device->logicalDevice, fragShaderModule, nullptr);
    vkDestroyShaderModule(_device->logicalDevice, vertShaderModule, nullptr);
}

void TetraImageDisplaySystem::createGraphicsPipeline(
    const VkRenderPass renderPass,
    const InitContext* initData
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
        systemUBOStaticBinding.binding = (int)BindingLocation::UBO_STATIC_SYSTEM;
        systemUBOStaticBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        systemUBOStaticBinding.descriptorCount = 1; // number of values in the array
        systemUBOStaticBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        systemUBOStaticBinding.pImmutableSamplers = nullptr;
    }
    { // combined image sampler array -- fragment
        samplerLayoutBinding.binding = (int)BindingLocation::TEXTURE_SAMPLER;
        samplerLayoutBinding.descriptorCount = TEXTURE_ARRAY_SIZE;
        samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerLayoutBinding.stageFlags
            = VK_SHADER_STAGE_FRAGMENT_BIT; // only used on fragment shader;
                                            // (may use for vertex shader for
                                            // height mapping)
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
                _device->logicalDevice, &layoutInfo, nullptr, &this->_descriptorSetLayout
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

        if (vkCreateDescriptorPool(_device->logicalDevice, &poolInfo, nullptr, &_descriptorPool)
            != VK_SUCCESS) {
            FATAL("Failed to create descriptor pool!");
        }
    }

    buildPipelineForContext(renderPass, _renderCtx, initData->engineUBOStaticDescriptorBufferInfo);
}

void TetraImageDisplaySystem::updateTextureDescriptorSet()
{
    DEBUG("updating texture descirptor set");
    for (size_t i = 0; i < NUM_FRAME_IN_FLIGHT; i++) {
        std::array<VkWriteDescriptorSet, 1> descriptorWrites{};
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = _renderCtx._descriptorSets[i];
        descriptorWrites[0].dstBinding = (int)BindingLocation::TEXTURE_SAMPLER;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[0].descriptorCount
            = _textureDescriptorInfoIdx; // descriptors are 0-indexed, +1 for
                                         // the # of valid samplers
        descriptorWrites[0].pImageInfo = _textureDescriptorInfo.data();

        DEBUG("descriptor count: {}", descriptorWrites[0].descriptorCount);

        vkUpdateDescriptorSets(
            _device->logicalDevice, descriptorWrites.size(), descriptorWrites.data(), 0, nullptr
        );
    }
};

void TetraImageDisplaySystem::Tick(const TickContext* tickCtx, const std::string& texturePath)
{
    const RenderSystemContext& renderCtx = _renderCtx;

    VkCommandBuffer CB = tickCtx->graphics.CB;
    int frameIdx = tickCtx->graphics.currentFrameInFlight;

    int textureOffset = 0;
    auto it = _textureDescriptorIndices.find(texturePath);
    if (it == _textureDescriptorIndices.end()) {
        FATAL("Cannot find texture for {}", texturePath);
    } else {
        textureOffset = it->second;
    }

    // update system UBO to point the correct texture descriptor index
    flushSystemUBO(frameIdx, textureOffset);

    vkCmdBindPipeline(CB, VK_PIPELINE_BIND_POINT_GRAPHICS, renderCtx._pipeline);
    vkCmdBindDescriptorSets(
        CB,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        renderCtx._pipelineLayout,
        0,
        1,
        &renderCtx._descriptorSets[frameIdx],
        0,
        nullptr
    );
    vkCmdDraw(CB, 3, 1, 0, 0);
}

void TetraImageDisplaySystem::flushSystemUBO(uint32_t frame, int textureIndex)
{
    VQBuffer& buf = _renderCtx._systemUBOStatic[frame];
    SystemUBOStatic ubo{.textureIndex = textureIndex};
    memcpy(buf.bufferAddress, &ubo, sizeof(ubo));
}

void TetraImageDisplaySystem::LoadTexture(const std::string& imagePath)
{
    // load texture into textures[textureOffset]
    DEBUG("loading texture into {}", _textureDescriptorInfoIdx);
    NEEDS_IMPLEMENTATION();
    // _textureManager->GetDescriptorImageInfo(
    //     imagePath, _textureDescriptorInfo[_textureDescriptorInfoIdx]
    // );
    _textureDescriptorIndices[imagePath] = _textureDescriptorInfoIdx;
    _textureDescriptorInfoIdx++;
    updateTextureDescriptorSet();
}
#endif
