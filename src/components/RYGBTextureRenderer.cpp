#include "RYGBTextureRenderer.h"

#include "components/ShaderUtils.h"
#include "lib/VulkanUtils.h"
#include <Pathing.h>
#include <tiffio.h>

namespace TetriumApp
{

RYGBTextureRenderer::RYGBTextureRenderer() {}

RYGBTextureRenderer::~RYGBTextureRenderer() {}

void RYGBTextureRenderer::Init(InitContext& ctx)
{
    _device = &ctx.device;
    _vkDevice = ctx.device.Get();
    _physicalDevice = ctx.device.physicalDevice;

    // Initialize clear values
    _clearValues
        = {vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}),
           vk::ClearDepthStencilValue(1.0f, 0)};
}

void RYGBTextureRenderer::Cleanup(CleanupContext& ctx)
{
    if (_isLoaded) {
        cleanupOutputFrameBuffers();
        cleanupTransformContext();
        cleanupRYGBTexture();
    }
}

void RYGBTextureRenderer::LoadRYGBTexture(const std::string& tiffPath)
{
    // Clean up previous texture if loaded
    if (_isLoaded) {
        cleanupOutputFrameBuffers();
        cleanupTransformContext();
        cleanupRYGBTexture();
        _isLoaded = false;
    }

    // Open TIFF file
    TIFF* tiff = TIFFOpen(tiffPath.c_str(), "r");
    if (!tiff) {
        ERROR("Failed to open RYGB TIFF file: {}", tiffPath);
        return;
    }

    // Read dimensions
    TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH, &_width);
    TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &_height);

    INFO("Loading RYGB texture: {}x{} from {}", _width, _height, tiffPath);

    // Allocate CPU buffer for RYGB data (4 channels, 32-bit float)
    VkDeviceSize bufferSize = _width * _height * 4 * sizeof(float);
    _device->CreateBufferInPlace(
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        _rygbCPUBuffer
    );

    // Read TIFF data into CPU buffer
    float* pBuffer = reinterpret_cast<float*>(_rygbCPUBuffer.bufferAddress);
    for (uint32_t row = 0; row < _height; ++row) {
        if (TIFFReadScanline(tiff, pBuffer + row * _width * 4, row, 0) < 0) {
            ERROR("Failed to read scanline {} from TIFF file", row);
            TIFFClose(tiff);
            _rygbCPUBuffer.Cleanup();
            return;
        }
    }

    TIFFClose(tiff);

    // Initialize Vulkan resources
    initRYGBTexture();
    initTransformContext();
    initDescriptorSets();
    initOutputFrameBuffers();

    // Upload RYGB data to GPU
    uploadRYGBToGPU();

    _isLoaded = true;
    INFO("RYGB texture loaded successfully");
}

void RYGBTextureRenderer::SetTransformMatrices(const glm::mat4x4& toRGB, const glm::mat4x4& toOCV)
{
    _toRGBMatrix = toRGB;
    _toOCVMatrix = toOCV;
    DEBUG("Transform matrices updated");
}

void RYGBTextureRenderer::TickVulkan(TickContextVulkan& ctx)
{
    if (!_isLoaded) {
        return;
    }

    int frameIndex = ctx.currentFrameInFlight;

    // Update UBOs with transformation matrices
    RYGBTransformUBO* pUBORGB
        = reinterpret_cast<RYGBTransformUBO*>(_transformContext.ubo[frameIndex].bufferAddress);
    pUBORGB->transformMatrix = _toRGBMatrix;

    // Render RGB transformation
    renderTransform(ctx.commandBuffer, frameIndex, ColorSpace::RGB, _rgbFrameBuffer[frameIndex]);

    // Update UBO for OCV (reuse same UBO, just update the matrix)
    pUBORGB->transformMatrix = _toOCVMatrix;

    // Render OCV transformation
    renderTransform(ctx.commandBuffer, frameIndex, ColorSpace::OCV, _ocvFrameBuffer[frameIndex]);
}

