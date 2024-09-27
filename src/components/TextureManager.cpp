#include "TextureManager.h"
#include "VulkanUtils.h"
#include "lib/VQBuffer.h"
#include <stb_image.h>
#include <vulkan/vulkan_core.h>

TextureManager::~TextureManager()
{
    if (!_textures.empty()) {
        PANIC("Textures must be cleaned up before ending texture manager!");
    }
}

void TextureManager::Cleanup()
{
    for (auto& elem : _textures) {
        __TextureInternal& texture = elem.second;
        vkDestroyImageView(_device->logicalDevice, texture.textureImageView, nullptr);
        vkDestroyImage(_device->logicalDevice, texture.textureImage, nullptr);
        vkDestroySampler(_device->logicalDevice, texture.textureSampler, nullptr);
        vkFreeMemory(_device->logicalDevice, texture.textureImageMemory, nullptr);
    }
    _textures.clear();
}

void TextureManager::GetDescriptorImageInfo(
    const std::string& texturePath,
    VkDescriptorImageInfo& imageInfo
)
{
    DEBUG("texture path: {}", texturePath);
    if (_textures.find(texturePath) == _textures.end()) {
        LoadTexture(texturePath);
    }
    __TextureInternal& texture = _textures.at(texturePath);
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = texture.textureImageView;
    imageInfo.sampler = texture.textureSampler;
}

void TextureManager::LoadTexture(const std::string& texturePath)
{
    if (_device == VK_NULL_HANDLE) {
        FATAL("Texture manager hasn't been initialized!");
    }
    int width, height, channels;
    stbi_uc* pixels = stbi_load(texturePath.c_str(), &width, &height, &channels, STBI_rgb_alpha);

    VkDeviceSize vkTextureSize = width * height * 4; // each pixel takes up 4 bytes: 1
                                                     // for R, G, B, A each.

    if (pixels == nullptr) {
        FATAL("Failed to load texture {}", texturePath);
    }

    VQBuffer stagingBuffer = this->_device->CreateBuffer(
        vkTextureSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    // copy memory to staging buffer. we can directly copy because staging buffer is host visible
    // and mapped.
    memcpy(stagingBuffer.bufferAddress, pixels, static_cast<size_t>(vkTextureSize));
    stbi_image_free(pixels);

    // create image object
    VkImage textureImage = VK_NULL_HANDLE;
    VkImageView textureImageView = VK_NULL_HANDLE;
    VkDeviceMemory textureImageMemory = VK_NULL_HANDLE;
    VkSampler textureSampler = VK_NULL_HANDLE;

    VulkanUtils::createImage(
        width,
        height,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        textureImage,
        textureImageMemory,
        _device->physicalDevice,
        _device->logicalDevice
    );

    transitionImageLayout(
        textureImage,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );
    copyBufferToImage(
        stagingBuffer.buffer,
        textureImage,
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height)
    );
    // transition again for shader read
    transitionImageLayout(
        textureImage,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

    textureImageView = VulkanUtils::createImageView(textureImage, _device->logicalDevice);

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
        samplerInfo.maxAnisotropy = 1; // TODO: implement customization of anisotrophic filtering.

        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;

        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;

        if (vkCreateSampler(_device->logicalDevice, &samplerInfo, nullptr, &textureSampler)
            != VK_SUCCESS) {
            FATAL("Failed to create texture sampler!");
        }
    }

    _textures.emplace(std::make_pair(
        texturePath,
        __TextureInternal{
            textureImage, textureImageView, textureImageMemory, textureSampler, width, height
        }
    ));

    stagingBuffer.Cleanup(); // clean up staging buffer
}

void TextureManager::transitionImageLayout(
    VkImage image,
    VkFormat format,
    VkImageLayout oldLayout,
    VkImageLayout newLayout
)
{
    VulkanUtils::QuickCommandBuffer commandBuffer(this->_device);

    // create a barrier to transition layout
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;

    // set queue families to ignored because we're not transferring
    // queue family ownership(another usage of the barrier)
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    // before texture from staging buffer
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED
        && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
               && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) { // after texture transfer
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        FATAL("Unsupported texture layout transition!");
    }

    vkCmdPipelineBarrier(
        commandBuffer.cmdBuffer,
        sourceStage,
        destinationStage,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &barrier
    );
}

void TextureManager::copyBufferToImage(
    VkBuffer buffer,
    VkImage image,
    uint32_t width,
    uint32_t height
)
{
    VulkanUtils::QuickCommandBuffer commandBuffer(this->_device);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    // in some cases the pixels aren't tightly packed, specify them.
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(
        commandBuffer.cmdBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region
    );
}

void TextureManager::Init(std::shared_ptr<VQDevice> device) { this->_device = device; }

TextureManager::Texture TextureManager::GetTexture(const std::string& texturePath)
{
    auto it = _textures.find(texturePath);
    if (it == _textures.end()) {
        LoadTexture(texturePath);
        it = _textures.find(texturePath);
        ASSERT(it != _textures.end());
    }
    __TextureInternal& tex = it->second;
    return Texture{
        .sampler = tex.textureSampler,
        .imageView = tex.textureImageView,
        .width = tex.width,
        .height = tex.height
    };
}
