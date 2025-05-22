// Color picker implementation for the painter app.
/**
 * real-time 4D color picker.
 * The user visually specifies the four dimensions by:
 * 1. select luminance and saturation using two sliders (2 dims)
 * 2. select a point from the tetrachromatic hue sphere, using a flattened out cubemap(2 dims)
 * The cubmap texture is computed real-time based on luminance and saturation
 *
 * Internally, the fragment shader performs all the color math and writes the RYGB cubemap
 * to a texture. The CPU then addresses the texture to get the selected RYGB color.
 * Presenting the RYGB cubemap is similar to presenting the canvas, where we
 * transform the RYGB color space into RGB/OCV using a 4x3 mat.
 *
 * At Tick time:
 * ---- ImGUI Tick
 * The color picker uses the current offset to read from the CPU-accessilbe cubemap texture.
 * ---- Vulkan Tick
 * If there is a change to either saturation and luminance:
 * | 1. the cubemap texture generation pass runs, updating cubemap texture using saturation and luminance,
 * |   for each coordinate on the cubemap
 * | 2. the cubemap texture gets copied to the CPU-accessilbe cubemap staging buffer
 * | 3. the cubemap texture gets sampled by the RYGBToViewSpaceContext pass to generate view space(RGB/OCV) texture
 * |    for ImGui to render
 * Render the RYGB cubemap texture, transforming it to RGB/OCV space using the 4x3 mat.
 *
 */

#include "apps/AppPainter.h"
#include <Pathing.h>
#include <components/ShaderUtils.h>

