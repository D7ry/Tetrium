#include "TextureFrameBuffer.h"
#include "backends/imgui_impl_vulkan.h"
#include "lib/VulkanUtils.h"

void TextureFrameBuffer::Init(
    vk::Device device,
    vk::PhysicalDevice physicalDevice,
    vk::RenderPass renderPass,
    uint32_t width,
    uint32_t height,
    VkFormat imageFormat,
    VkFormat depthFormat
)
{
    _device = device;
    _physicalDevice = physicalDevice;
    _renderPass = renderPass;
    _imageFormat = imageFormat;
    _depthFormat = depthFormat;

    // Create color image & image view
    VulkanUtils::createImage(
        width,
        height,
        imageFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        _deviceImage.image,
        _deviceImage.memory,
        physicalDevice,
        device
    );
    _deviceImage.view = VulkanUtils::createImageView(
        _deviceImage.image, device, imageFormat, VK_IMAGE_ASPECT_COLOR_BIT
    );

    // Create depth image & image view
    VulkanUtils::createImage(
        width,
        height,
        depthFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        _depthImage.image,
        _depthImage.memory,
        physicalDevice,
        device
    );
    _depthImage.view = VulkanUtils::createImageView(
        _depthImage.image, device, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT
    );

    // Create framebuffer
    std::array<vk::ImageView, 2> attachments = {_deviceImage.view, _depthImage.view};
    vk::FramebufferCreateInfo fbCreateInfo(
        {}, renderPass, attachments.size(), attachments.data(), width, height, 1
    );
    _frameBuffer = device.createFramebuffer(fbCreateInfo);

    // Create sampler
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

    _sampler = device.createSampler(samplerInfo);

    // Create ImGui texture
    _imguiTextureId = ImGui_ImplVulkan_AddTexture(
        _sampler, _deviceImage.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );
}

void TextureFrameBuffer::Cleanup()
{
    _device.destroyFramebuffer(_frameBuffer);
    _device.destroySampler(_sampler);
    _device.destroyImageView(_deviceImage.view);
    _device.destroyImage(_deviceImage.image);
    _device.freeMemory(_deviceImage.memory);
    _device.destroyImageView(_depthImage.view);
    _device.destroyImage(_depthImage.image);
    _device.freeMemory(_depthImage.memory);
}

void TextureFrameBuffer::Resize(uint32_t width, uint32_t height)
{
    Cleanup();
    Init(_device, _physicalDevice, _renderPass, width, height, _imageFormat, _depthFormat);
}
