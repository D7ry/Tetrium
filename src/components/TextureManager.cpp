#include "backends/imgui_impl_vulkan.h"

#include "TextureManager.h"
#include "lib/VQBuffer.h"
#include "lib/VulkanUtils.h"
#include <stb_image.h>
#include <vulkan/vulkan_core.h>

namespace
{
uint32_t findMemoryType(
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
} // namespace

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
     248, 249, 249, 250, 251, 252, 253, 254, 255},
    GetIdLUTChannel(),
    GetIdLUTChannel(),
    GetIdLUTChannel()};

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

void TextureManager::GetDescriptorImageInfo(uint32_t handle, VkDescriptorImageInfo& imageInfo)
{
    if (_textures.find(handle) == _textures.end()) {
        FATAL("Texture not loaded: {}", handle);
    }
    __TextureInternal& texture = _textures.at(handle);
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = texture.textureImageView;
    imageInfo.sampler = texture.textureSampler;
}

uint32_t TextureManager::LoadCubemapTexture(const std::string& imagePath)
{
    const auto imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
    // Load the single image
    int width, height, channels;
    stbi_uc* pixels = stbi_load(imagePath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels) {
        PANIC("Failed to load texture image: {}")
    }

    // Assuming the image is a vertical cross layout (3x4 grid)
    int faceWidth = width / 4;
    int faceHeight = height / 3;
    VkDeviceSize faceSize = faceWidth * faceHeight * 4;

    // Create staging buffer
    VQBuffer stagingBuffer{};
    _device->CreateBufferInPlace(
        faceSize * 6,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer
    );

    // Copy pixel data to staging buffer

    // Extract faces from the vertical cross layout
    auto copyFace = [&](int srcX, int srcY, int faceIndex) {
        DEBUG("Copying face {} from ({}, {}) to {}", faceIndex, srcX, srcY, faceIndex);
        for (int y = 0; y < faceHeight; ++y) {
            for (int x = 0; x < faceWidth; ++x) {
                int srcIndex = ((srcY * faceHeight + y) * width + (srcX * faceWidth + x)) * 4;
                int dstOffset = (faceIndex * faceSize) + (y * faceWidth + x) * 4;
                // DEBUG("Copying pixel at ({}, {}) to {}", x, y, dstOffset);
                memcpy(
                    static_cast<char*>(stagingBuffer.bufferAddress) + dstOffset,
                    pixels + srcIndex,
                    4
                );
            }
        }
    };

    // Order: +X, -X, +Y, -Y, +Z, -Z
    copyFace(2, 1, 0); // +X
    copyFace(0, 1, 1); // -X
    copyFace(1, 0, 2); // +Y
    copyFace(1, 2, 3); // -Y
    copyFace(1, 1, 4); // +Z
    copyFace(3, 1, 5); // -Z

    // Free the pixel data
    stbi_image_free(pixels);

    // Create cubemap image
    VkImage cubemapImage;
    VkDeviceMemory cubemapImageMemory;

    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = imageFormat;
    imageCreateInfo.extent.width = faceWidth;
    imageCreateInfo.extent.height = faceHeight;
    imageCreateInfo.extent.depth = 1;
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 6;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    vkCreateImage(_device->logicalDevice, &imageCreateInfo, nullptr, &cubemapImage);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(_device->logicalDevice, cubemapImage, &memRequirements);
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(
        _device->physicalDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    vkAllocateMemory(_device->logicalDevice, &allocInfo, nullptr, &cubemapImageMemory);
    vkBindImageMemory(_device->logicalDevice, cubemapImage, cubemapImageMemory, 0);


    // Transition image layout and copy data
    for (int i = 0; i < 6; i++) {
        transitionImageLayout(
            cubemapImage,
            imageFormat,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            i,
            1
        );
    }

    VkBufferImageCopy regions[6];
    uint32_t uWidth = static_cast<uint32_t>(faceWidth);
    uint32_t uHeight = static_cast<uint32_t>(faceHeight);
    for (int i = 0; i < 6; ++i) {
        regions[i].bufferOffset = faceSize * i;
        regions[i].bufferRowLength = 0;
        regions[i].bufferImageHeight = 0;
        regions[i].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        regions[i].imageSubresource.mipLevel = 0;
        regions[i].imageSubresource.baseArrayLayer = i;
        regions[i].imageSubresource.layerCount = 1;
        regions[i].imageOffset = {0, 0, 0};
        regions[i].imageExtent = {uWidth, uHeight, 1};
    }

    {
        VulkanUtils::QuickCommandBuffer commandBuffer(_device);

        vkCmdCopyBufferToImage(
            commandBuffer.cmdBuffer,
            stagingBuffer.buffer,
            cubemapImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            6,
            regions
        );
    }

    for (int i = 0; i < 6; i++) {
        transitionImageLayout(
            cubemapImage,
            imageFormat,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            i,
            1
        );
    }

    // Create image view
    VkImageViewCreateInfo viewCreateInfo = {};
    viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCreateInfo.image = cubemapImage;
    viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    viewCreateInfo.format = imageFormat;
    viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewCreateInfo.subresourceRange.baseMipLevel = 0;
    viewCreateInfo.subresourceRange.levelCount = 1;
    viewCreateInfo.subresourceRange.baseArrayLayer = 0;
    viewCreateInfo.subresourceRange.layerCount = 6;

    VkImageView cubemapImageView;
    vkCreateImageView(_device->logicalDevice, &viewCreateInfo, nullptr, &cubemapImageView);

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

    VkSampler cubemapSampler;
    vkCreateSampler(_device->logicalDevice, &samplerCreateInfo, nullptr, &cubemapSampler);

    // Store the cubemap texture
    uint32_t handle = _nextHandle++;
    _textures[handle] = __TextureInternal{
        .textureImage = cubemapImage,
        .textureImageView = cubemapImageView,
        .textureImageMemory = cubemapImageMemory,
        .textureSampler = cubemapSampler,
        .width = faceWidth,
        .height = faceHeight};

    // Cleanup staging buffer
    stagingBuffer.Cleanup();

    return handle;
}

uint32_t TextureManager::LoadTexture(const std::string& texturePath)
{
    if (texturePath.empty()) {
        FATAL("Empty texture path!");
    }
    if (_device == VK_NULL_HANDLE) {
        FATAL("Texture manager hasn't been initialized!");
    }
    int width, height, channels;
    stbi_uc* pixels = stbi_load(texturePath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    LUT::LUTTexture(pixels, width, height, STBI_rgb_alpha);

    VkDeviceSize vkTextureSize = width * height * 4;

    if (pixels == nullptr) {
        FATAL("Failed to load texture {}", texturePath);
    }

    VQBuffer stagingBuffer = this->_device->CreateBuffer(
        vkTextureSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    memcpy(stagingBuffer.bufferAddress, pixels, static_cast<size_t>(vkTextureSize));
    stbi_image_free(pixels);

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
    transitionImageLayout(
        textureImage,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

    textureImageView = VulkanUtils::createImageView(textureImage, _device->logicalDevice);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1;
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

    uint32_t handle = _nextHandle++;
    _textures.emplace(
        handle,
        __TextureInternal{
            textureImage, textureImageView, textureImageMemory, textureSampler, width, height}
    );

    stagingBuffer.Cleanup();
    DEBUG("Texture {} loaded: {}", texturePath, handle);
    return handle;
}

void TextureManager::UnLoadTexture(uint32_t handle)
{
    auto elem = _textures.find(handle);
    if (elem == _textures.end()) {
        PANIC("Attemping to delete non-existing texture handle with id {}", handle);
    }
    __TextureInternal& texture = elem->second;
    if (texture.imguiTextureId.has_value()) {
        ImGui_ImplVulkan_RemoveTexture(static_cast<VkDescriptorSet>(texture.imguiTextureId.value())
        );
    }
    vkDestroyImageView(_device->logicalDevice, texture.textureImageView, nullptr);
    vkDestroyImage(_device->logicalDevice, texture.textureImage, nullptr);
    vkDestroySampler(_device->logicalDevice, texture.textureSampler, nullptr);
    vkFreeMemory(_device->logicalDevice, texture.textureImageMemory, nullptr);
    _textures.erase(handle);
}

void TextureManager::transitionImageLayout(
    VkImage image,
    VkFormat format,
    VkImageLayout oldLayout,
    VkImageLayout newLayout,
    uint32_t baseArrayLayer,
    uint32_t layerCount
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
    barrier.subresourceRange.baseArrayLayer = baseArrayLayer;
    barrier.subresourceRange.layerCount = layerCount;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    // before texture from staging buffer
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED
        && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) { // after texture transfer
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

TextureManager::Texture TextureManager::GetTexture(uint32_t handle)
{
    auto it = _textures.find(handle);
    if (it == _textures.end()) {
        FATAL("Texture not loaded: {}", handle);
    }
    __TextureInternal& tex = it->second;
    return Texture{
        .sampler = tex.textureSampler,
        .imageView = tex.textureImageView,
        .width = tex.width,
        .height = tex.height};
}

ImGuiTexture TextureManager::GetImGuiTexture(uint32_t handle)
{
    auto it = _textures.find(handle);
    if (it == _textures.end()) {
        FATAL("Texture not loaded: {}", handle);
    }
    __TextureInternal& tex = it->second;
    if (!tex.imguiTextureId.has_value()) {
        FATAL("Texture {} is not loaded as ImGui texture!", handle);
    }
    return ImGuiTexture{tex.imguiTextureId.value(), tex.width, tex.height};
}

void TextureManager::LoadImGuiTexture(uint32_t handle)
{
    auto it = _textures.find(handle);
    if (it == _textures.end()) {
        FATAL("Texture not loaded: {}", handle);
    }
    __TextureInternal& tex = it->second;
    if (tex.imguiTextureId.has_value()) {
        FATAL("Texture {} has its imgui texture loaded already!", handle);
    }

    tex.imguiTextureId = ImGui_ImplVulkan_AddTexture(
        tex.textureSampler, tex.textureImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );
}