namespace TetriumApp
{

void AppPainter::ColorPicker::initRYGBTransform(TetriumApp::InitContext& ctx)
{
    vk::Device device = ctx.device.Get();
    /* allocate descriptor sets */
    {
        std::vector<vk::DescriptorSetLayout> layouts(
            NUM_FRAME_IN_FLIGHT, _rygbToViewSpaceCtx->descriptorSetLayout
        );
        vk::DescriptorSetAllocateInfo descriptorSetAllocateInfo(
            _rygbToViewSpaceCtx->descriptorPool, NUM_FRAME_IN_FLIGHT, layouts.data()
        );
        std::vector<vk::DescriptorSet> res
            = device.allocateDescriptorSets(descriptorSetAllocateInfo);
        ASSERT(res.size() == NUM_FRAME_IN_FLIGHT); // Ensure the allocation was successful
        for (size_t i = 0; i < res.size(); i++) {
            _rygbTransformDescriptorSets[i] = res[i];
        }
        res = device.allocateDescriptorSets(descriptorSetAllocateInfo);
        ASSERT(res.size() == NUM_FRAME_IN_FLIGHT); // Ensure the allocation was successful
        for (size_t i = 0; i < res.size(); i++) {
            _rygbTransformDescriptorSetsColorSquare[i] = res[i];
        }
    }
    ASSERT(!_rygbTransformDescriptorSets.empty()) // Ensure the array is not empty
    ASSERT(!_rygbTransformDescriptorSetsColorSquare.empty()) // Ensure the array is not empty

    /* update descriptor sets */
    {
        for (size_t i = 0; i < _rygbTransformDescriptorSets.size(); ++i) {
            vk::DescriptorSet descriptorSet = _rygbTransformDescriptorSets[i];

            vk::DescriptorBufferInfo bufferInfo(
                _rygbToViewSpaceCtx->ubo[i].buffer, 0, sizeof(RYGBToViewSpaceUBO)
            );

            vk::DescriptorImageInfo imageInfo(
                _rygbToViewSpaceCtx->samplers[i],
                _cubemapTexture.GetImageView(),
                vk::ImageLayout::eGeneral
            );

            // Update descriptor set with the buffer and image info
            device.updateDescriptorSets(
                {vk::WriteDescriptorSet(
                     descriptorSet,
                     (uint32_t)RYGBToViewSpaceBindingLocation::ubo,
                     0,
                     1,
                     vk::DescriptorType::eUniformBuffer,
                     nullptr,
                     &bufferInfo,
                     nullptr
                 ),
                 vk::WriteDescriptorSet(
                     descriptorSet,
                     (uint32_t)RYGBToViewSpaceBindingLocation::textureSampler,
                     0,
                     1,
                     vk::DescriptorType::eCombinedImageSampler,
                     &imageInfo,
                     nullptr,
                     nullptr
                 )},
                nullptr
            );
        }


        for (size_t i = 0; i < _rygbTransformDescriptorSetsColorSquare.size(); ++i) {
            vk::DescriptorSet descriptorSet = _rygbTransformDescriptorSetsColorSquare[i];

            vk::DescriptorBufferInfo bufferInfo(
                _rygbToViewSpaceCtx->ubo[i].buffer, 0, sizeof(RYGBToViewSpaceUBO)
            );

            vk::DescriptorImageInfo imageInfo(
                _rygbToViewSpaceCtx->samplers[i],
                _colorSquareTexture.GetImageView(),
                vk::ImageLayout::eGeneral
            );

            // Update descriptor set with the buffer and image info
            device.updateDescriptorSets(
                {vk::WriteDescriptorSet(
                     descriptorSet,
                     (uint32_t)RYGBToViewSpaceBindingLocation::ubo,
                     0,
                     1,
                     vk::DescriptorType::eUniformBuffer,
                     nullptr,
                     &bufferInfo,
                     nullptr
                 ),
                 vk::WriteDescriptorSet(
                     descriptorSet,
                     (uint32_t)RYGBToViewSpaceBindingLocation::textureSampler,
                     0,
                     1,
                     vk::DescriptorType::eCombinedImageSampler,
                     &imageInfo,
                     nullptr,
                     nullptr
                 )},
                nullptr
            );
        }
    }
}

// TODO: all these pipeline creation should be better abstracted
void AppPainter::ColorPicker::initCubemapGenerateContext(TetriumApp::InitContext& ctx)
{
    vk::Device device = ctx.device.logicalDevice;

    /* create UBO */
    ctx.device.CreateBufferInPlace(
        sizeof(CubemapGenerateUBO),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        _cubemapGenerateContext.ubo
    );

    /* Descriptors */
    /* create descriptor pool */
    {
        // TODO: use clean descriptor pool sizing
        vk::DescriptorPoolSize poolSizes[]
            = {{vk::DescriptorType::eUniformBuffer, NUM_FRAME_IN_FLIGHT * 2},
               {vk::DescriptorType::eCombinedImageSampler, NUM_FRAME_IN_FLIGHT * 2}};

        vk::DescriptorPoolCreateInfo poolCreateInfo({}, NUM_FRAME_IN_FLIGHT * 4, 2, poolSizes);

        _cubemapGenerateContext.descriptorPool = device.createDescriptorPool(poolCreateInfo);
    }

    /* create descriptor set layout */
    {
        std::array<vk::DescriptorSetLayoutBinding, 2> descriptorSetLayoutBindings
            = {// UBO
               vk::DescriptorSetLayoutBinding(
                   (uint32_t)CubemapGenerationBindingLocation::ubo,
                   vk::DescriptorType::eUniformBuffer,
                   1,
                   vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
                   nullptr

               ),

               vk::DescriptorSetLayoutBinding(
                   (uint32_t)CubemapGenerationBindingLocation::vshMaxSaturationLUT,
                   vk::DescriptorType::eCombinedImageSampler,
                   1,
                   vk::ShaderStageFlagBits::eFragment,
                   nullptr
               )
               };
        vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo(
            {}, descriptorSetLayoutBindings.size(), descriptorSetLayoutBindings.data()
        );

        vk::Result res = device.createDescriptorSetLayout(
            &descriptorSetLayoutCreateInfo, nullptr, &_cubemapGenerateContext.descriptorSetLayout
        );
        ASSERT(res == vk::Result::eSuccess);
    }

    /* create descriptor set*/
    {
        /* allocate descriptor sets */
        {
            std::vector<vk::DescriptorSetLayout> layouts(
                1, _cubemapGenerateContext.descriptorSetLayout
            );
            vk::DescriptorSetAllocateInfo descriptorSetAllocateInfo(
                _cubemapGenerateContext.descriptorPool, 1, layouts.data()
            );
            std::vector<vk::DescriptorSet> res
                = device.allocateDescriptorSets(descriptorSetAllocateInfo);
            ASSERT(res.size() == 1)
            _cubemapGenerateContext.descriptorSet = res[0];
        }

        /* update descriptor sets */
        {
            vk::DescriptorSet descriptorSet = _cubemapGenerateContext.descriptorSet;

            vk::DescriptorBufferInfo bufferInfo(
                _cubemapGenerateContext.ubo.buffer, 0, sizeof(CubemapGenerateUBO)
            );
            std::string lutImagePath = ASSETS_PATH + "apps/AppPainter/textures/LUT.png";
            _cubemapGenerateContext.vshMaxSaturationLUTTextureHandle
                = ctx.api.LoadTexture(lutImagePath);

            ASSERT(_cubemapGenerateContext.vshMaxSaturationLUTTextureHandle != 0);
            vk::DescriptorImageInfo imageInfo = 
            ctx.api.GetTextureDescriptorImageInfo
                (_cubemapGenerateContext.vshMaxSaturationLUTTextureHandle);


            device.updateDescriptorSets(
                {vk::WriteDescriptorSet(
                     descriptorSet,
                     (uint32_t)CubemapGenerationBindingLocation::ubo,
                     0,
                     1,
                     vk::DescriptorType::eUniformBuffer,
                     nullptr,
                     &bufferInfo,
                     nullptr
                 ),
                 vk::WriteDescriptorSet(
                     descriptorSet,
                     (uint32_t)CubemapGenerationBindingLocation::vshMaxSaturationLUT,
                     0,
                     1,
                     vk::DescriptorType::eCombinedImageSampler,
                     &imageInfo,
                     nullptr,
                     nullptr
                 )
                 },
                nullptr
            );
        }
    }


    /* create renderpass */
    {
        vk::AttachmentReference colorAttachmentRef(0, vk::ImageLayout::eColorAttachmentOptimal);
        vk::AttachmentReference depthAttachmentRef(
            1, vk::ImageLayout::eDepthStencilAttachmentOptimal
        );
        vk::SubpassDescription subpass(
            {},
            vk::PipelineBindPoint::eGraphics,
            0,
            nullptr,
            1,
            &colorAttachmentRef,
            nullptr,
            &depthAttachmentRef
        );
        vk::AttachmentDescription attachments[2]
            = {// color attachment
               vk::AttachmentDescription(
                   {},
                   vk::Format::eR32G32B32A32Sfloat,
                   vk::SampleCountFlagBits::e1,
                   vk::AttachmentLoadOp::eClear,
                   vk::AttachmentStoreOp::eStore,
                   vk::AttachmentLoadOp::eDontCare,
                   vk::AttachmentStoreOp::eDontCare,
                   vk::ImageLayout::eUndefined,
                   vk::ImageLayout::eGeneral // to be 1. sampled by RYGB transform 2. copied to CPU-accessible buffer
               ),
               // depth attachment
               vk::AttachmentDescription(
                   {},
                   vk::Format(ctx.device.depthFormat),
                   vk::SampleCountFlagBits::e1,
                   vk::AttachmentLoadOp::eClear,
                   vk::AttachmentStoreOp::eDontCare,
                   vk::AttachmentLoadOp::eDontCare,
                   vk::AttachmentStoreOp::eDontCare,
                   vk::ImageLayout::eUndefined,
                   vk::ImageLayout::eDepthStencilAttachmentOptimal
               )};
        vk::RenderPassCreateInfo createInfo({}, 2, attachments, 1, &subpass, 0);
        _cubemapGenerateContext.renderPass = device.createRenderPass(createInfo);
        ASSERT(_cubemapGenerateContext.renderPass != VK_NULL_HANDLE);
    }

    /* create pipeline */
    {
        std::string VERTEX_SHADER_PATH
            = std::string(ASSETS_PATH + "apps/AppPainter/shaders/cubemap_rygb_gen.vert.spv");
        std::string FRAGMENT_SHADER_PATH
            = std::string(ASSETS_PATH + "apps/AppPainter/shaders/cubemap_rygb_gen.frag.spv");

        // shader modules
        vk::ShaderModule vertShaderModule
            = ShaderCreation::createShaderModule(ctx.device.logicalDevice, VERTEX_SHADER_PATH.c_str());
        vk::ShaderModule fragShaderModule
            = ShaderCreation::createShaderModule(ctx.device.logicalDevice, FRAGMENT_SHADER_PATH.c_str());

        std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages
            = {vk::PipelineShaderStageCreateInfo(
                   {}, vk::ShaderStageFlagBits::eVertex, vertShaderModule, "main"
               ),
               vk::PipelineShaderStageCreateInfo(
                   {}, vk::ShaderStageFlagBits::eFragment, fragShaderModule, "main"
               )};

        // no vertex input -- use full screen quad only
        vk::PipelineVertexInputStateCreateInfo vertexInputInfo({}, 0, nullptr, 0, nullptr);

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

        vk::Viewport viewport(
            0.f, 0.f, CUBEMAP_WIDTH, CUBEMAP_HEIGHT, 0.f, 1.f
        );
        vk::Rect2D scissor(
            {0, 0},
            {
                CUBEMAP_WIDTH,
                CUBEMAP_HEIGHT,
            }
        );
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
            vk::PipelineLayoutCreateFlags(),
            1,
            &_cubemapGenerateContext.descriptorSetLayout,
            0,
            nullptr
        );

        if (device.createPipelineLayout(
                &pipelineLayoutInfo, nullptr, &_cubemapGenerateContext.pipelineLayout
            )
            != vk::Result::eSuccess) {
            FATAL("Failed to create pipeline layout!");
        }

        vk::GraphicsPipelineCreateInfo pipelineInfo(
            vk::PipelineCreateFlags(),               // flags
            shaderStages.size(),                     // stageCount
            shaderStages.data(),                     // pStages
            &vertexInputInfo,                        // pVertexInputState
            &inputAssembly,                          // pInputAssemblyState
            nullptr,                                 // pTessellationState
            &viewportState,                          // pViewportState
            &rasterizer,                             // pRasterizationState
            &multisampling,                          // pMultisampleState
            &depthStencil,                           // pDepthStencilState
            &colorBlending,                          // pColorBlendState
            &dynamicState,                           // pDynamicState
            _cubemapGenerateContext.pipelineLayout, // layout
            _cubemapGenerateContext.renderPass,     // renderPass
            0,                                       // subpass
            vk::Pipeline(),                          // basePipelineHandle
            -1                                       // basePipelineIndex
        );

        if (device.createGraphicsPipelines(
                nullptr, 1, &pipelineInfo, nullptr, &_cubemapGenerateContext.pipeline
            )
            != vk::Result::eSuccess) {
            FATAL("Failed to create graphics pipeline!");
        }

        device.destroyShaderModule(fragShaderModule, nullptr);
        device.destroyShaderModule(vertShaderModule, nullptr);
    }
}