uint32_t RYGBTextureRenderer::GetRGBTextureHandle() const
{
    // Return a pseudo-handle (we'll use the framebuffer directly)
    return _isLoaded ? 1 : 0;
}

uint32_t RYGBTextureRenderer::GetOCVTextureHandle() const { return _isLoaded ? 2 : 0; }

ImGuiTexture RYGBTextureRenderer::GetRGBTexture() const
{
    if (!_isLoaded) {
        return ImGuiTexture{nullptr, 0, 0};
    }
    return ImGuiTexture{
        _rgbFrameBuffer[0].GetImGuiTextureId(), // Use frame 0 as reference
        static_cast<int>(_width),
        static_cast<int>(_height)};
}

ImGuiTexture RYGBTextureRenderer::GetOCVTexture() const
{
    if (!_isLoaded) {
        return ImGuiTexture{nullptr, 0, 0};
    }
    return ImGuiTexture{
        _ocvFrameBuffer[0].GetImGuiTextureId(),
        static_cast<int>(_width),
        static_cast<int>(_height)};
}

// ========== Private Implementation ==========

void RYGBTextureRenderer::initRYGBTexture()
{
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

    for (RYGBTexture& tex : _rygbGPUTexture) {
        VkImage image{};
        VkDeviceMemory memory{};
        VkImageView imageView{};

        VulkanUtils::createImage(
            _width,
            _height,
            IMAGE_FORMAT,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            image,
            memory,
            _physicalDevice,
            _vkDevice
        );

        // Transition image layout from undefined to general
        {
            vk::CommandBuffer cb = _device->BeginSingleTimeCommands();
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
            _device->EndSingleTimeCommands(cb);
        }

        imageViewCreateInfo.image = image;
        VK_CHECK_RESULT(vkCreateImageView(_vkDevice, &imageViewCreateInfo, nullptr, &imageView));

        tex.image = image;
        tex.imageView = imageView;
        tex.memory = memory;
        tex.needsUpdate = true;
    }
}

