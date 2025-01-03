#include "components/ShaderUtils.h"
#include "imgui.h"

#include "lib/VulkanUtils.h"

#include "AppPainter.h"

namespace TetriumApp
{

/* ---------- Init & Cleanup ---------- */

void AppPainter::initPaintSpaceBuffer(TetriumApp::InitContext& ctx)
{
    VkDeviceSize bufferSize
        = _canvasWidth * _canvasHeight * PAINT_SPACE_PIXEL_SIZE;
    DEBUG("Paint space buffer size: {}", bufferSize);
    _paintSpaceBuffer = ctx.device.CreateBuffer(
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    // fill buffer with zeros
    memset(_paintSpaceBuffer.bufferAddress, 0, bufferSize);
}

void AppPainter::cleanupPaintSpaceBuffer(TetriumApp::CleanupContext& ctx)
{
    _paintSpaceBuffer.Cleanup();
}

void AppPainter::initPaintSpaceTexture(TetriumApp::InitContext& ctx)
{
    // RYGB color space, need 4 bytes per channel to store potentially
    // negative float values.
    const VkFormat IMAGE_FORMAT = VK_FORMAT_R32G32B32A32_SFLOAT;

    VkImageViewCreateInfo imageViewCreateInfo{};
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.format = IMAGE_FORMAT;
    imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    imageViewCreateInfo.subresourceRange.levelCount = 1;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount = 1;

    for (PaintSpaceTexture& fb : _paintSpaceTexture) {
        VkImage image{};
        VkDeviceMemory memory{};
        VkImageView imageView{};

        VulkanUtils::createImage(
            _canvasWidth,
            _canvasHeight,
            IMAGE_FORMAT,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            image,
            memory,
            ctx.device.physicalDevice,
            ctx.device.logicalDevice
        );

        // transition image layout from undefined to general
        {
            vk::CommandBuffer cb = ctx.device.BeginSingleTimeCommands();
            vk::ImageMemoryBarrier barrier(
                vk::AccessFlags(),
                vk::AccessFlags(),
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eGeneral,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                image,
                vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
            );

            cb.pipelineBarrier(
                vk::PipelineStageFlagBits::eTopOfPipe,
                vk::PipelineStageFlagBits::eTransfer,
                vk::DependencyFlags(),
                nullptr,
                nullptr,
                barrier
            );
            ctx.device.EndSingleTimeCommands(cb);
        }

        imageViewCreateInfo.image = image;

        VK_CHECK_RESULT(
            vkCreateImageView(ctx.device.logicalDevice, &imageViewCreateInfo, nullptr, &imageView)
        );

        ASSERT(image != VK_NULL_HANDLE);
        ASSERT(memory != VK_NULL_HANDLE);
        ASSERT(imageView != VK_NULL_HANDLE);
        fb.image = image;
        fb.imageView = imageView;
        fb.memory = memory;
        fb.needsUpdate = true;
    }
}

void AppPainter::cleanupPaintSpaceTexture(TetriumApp::CleanupContext& ctx)
{
    vk::Device device = ctx.device.logicalDevice;
    for (PaintSpaceTexture& fb : _paintSpaceTexture) {
        device.destroyImage(fb.image);
        device.destroyImageView(fb.imageView);
        device.freeMemory(fb.memory);
    }
}

void AppPainter::initViewSpaceFrameBuffer(TetriumApp::InitContext& ctx)
{
    ASSERT(
        _paintToViewSpaceContext.renderPass != VK_NULL_HANDLE && "render pass must be initialized"
    );
    for (TextureFrameBuffer& fb : _viewSpaceFrameBuffer) {
        fb.Init(
            ctx.device.logicalDevice,
            ctx.device.physicalDevice,
            _paintToViewSpaceContext.renderPass,
            _canvasWidth,
            _canvasHeight,
            VK_FORMAT_R8G8B8A8_SRGB, // RGB / OCV color space
            ctx.device.depthFormat,
            true
        );
    }
}

void AppPainter::cleanupViewSpaceFrameBuffer(TetriumApp::CleanupContext& ctx)
{
    for (TextureFrameBuffer& fb : _viewSpaceFrameBuffer) {
        fb.Cleanup();
    }
}

void AppPainter::initPaintToViewSpaceContext(TetriumApp::InitContext& ctx)
{
    vk::Device device = ctx.device.logicalDevice;

    /* create UBO */
    for (VQBuffer& ubo : _paintToViewSpaceContext.ubo) {
        ctx.device.CreateBufferInPlace(
            sizeof(UBO),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            ubo
        );
    }

    /* create samplers */
    {
        VkSamplerCreateInfo samplerCreateInfo = {};
        samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        for (vk::Sampler& sampler : _paintToViewSpaceContext.samplers) {
            sampler = device.createSampler(samplerCreateInfo);
        }
    }

    /* Descriptors */
    /* create descriptor pool */
    {
        vk::DescriptorPoolSize poolSizes[]
            = {{vk::DescriptorType::eUniformBuffer, NUM_FRAME_IN_FLIGHT},
               {vk::DescriptorType::eCombinedImageSampler, NUM_FRAME_IN_FLIGHT}};

        vk::DescriptorPoolCreateInfo poolCreateInfo({}, NUM_FRAME_IN_FLIGHT * 2, 2, poolSizes);

        _paintToViewSpaceContext.descriptorPool = device.createDescriptorPool(poolCreateInfo);
    }

    /* create descriptor set layout */
    {
        std::array<vk::DescriptorSetLayoutBinding, 2> descriptorSetLayoutBindings
            = {// UBO
               vk::DescriptorSetLayoutBinding(
                   (uint32_t)BindingLocation::ubo,
                   vk::DescriptorType::eUniformBuffer,
                   1,
                   vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
                   nullptr

               ),
               // paint space texture sampler
               vk::DescriptorSetLayoutBinding(
                   (uint32_t)BindingLocation::sampler,
                   vk::DescriptorType::eCombinedImageSampler,
                   1,
                   vk::ShaderStageFlagBits::eFragment,
                   nullptr
               )};
        vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo(
            {}, descriptorSetLayoutBindings.size(), descriptorSetLayoutBindings.data()
        );

        vk::Result res = device.createDescriptorSetLayout(
            &descriptorSetLayoutCreateInfo, nullptr, &_paintToViewSpaceContext.descriptorSetLayout
        );
        ASSERT(res == vk::Result::eSuccess);
    }

    /* allocate descriptor sets */
    {
        std::vector<vk::DescriptorSetLayout> layouts(
            NUM_FRAME_IN_FLIGHT, _paintToViewSpaceContext.descriptorSetLayout
        );
        vk::DescriptorSetAllocateInfo descriptorSetAllocateInfo(
            _paintToViewSpaceContext.descriptorPool, NUM_FRAME_IN_FLIGHT, layouts.data()
        );
        std::vector<vk::DescriptorSet> res
            = device.allocateDescriptorSets(descriptorSetAllocateInfo);
        ASSERT(res.size() == _paintToViewSpaceContext.descriptorSets.size());
        for (size_t i = 0; i < res.size(); i++) {
            _paintToViewSpaceContext.descriptorSets[i] = res[i];
        }
    }

    /* update descriptor sets */
    {
        for (int i = 0; i < _paintToViewSpaceContext.descriptorSets.size(); i++) {
            vk::DescriptorSet descriptorSet = _paintToViewSpaceContext.descriptorSets[i];

            vk::DescriptorBufferInfo bufferInfo(
                _paintToViewSpaceContext.ubo[i].buffer, 0, sizeof(UBO)
            );

            vk::DescriptorImageInfo imageInfo(
                _paintToViewSpaceContext.samplers[i],
                _paintSpaceTexture[0].imageView,
                vk::ImageLayout::eGeneral
            );

            device.updateDescriptorSets(
                {vk::WriteDescriptorSet(
                     descriptorSet,
                     (uint32_t)BindingLocation::ubo,
                     0,
                     1,
                     vk::DescriptorType::eUniformBuffer,
                     nullptr,
                     &bufferInfo,
                     nullptr
                 ),
                 vk::WriteDescriptorSet(
                     descriptorSet,
                     (uint32_t)BindingLocation::sampler,
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
                   vk::Format::eR8G8B8A8Srgb,
                   vk::SampleCountFlagBits::e1,
                   vk::AttachmentLoadOp::eClear,
                   vk::AttachmentStoreOp::eStore,
                   vk::AttachmentLoadOp::eDontCare,
                   vk::AttachmentStoreOp::eDontCare,
                   vk::ImageLayout::eUndefined,
                   vk::ImageLayout::eShaderReadOnlyOptimal // write to imgui texture
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
        _paintToViewSpaceContext.renderPass = device.createRenderPass(createInfo);
        ASSERT(_paintToViewSpaceContext.renderPass != VK_NULL_HANDLE);
    }

    /* create pipeline */
    {
        const char* VERTEX_SHADER_PATH
            = "../assets/apps/AppPainter/shaders/paint_to_view_space.vert.spv";
        const char* FRAGMENT_SHADER_PATH
            = "../assets/apps/AppPainter/shaders/paint_to_view_space.frag.spv";

        // shader modules
        vk::ShaderModule vertShaderModule
            = ShaderCreation::createShaderModule(ctx.device.logicalDevice, VERTEX_SHADER_PATH);
        vk::ShaderModule fragShaderModule
            = ShaderCreation::createShaderModule(ctx.device.logicalDevice, FRAGMENT_SHADER_PATH);

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
            0.f, 0.f, _canvasWidth, _canvasHeight, 0.f, 1.f
        );
        vk::Rect2D scissor(
            {0, 0},
            {
                _canvasWidth,
                _canvasHeight,
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
            &_paintToViewSpaceContext.descriptorSetLayout,
            0,
            nullptr
        );

        if (device.createPipelineLayout(
                &pipelineLayoutInfo, nullptr, &_paintToViewSpaceContext.pipelineLayout
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
            _paintToViewSpaceContext.pipelineLayout, // layout
            _paintToViewSpaceContext.renderPass,     // renderPass
            0,                                       // subpass
            vk::Pipeline(),                          // basePipelineHandle
            -1                                       // basePipelineIndex
        );

        if (device.createGraphicsPipelines(
                nullptr, 1, &pipelineInfo, nullptr, &_paintToViewSpaceContext.pipeline
            )
            != vk::Result::eSuccess) {
            FATAL("Failed to create graphics pipeline!");
        }

        device.destroyShaderModule(fragShaderModule, nullptr);
        device.destroyShaderModule(vertShaderModule, nullptr);
    }
}

void AppPainter::cleanupPaintToViewSpaceContext(TetriumApp::CleanupContext& ctx)
{
    vk::Device device = ctx.device.logicalDevice;

    for (VQBuffer& ubo : _paintToViewSpaceContext.ubo) {
        ubo.Cleanup();
    }

    for (vk::Sampler& sampler : _paintToViewSpaceContext.samplers) {
        device.destroySampler(sampler);
    }

    device.destroyDescriptorSetLayout(_paintToViewSpaceContext.descriptorSetLayout);
    device.destroyDescriptorPool(_paintToViewSpaceContext.descriptorPool);
    device.destroyRenderPass(_paintToViewSpaceContext.renderPass);
    device.destroyPipeline(_paintToViewSpaceContext.pipeline);
    device.destroyPipelineLayout(_paintToViewSpaceContext.pipelineLayout);
}

void AppPainter::Init(TetriumApp::InitContext& ctx)
{
    initPaintSpaceBuffer(ctx);
    initPaintSpaceTexture(ctx);
    initPaintToViewSpaceContext(ctx);
    initViewSpaceFrameBuffer(ctx);

    _clearValues
        = {vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}),
           vk::ClearDepthStencilValue(1.0f, 0)};

    _colorPicker.Init();
}

void AppPainter::Cleanup(TetriumApp::CleanupContext& ctx)
{
    _colorPicker.Cleanup();

    cleanupViewSpaceFrameBuffer(ctx);
    cleanupPaintToViewSpaceContext(ctx);
    cleanupPaintSpaceTexture(ctx);
    cleanupPaintSpaceBuffer(ctx);
}

/* ---------- Tick ---------- */

void AppPainter::TickVulkan(TetriumApp::TickContextVulkan& ctx)
{
    PaintSpaceTexture& canvas = _paintSpaceTexture[ctx.currentFrameInFlight];
    vk::CommandBuffer& cb = ctx.commandBuffer;

    // update paint space texture if needed
    if (canvas.needsUpdate) {
        vk::BufferImageCopy copyRegion(
            0,
            0,
            0,
            vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
            vk::Offset3D(0, 0, 0),
            vk::Extent3D(_canvasWidth, _canvasHeight, 1)
        );
        cb.copyBufferToImage(
            _paintSpaceBuffer.buffer, // src
            canvas.image,             // dst
            vk::ImageLayout::eGeneral,
            1,
            &copyRegion
        );

        // pipeline blocks until paint space texture is updated.
        cb.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eFragmentShader,
            vk::DependencyFlags(),
            0,
            nullptr,
            0,
            nullptr,
            0,
            nullptr
        );
        canvas.needsUpdate = false;
    }

    // flush UBO
    UBO* pUBO = reinterpret_cast<UBO*>(
        _paintToViewSpaceContext.ubo[ctx.currentFrameInFlight].bufferAddress
    );

    pUBO->transformMatrix = _tranformMatrixFromRygb[ctx.colorSpace];

    // Transform paint space to view space
    vk::Extent2D extend(_canvasWidth, _canvasHeight);
    vk::Rect2D renderArea(VkOffset2D{0, 0}, extend);
    vk::RenderPassBeginInfo renderPassBeginInfo(
        _paintToViewSpaceContext.renderPass,
        _viewSpaceFrameBuffer[ctx.currentFrameInFlight].GetFrameBuffer(),
        renderArea,
        _clearValues.size(),
        _clearValues.data()
    );

    cb.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
    cb.setViewport(0, vk::Viewport(0.f, 0.f, extend.width, extend.height, 0.f, 1.f));
    cb.setScissor(0, vk::Rect2D({0, 0}, extend));
    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, _paintToViewSpaceContext.pipeline);
    cb.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        _paintToViewSpaceContext.pipelineLayout,
        0,
        1,
        &_paintToViewSpaceContext.descriptorSets[ctx.currentFrameInFlight],
        0,
        nullptr,
        vk::getDispatchLoaderStatic()
    );
    // draw a full-screen quad
    cb.draw(3, 1, 0, 0);
    cb.endRenderPass();
}

} // namespace TetriumApp