void AppPainter::ColorPicker::cleanupCubemapGenerateContext(TetriumApp::CleanupContext& ctx)
{
    vk::Device device = ctx.device.logicalDevice;

    _cubemapGenerateContext.ubo.Cleanup();

    ctx.api.UnloadTexture(_cubemapGenerateContext.vshMaxSaturationLUTTextureHandle);
    device.destroyDescriptorSetLayout(_cubemapGenerateContext.descriptorSetLayout);
    device.destroyDescriptorPool(_cubemapGenerateContext.descriptorPool);
    device.destroyRenderPass(_cubemapGenerateContext.renderPass);
    device.destroyPipeline(_cubemapGenerateContext.pipeline);
    device.destroyPipelineLayout(_cubemapGenerateContext.pipelineLayout);
}

void AppPainter::ColorPicker::initColorSquareGenerateContext(TetriumApp::InitContext& ctx)
{
    vk::Device device = ctx.device.logicalDevice;

    /* create UBO */
    ctx.device.CreateBufferInPlace(
        sizeof(ColorSquareGenerationUBO),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        _colorSquareContext.ubo
    );

    /* Descriptors */
    /* create descriptor pool */
    {
        // TODO: use clean descriptor pool sizing
        vk::DescriptorPoolSize poolSizes[]
            = {{vk::DescriptorType::eUniformBuffer, NUM_FRAME_IN_FLIGHT * 2},
               {vk::DescriptorType::eCombinedImageSampler, NUM_FRAME_IN_FLIGHT * 2}};

        vk::DescriptorPoolCreateInfo poolCreateInfo({}, NUM_FRAME_IN_FLIGHT * 4, 2, poolSizes);

        _colorSquareContext.descriptorPool = device.createDescriptorPool(poolCreateInfo);
    }

    /* create descriptor set layout */
    {
        std::array<vk::DescriptorSetLayoutBinding, 2> descriptorSetLayoutBindings
            = {// UBO
               vk::DescriptorSetLayoutBinding(
                   (uint32_t)ColorSquareBindingLocation::ubo,
                   vk::DescriptorType::eUniformBuffer,
                   1,
                   vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
                   nullptr

               ),

               vk::DescriptorSetLayoutBinding(
                   (uint32_t)ColorSquareBindingLocation::vshMaxSaturationLUT,
                   vk::DescriptorType::eCombinedImageSampler,
                   1,
                   vk::ShaderStageFlagBits::eFragment,
                   nullptr
               )
               };
        vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo(
            {}, descriptorSetLayoutBindings.size(), descriptorSetLayoutBindings.data()
        );

        vk::Result res = device.createDescriptorSetLayout(
            &descriptorSetLayoutCreateInfo, nullptr, &_colorSquareContext.descriptorSetLayout
        );
        ASSERT(res == vk::Result::eSuccess);
    }

    /* create descriptor set*/
    {
        /* allocate descriptor sets */
        {
            std::vector<vk::DescriptorSetLayout> layouts(
                1, _colorSquareContext.descriptorSetLayout
            );
            vk::DescriptorSetAllocateInfo descriptorSetAllocateInfo(
                _colorSquareContext.descriptorPool, 1, layouts.data()
            );
            std::vector<vk::DescriptorSet> res
                = device.allocateDescriptorSets(descriptorSetAllocateInfo);
            ASSERT(res.size() == 1)
            _colorSquareContext.descriptorSet = res[0];
        }

        /* update descriptor sets */
        {
            vk::DescriptorSet descriptorSet = _colorSquareContext.descriptorSet;

            vk::DescriptorBufferInfo bufferInfo(
                _colorSquareContext.ubo.buffer, 0, sizeof(CubemapGenerateUBO)
            );
            std::string lutImagePath = ASSETS_PATH + "apps/AppPainter/textures/LUT.png";
            _colorSquareContext.vshMaxSaturationLUTTextureHandle
                = ctx.api.LoadTexture(lutImagePath);

            ASSERT(_colorSquareContext.vshMaxSaturationLUTTextureHandle != 0);
            vk::DescriptorImageInfo imageInfo = 
            ctx.api.GetTextureDescriptorImageInfo
                (_colorSquareContext.vshMaxSaturationLUTTextureHandle);


            device.updateDescriptorSets(
                {vk::WriteDescriptorSet(
                     descriptorSet,
                     (uint32_t)CubemapGenerationBindingLocation::ubo,
                     0,
                     1,
                     vk::DescriptorType::eUniformBuffer,
                     nullptr,
                     &bufferInfo,
                     nullptr
                 ),
                 vk::WriteDescriptorSet(
                     descriptorSet,
                     (uint32_t)CubemapGenerationBindingLocation::vshMaxSaturationLUT,
                     0,
                     1,
                     vk::DescriptorType::eCombinedImageSampler,
                     &imageInfo,
                     nullptr,
                     nullptr
                 )
                 },
                nullptr
            );
        }
    }


    /* create renderpass */
    {
        vk::AttachmentReference colorAttachmentRef(0, vk::ImageLayout::eColorAttachmentOptimal);
        vk::AttachmentReference depthAttachmentRef(
            1, vk::ImageLayout::eDepthStencilAttachmentOptimal
        );
        vk::SubpassDescription subpass(
            {},
            vk::PipelineBindPoint::eGraphics,
            0,
            nullptr,
            1,
            &colorAttachmentRef,
            nullptr,
            &depthAttachmentRef
        );
        vk::AttachmentDescription attachments[2]
            = {// color attachment
               vk::AttachmentDescription(
                   {},
                   vk::Format::eR32G32B32A32Sfloat,
                   vk::SampleCountFlagBits::e1,
                   vk::AttachmentLoadOp::eClear,
                   vk::AttachmentStoreOp::eStore,
                   vk::AttachmentLoadOp::eDontCare,
                   vk::AttachmentStoreOp::eDontCare,
                   vk::ImageLayout::eUndefined,
                   vk::ImageLayout::eGeneral // to be 1. sampled by RYGB transform 2. copied to CPU-accessible buffer
               ),
               // depth attachment
               vk::AttachmentDescription(
                   {},
                   vk::Format(ctx.device.depthFormat),
                   vk::SampleCountFlagBits::e1,
                   vk::AttachmentLoadOp::eClear,
                   vk::AttachmentStoreOp::eDontCare,
                   vk::AttachmentLoadOp::eDontCare,
                   vk::AttachmentStoreOp::eDontCare,
                   vk::ImageLayout::eUndefined,
                   vk::ImageLayout::eDepthStencilAttachmentOptimal
               )};
        vk::RenderPassCreateInfo createInfo({}, 2, attachments, 1, &subpass, 0);
        _colorSquareContext.renderPass = device.createRenderPass(createInfo);
        ASSERT(_colorSquareContext.renderPass != VK_NULL_HANDLE);
    }

    /* create pipeline */
    {
        std::string VERTEX_SHADER_PATH
            = std::string(ASSETS_PATH + "apps/AppPainter/shaders/colorsquare_rygb_gen.vert.spv");
        std::string FRAGMENT_SHADER_PATH
            = std::string(ASSETS_PATH + "apps/AppPainter/shaders/colorsquare_rygb_gen.frag.spv");

        // shader modules
        vk::ShaderModule vertShaderModule
            = ShaderCreation::createShaderModule(ctx.device.logicalDevice, VERTEX_SHADER_PATH.c_str());
        vk::ShaderModule fragShaderModule
            = ShaderCreation::createShaderModule(ctx.device.logicalDevice, FRAGMENT_SHADER_PATH.c_str());

        std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages
            = {vk::PipelineShaderStageCreateInfo(
                   {}, vk::ShaderStageFlagBits::eVertex, vertShaderModule, "main"
               ),
               vk::PipelineShaderStageCreateInfo(
                   {}, vk::ShaderStageFlagBits::eFragment, fragShaderModule, "main"
               )};

        // no vertex input -- use full screen quad only
        vk::PipelineVertexInputStateCreateInfo vertexInputInfo({}, 0, nullptr, 0, nullptr);

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

        vk::Viewport viewport(
            0.f, 0.f, COLORSQUARE_SIZE, COLORSQUARE_SIZE, 0.f, 1.f
        );
        vk::Rect2D scissor(
            {0, 0},
            {
                COLORSQUARE_SIZE,
                COLORSQUARE_SIZE,
            }
        );
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
            vk::PipelineLayoutCreateFlags(),
            1,
            &_colorSquareContext.descriptorSetLayout,
            0,
            nullptr
        );

        if (device.createPipelineLayout(
                &pipelineLayoutInfo, nullptr, &_colorSquareContext.pipelineLayout
            )
            != vk::Result::eSuccess) {
            FATAL("Failed to create pipeline layout!");
        }

        vk::GraphicsPipelineCreateInfo pipelineInfo(
            vk::PipelineCreateFlags(),               // flags
            shaderStages.size(),                     // stageCount
            shaderStages.data(),                     // pStages
            &vertexInputInfo,                        // pVertexInputState
            &inputAssembly,                          // pInputAssemblyState
            nullptr,                                 // pTessellationState
            &viewportState,                          // pViewportState
            &rasterizer,                             // pRasterizationState
            &multisampling,                          // pMultisampleState
            &depthStencil,                           // pDepthStencilState
            &colorBlending,                          // pColorBlendState
            &dynamicState,                           // pDynamicState
            _colorSquareContext.pipelineLayout, // layout
            _colorSquareContext.renderPass,     // renderPass
            0,                                       // subpass
            vk::Pipeline(),                          // basePipelineHandle
            -1                                       // basePipelineIndex
        );

        if (device.createGraphicsPipelines(
                nullptr, 1, &pipelineInfo, nullptr, &_colorSquareContext.pipeline
            )
            != vk::Result::eSuccess) {
            FATAL("Failed to create graphics pipeline!");
        }

        device.destroyShaderModule(fragShaderModule, nullptr);
        device.destroyShaderModule(vertShaderModule, nullptr);
    }
}