void RYGBTextureRenderer::initTransformContext()
{
    // Create UBOs
    for (VQBuffer& ubo : _transformContext.ubo) {
        _device->CreateBufferInPlace(
            sizeof(RYGBTransformUBO),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            ubo
        );
    }

    // Create samplers
    {
        VkSamplerCreateInfo samplerCreateInfo = {};
        samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        for (vk::Sampler& sampler : _transformContext.samplers) {
            sampler = _vkDevice.createSampler(samplerCreateInfo);
        }
    }

    // Create descriptor pool
    {
        vk::DescriptorPoolSize poolSizes[]
            = {{vk::DescriptorType::eUniformBuffer, NUM_FRAME_IN_FLIGHT * 4},
               {vk::DescriptorType::eCombinedImageSampler, NUM_FRAME_IN_FLIGHT * 4}};

        vk::DescriptorPoolCreateInfo poolCreateInfo({}, NUM_FRAME_IN_FLIGHT * 4, 2, poolSizes);

        _transformContext.descriptorPool = _vkDevice.createDescriptorPool(poolCreateInfo);
    }

    // Create descriptor set layout
    {
        std::array<vk::DescriptorSetLayoutBinding, 2> bindings
            = {// UBO
               vk::DescriptorSetLayoutBinding(
                   (uint32_t)RYGBTransformBindingLocation::ubo,
                   vk::DescriptorType::eUniformBuffer,
                   1,
                   vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
                   nullptr
               ),
               // Texture sampler
               vk::DescriptorSetLayoutBinding(
                   (uint32_t)RYGBTransformBindingLocation::textureSampler,
                   vk::DescriptorType::eCombinedImageSampler,
                   1,
                   vk::ShaderStageFlagBits::eFragment,
                   nullptr
               )};

        vk::DescriptorSetLayoutCreateInfo layoutCreateInfo({}, bindings.size(), bindings.data());

        _transformContext.descriptorSetLayout
            = _vkDevice.createDescriptorSetLayout(layoutCreateInfo);
    }

    // Create render pass
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
            = {// Color attachment
               vk::AttachmentDescription(
                   {},
                   vk::Format::eR8G8B8A8Srgb,
                   vk::SampleCountFlagBits::e1,
                   vk::AttachmentLoadOp::eClear,
                   vk::AttachmentStoreOp::eStore,
                   vk::AttachmentLoadOp::eDontCare,
                   vk::AttachmentStoreOp::eDontCare,
                   vk::ImageLayout::eUndefined,
                   vk::ImageLayout::eShaderReadOnlyOptimal
               ),
               // Depth attachment
               vk::AttachmentDescription(
                   {},
                   vk::Format(_device->depthFormat),
                   vk::SampleCountFlagBits::e1,
                   vk::AttachmentLoadOp::eClear,
                   vk::AttachmentStoreOp::eDontCare,
                   vk::AttachmentLoadOp::eDontCare,
                   vk::AttachmentStoreOp::eDontCare,
                   vk::ImageLayout::eUndefined,
                   vk::ImageLayout::eDepthStencilAttachmentOptimal
               )};

        vk::RenderPassCreateInfo createInfo({}, 2, attachments, 1, &subpass, 0);
        _transformContext.renderPass = _vkDevice.createRenderPass(createInfo);
    }

    // Create pipeline
    {
        std::string VERTEX_SHADER_PATH
            = std::string(ASSETS_PATH) + "shaders/rygb_transform.vert.spv";
        std::string FRAGMENT_SHADER_PATH
            = std::string(ASSETS_PATH) + "shaders/rygb_transform.frag.spv";

        vk::ShaderModule vertShaderModule
            = ShaderCreation::createShaderModule(_vkDevice, VERTEX_SHADER_PATH.c_str());
        vk::ShaderModule fragShaderModule
            = ShaderCreation::createShaderModule(_vkDevice, FRAGMENT_SHADER_PATH.c_str());

        std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages
            = {vk::PipelineShaderStageCreateInfo(
                   {}, vk::ShaderStageFlagBits::eVertex, vertShaderModule, "main"
               ),
               vk::PipelineShaderStageCreateInfo(
                   {}, vk::ShaderStageFlagBits::eFragment, fragShaderModule, "main"
               )};

        vk::PipelineVertexInputStateCreateInfo vertexInputInfo({}, 0, nullptr, 0, nullptr);
        vk::PipelineInputAssemblyStateCreateInfo inputAssembly(
            {}, vk::PrimitiveTopology::eTriangleList, VK_FALSE
        );

        vk::PipelineDepthStencilStateCreateInfo depthStencil(
            {}, VK_TRUE, VK_TRUE, vk::CompareOp::eLess, VK_FALSE, VK_FALSE
        );

        std::vector<vk::DynamicState> dynamicStates
            = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};

        vk::PipelineDynamicStateCreateInfo dynamicState(
            {}, dynamicStates.size(), dynamicStates.data()
        );

        vk::Viewport viewport(0.f, 0.f, _width, _height, 0.f, 1.f);
        vk::Rect2D scissor({0, 0}, {_width, _height});
        vk::PipelineViewportStateCreateInfo viewportState({}, 1, &viewport, 1, &scissor);

        vk::PipelineRasterizationStateCreateInfo rasterizer(
            {},
            VK_FALSE,
            VK_FALSE,
            vk::PolygonMode::eFill,
            vk::CullModeFlagBits::eNone,
            vk::FrontFace::eCounterClockwise,
            VK_FALSE,
            0.0f,
            0.0f,
            0.0f,
            1.0f
        );

        vk::PipelineMultisampleStateCreateInfo multisampling(
            {}, vk::SampleCountFlagBits::e1, VK_FALSE, 1.0f, nullptr, VK_FALSE, VK_FALSE
        );

        vk::PipelineColorBlendAttachmentState colorBlendAttachment(
            VK_FALSE,
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
            {}, VK_FALSE, vk::LogicOp::eCopy, 1, &colorBlendAttachment, {0.0f, 0.0f, 0.0f, 0.0f}
        );

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo(
            {}, 1, &_transformContext.descriptorSetLayout, 0, nullptr
        );

        _transformContext.pipelineLayout = _vkDevice.createPipelineLayout(pipelineLayoutInfo);

        vk::GraphicsPipelineCreateInfo pipelineInfo(
            {},
            shaderStages.size(),
            shaderStages.data(),
            &vertexInputInfo,
            &inputAssembly,
            nullptr,
            &viewportState,
            &rasterizer,
            &multisampling,
            &depthStencil,
            &colorBlending,
            &dynamicState,
            _transformContext.pipelineLayout,
            _transformContext.renderPass,
            0,
            vk::Pipeline(),
            -1
        );

        _transformContext.pipeline = _vkDevice.createGraphicsPipeline(nullptr, pipelineInfo).value;

        _vkDevice.destroyShaderModule(fragShaderModule, nullptr);
        _vkDevice.destroyShaderModule(vertShaderModule, nullptr);
    }
}

