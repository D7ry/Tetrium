#include "TextureManager.h"
#include "VulkanUtils.h"
#include "lib/VQBuffer.h"
#include <stb_image.h>
#include <vulkan/vulkan_core.h>

// temporary LUT utilities
// TODO: this slows down image loading and doens't work for raster, should implement a LUT
// post-processing pass
namespace LUT
{
using LUTChannel = std::array<uint8_t, 256>;

constexpr LUTChannel GetIdLUTChannel()
{
    LUTChannel idChannel;
    for (int i = 0; i < idChannel.size(); i++) {
        idChannel.at(i) = i;
    }

    return idChannel;
}

const LUTChannel LUT_CHANNELS_RGBA[4] = {
    // RED
    {0,   2,   3,   4,   5,   6,   7,   8,   9,   9,   10,  12,  12,  13,  15,  16,  17,  17,  19,
     20,  21,  22,  23,  24,  24,  25,  26,  27,  28,  29,  30,  31,  32,  34,  35,  36,  37,  38,
     39,  40,  41,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,
     57,  58,  59,  60,  61,  62,  63,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,
     77,  78,  79,  80,  81,  82,  83,  84,  85,  86,  87,  88,  88,  89,  90,  91,  92,  93,  94,
     95,  97,  98,  99,  100, 101, 102, 103, 104, 105, 105, 106, 107, 108, 109, 110, 111, 112, 113,
     114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 131, 132, 133, 134,
     135, 136, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152,
     153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171,
     172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 185, 186, 187, 188, 189,
     190, 193, 192, 195, 196, 197, 198, 199, 200, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209,
     210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228,
     229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247,
     248, 249, 249, 250, 251, 252, 253, 254, 255
    },
    GetIdLUTChannel(),
    GetIdLUTChannel(),
    GetIdLUTChannel()
};

// Perform LUT mapping on a stbi-loaded texture
void LUTTexture(stbi_uc* pixels, int width, int height, int channels)
{
    ASSERT(channels == STBI_rgb_alpha);

    for (int i = 0; i < width * height * channels; i++) {
        ASSERT(pixels[i] >= 0 && pixels[i] <= 255 && "Bad pixel color value");
        int channelIdx = i % 4;
        const LUTChannel* channel = &LUT_CHANNELS_RGBA[channelIdx];

        pixels[i] = channel->at(pixels[i]);
    }
}
} // namespace LUT

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
    LUT::LUTTexture(pixels, width, height, STBI_rgb_alpha);

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