void AppPainter::ColorPicker::cleanupColorSquareGenerateContext(TetriumApp::CleanupContext& ctx)
{
    vk::Device device = ctx.device.logicalDevice;

    _colorSquareContext.ubo.Cleanup();

    ctx.api.UnloadTexture(_colorSquareContext.vshMaxSaturationLUTTextureHandle);
    device.destroyDescriptorSetLayout(_colorSquareContext.descriptorSetLayout);
    device.destroyDescriptorPool(_colorSquareContext.descriptorPool);
    device.destroyRenderPass(_colorSquareContext.renderPass);
    device.destroyPipeline(_colorSquareContext.pipeline);
    device.destroyPipelineLayout(_colorSquareContext.pipelineLayout);
}


void AppPainter::ColorPicker::Init(TetriumApp::InitContext& ctx, RYGBToViewSpaceContext* rygbToViewspaceCtx)
{
    initCubemapGenerateContext(ctx);
    initColorSquareGenerateContext(ctx);
    ASSERT(_cubemapGenerateContext.renderPass)
    _cubemapTexture.Init(
        ctx.device.logicalDevice,
        ctx.device.physicalDevice,
        _cubemapGenerateContext.renderPass,
        CUBEMAP_WIDTH,
        CUBEMAP_HEIGHT,
        VK_FORMAT_R32G32B32A32_SFLOAT, // RYGB color space
        ctx.device.depthFormat,
        false,
        true // allowImageTransfer -- enable the VK_IMAGE_USAGE_TRANSFER_SRC_BIT flag
    );

    ASSERT(_colorSquareContext.renderPass)
    _colorSquareTexture.Init(
        ctx.device.logicalDevice,
        ctx.device.physicalDevice,
        _colorSquareContext.renderPass,
        COLORSQUARE_SIZE,
        COLORSQUARE_SIZE,
        VK_FORMAT_R32G32B32A32_SFLOAT, // RYGB color space
        ctx.device.depthFormat,
        true,
        true // allowImageTransfer -- enable the VK_IMAGE_USAGE_TRANSFER_SRC_BIT flag
    );

    _rygbToViewSpaceCtx = rygbToViewspaceCtx;
    
    ASSERT(_rygbToViewSpaceCtx->renderPass)
    _cubemapTextureViewSpace.Init(
        ctx.device.logicalDevice,
        ctx.device.physicalDevice,
        _rygbToViewSpaceCtx->renderPass, // TODO: put RYGBTOviewspaceContext to a separate file
        CUBEMAP_WIDTH,
        CUBEMAP_HEIGHT,
        VK_FORMAT_R8G8B8A8_SRGB, // RGB / OCV color space
        ctx.device.depthFormat,
        true
    );

    _colorSquareTextureViewSpace.Init(
        ctx.device.logicalDevice,
        ctx.device.physicalDevice,
        _rygbToViewSpaceCtx->renderPass, // TODO: put RYGBTOviewspaceContext to a separate file
        COLORSQUARE_SIZE,
        COLORSQUARE_SIZE,
        VK_FORMAT_R8G8B8A8_SRGB, // RGB / OCV color space
        ctx.device.depthFormat,
        true
    );
    initRYGBTransform(ctx); /// basically allocates a bunch of descriptor sets for use, god i hate this please lets' get desciprtor heap working

    // init CPU-accessible color picker texture
    ctx.device.CreateBufferInPlace(
        sizeof(float) * 4 * CUBEMAP_HEIGHT * CUBEMAP_WIDTH,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        _cubemapRYGBTextureCPU
    );

    ctx.device.CreateBufferInPlace(
        sizeof(float) * 4 * COLORSQUARE_SIZE *COLORSQUARE_SIZE,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        _colorSquareRYGBTextureCPU
    );

    _clearValues
        = {vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}),
           vk::ClearDepthStencilValue(1.0f, 0)};

    _hueSphere.Init(ctx, 1024, 1024, CUBEMAP_CUBE_SIZE, _cubemapTextureViewSpace);
}

void AppPainter::ColorPicker::Cleanup(TetriumApp::CleanupContext& ctx)
{
    _hueSphere.Cleanup(ctx);
    _cubemapRYGBTextureCPU.Cleanup();
    _colorSquareRYGBTextureCPU.Cleanup();
    cleanupCubemapGenerateContext(ctx);
    _cubemapTextureViewSpace.Cleanup();
    _cubemapTexture.Cleanup();
    _colorSquareTextureViewSpace.Cleanup();
    _colorSquareTexture.Cleanup();
}

glm::vec4 AppPainter::ColorPicker::GetSelectedColorRYGB() const { return _selectedColorRYGB; }

std::array<float, 4> AppPainter::ColorPicker::GetSelectedColorRYGBData() const
{
    return {_selectedColorRYGB.r, _selectedColorRYGB.g, _selectedColorRYGB.b, _selectedColorRYGB.a};
}

} // namespace TetriumApp