void RYGBTextureRenderer::initDescriptorSets()
{
    // Allocate descriptor sets for RGB
    {
        std::vector<vk::DescriptorSetLayout> layouts(
            NUM_FRAME_IN_FLIGHT, _transformContext.descriptorSetLayout
        );
        vk::DescriptorSetAllocateInfo allocInfo(
            _transformContext.descriptorPool, NUM_FRAME_IN_FLIGHT, layouts.data()
        );
        auto result = _vkDevice.allocateDescriptorSets(allocInfo);
        for (size_t i = 0; i < result.size(); i++) {
            _descriptorSetsRGB[i] = result[i];
        }
    }

    // Allocate descriptor sets for OCV
    {
        std::vector<vk::DescriptorSetLayout> layouts(
            NUM_FRAME_IN_FLIGHT, _transformContext.descriptorSetLayout
        );
        vk::DescriptorSetAllocateInfo allocInfo(
            _transformContext.descriptorPool, NUM_FRAME_IN_FLIGHT, layouts.data()
        );
        auto result = _vkDevice.allocateDescriptorSets(allocInfo);
        for (size_t i = 0; i < result.size(); i++) {
            _descriptorSetsOCV[i] = result[i];
        }
    }

    // Update descriptor sets
    for (int i = 0; i < NUM_FRAME_IN_FLIGHT; i++) {
        vk::DescriptorBufferInfo bufferInfo(
            _transformContext.ubo[i].buffer, 0, sizeof(RYGBTransformUBO)
        );

        vk::DescriptorImageInfo imageInfo(
            _transformContext.samplers[i], _rygbGPUTexture[i].imageView, vk::ImageLayout::eGeneral
        );

        // Update RGB descriptor set
        _vkDevice.updateDescriptorSets(
            {vk::WriteDescriptorSet(
                 _descriptorSetsRGB[i],
                 (uint32_t)RYGBTransformBindingLocation::ubo,
                 0,
                 1,
                 vk::DescriptorType::eUniformBuffer,
                 nullptr,
                 &bufferInfo,
                 nullptr
             ),
             vk::WriteDescriptorSet(
                 _descriptorSetsRGB[i],
                 (uint32_t)RYGBTransformBindingLocation::textureSampler,
                 0,
                 1,
                 vk::DescriptorType::eCombinedImageSampler,
                 &imageInfo,
                 nullptr,
                 nullptr
             )},
            nullptr
        );

        // Update OCV descriptor set (same as RGB, will update UBO before rendering)
        _vkDevice.updateDescriptorSets(
            {vk::WriteDescriptorSet(
                 _descriptorSetsOCV[i],
                 (uint32_t)RYGBTransformBindingLocation::ubo,
                 0,
                 1,
                 vk::DescriptorType::eUniformBuffer,
                 nullptr,
                 &bufferInfo,
                 nullptr
             ),
             vk::WriteDescriptorSet(
                 _descriptorSetsOCV[i],
                 (uint32_t)RYGBTransformBindingLocation::textureSampler,
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

void RYGBTextureRenderer::initOutputFrameBuffers()
{
    for (int i = 0; i < NUM_FRAME_IN_FLIGHT; i++) {
        _rgbFrameBuffer[i].Init(
            _vkDevice,
            _physicalDevice,
            _transformContext.renderPass,
            _width,
            _height,
            VK_FORMAT_R8G8B8A8_UNORM,
            _device->depthFormat,
            true // create ImGui texture
        );

        _ocvFrameBuffer[i].Init(
            _vkDevice,
            _physicalDevice,
            _transformContext.renderPass,
            _width,
            _height,
            VK_FORMAT_R8G8B8A8_UNORM,
            _device->depthFormat,
            true
        );
    }
}

void RYGBTextureRenderer::cleanupRYGBTexture()
{
    for (RYGBTexture& tex : _rygbGPUTexture) {
        _vkDevice.destroyImage(tex.image);
        _vkDevice.destroyImageView(tex.imageView);
        _vkDevice.freeMemory(tex.memory);
    }

    if (_rygbCPUBuffer.buffer != VK_NULL_HANDLE) {
        _rygbCPUBuffer.Cleanup();
    }
}

void RYGBTextureRenderer::cleanupTransformContext()
{
    for (VQBuffer& ubo : _transformContext.ubo) {
        ubo.Cleanup();
    }

    for (vk::Sampler& sampler : _transformContext.samplers) {
        _vkDevice.destroySampler(sampler);
    }

    _vkDevice.destroyDescriptorSetLayout(_transformContext.descriptorSetLayout);
    _vkDevice.destroyDescriptorPool(_transformContext.descriptorPool);
    _vkDevice.destroyRenderPass(_transformContext.renderPass);
    _vkDevice.destroyPipeline(_transformContext.pipeline);
    _vkDevice.destroyPipelineLayout(_transformContext.pipelineLayout);
}

void RYGBTextureRenderer::cleanupOutputFrameBuffers()
{
    for (int i = 0; i < NUM_FRAME_IN_FLIGHT; i++) {
        _rgbFrameBuffer[i].Cleanup();
        _ocvFrameBuffer[i].Cleanup();
    }
}

void RYGBTextureRenderer::uploadRYGBToGPU()
{
    vk::CommandBuffer cb = _device->BeginSingleTimeCommands();

    for (RYGBTexture& tex : _rygbGPUTexture) {
        if (tex.needsUpdate) {
            vk::BufferImageCopy copyRegion(
                0,
                0,
                0,
                vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
                vk::Offset3D(0, 0, 0),
                vk::Extent3D(_width, _height, 1)
            );

            cb.copyBufferToImage(
                _rygbCPUBuffer.buffer, tex.image, vk::ImageLayout::eGeneral, 1, &copyRegion
            );

            tex.needsUpdate = false;
        }
    }

    _device->EndSingleTimeCommands(cb);
}

void RYGBTextureRenderer::renderTransform(
    vk::CommandBuffer& cb,
    int frameIndex,
    ColorSpace colorSpace,
    TextureFrameBuffer& outputFB
)
{
    vk::Extent2D extent(_width, _height);
    vk::Rect2D renderArea(VkOffset2D{0, 0}, extent);

    vk::RenderPassBeginInfo renderPassBeginInfo(
        _transformContext.renderPass,
        outputFB.GetFrameBuffer(),
        renderArea,
        _clearValues.size(),
        _clearValues.data()
    );

    cb.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
    cb.setViewport(0, vk::Viewport(0.f, 0.f, extent.width, extent.height, 0.f, 1.f));
    cb.setScissor(0, vk::Rect2D({0, 0}, extent));
    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, _transformContext.pipeline);

    // Bind appropriate descriptor set (RGB or OCV)
    vk::DescriptorSet descriptorSet = (colorSpace == ColorSpace::RGB)
                                          ? _descriptorSetsRGB[frameIndex]
                                          : _descriptorSetsOCV[frameIndex];

    cb.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        _transformContext.pipelineLayout,
        0,
        1,
        &descriptorSet,
        0,
        nullptr
    );

    // Draw full-screen triangle
    cb.draw(3, 1, 0, 0);
    cb.endRenderPass();
}

} // namespace TetriumApp
