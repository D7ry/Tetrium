#include "backends/imgui_impl_vulkan.h"

#include "Pathing.h"
#include "TextureManager.h"
#include "components/Logging.h"
#include "components/ShaderUtils.h"
#include "lib/VQBuffer.h"
#include "lib/VulkanUtils.h"
#include <stb_image.h>
#include <tiffio.h>
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
    GetIdLUTChannel(),
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
    // Cleanup RYGB transform context
    cleanupRYGBTransformContext();

    for (auto& elem : _textures) {
        __TextureInternal& texture = elem.second;
        vkDestroyImageView(_device->logicalDevice, texture.textureImageView, nullptr);
        vkDestroyImage(_device->logicalDevice, texture.textureImage, nullptr);
        vkDestroySampler(_device->logicalDevice, texture.textureSampler, nullptr);
        vkFreeMemory(_device->logicalDevice, texture.textureImageMemory, nullptr);
    }
    _textures.clear();
    _rygbTextureMap.clear();
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
    const auto imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
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
        // DEBUG("Copying face {} from ({}, {}) to {}", faceIndex, srcX, srcY, faceIndex);
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
        VK_FORMAT_R8G8B8A8_UNORM,
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
        VK_FORMAT_R8G8B8A8_UNORM,
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
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

    textureImageView = VulkanUtils::createImageView(
        textureImage, _device->logicalDevice, VK_FORMAT_R8G8B8A8_UNORM
    );

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

// ============================================================================
// RYGB Texture Loading and Transformation
// ============================================================================

void TextureManager::SetRYGBTransformMatrices(const glm::mat4x4& toRGB, const glm::mat4x4& toOCV)
{
    _rygbToRGBMatrix = toRGB;
    _rygbToOCVMatrix = toOCV;
    INFO("RYGB transformation matrices set");
}

uint32_t TextureManager::LoadRYGBTexture(const std::string& tiffPath)
{
    // 1. Open TIFF with libtiff
    TIFF* tiff = TIFFOpen(tiffPath.c_str(), "r");
    if (!tiff) {
        ERROR("Failed to load RYGB TIFF: {}", tiffPath);
        return 0;
    }

    // 2. Validate format (4 channels, 32-bit float)
    uint16_t samplesPerPixel, bitsPerSample;
    TIFFGetField(tiff, TIFFTAG_SAMPLESPERPIXEL, &samplesPerPixel);
    TIFFGetField(tiff, TIFFTAG_BITSPERSAMPLE, &bitsPerSample);

    if (samplesPerPixel != 4 || bitsPerSample != 32) {
        ERROR("Invalid RYGB TIFF format: {} (expected 4ch 32bit)", tiffPath);
        TIFFClose(tiff);
        return 0;
    }

    // 3. Read dimensions
    uint32_t width, height;
    TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &height);

    // 4. Allocate CPU buffer
    std::vector<float> rygbData(width * height * 4);

    // 5. Read scanlines
    for (uint32_t row = 0; row < height; ++row) {
        float* rowData = rygbData.data() + row * width * 4;
        if (TIFFReadScanline(tiff, rowData, row, 0) < 0) {
            ERROR("Failed to read TIFF scanline {}", row);
            TIFFClose(tiff);
            return 0;
        }
    }
    TIFFClose(tiff);

    INFO("Loaded RYGB TIFF: {} ({}x{})", tiffPath, width, height);

    // 6. Create GPU texture (VK_FORMAT_R32G32B32A32_SFLOAT)
    uint32_t rygbSourceHandle = createRYGBGPUTexture(rygbData.data(), width, height);

    // 7. Initialize RYGB transform context (if not already)
    if (!_rygbTransformCtx.initialized) {
        initRYGBTransformContext();
    }

    // 8. Create RGB and OCV output framebuffers (VK_FORMAT_R8G8B8A8_UNORM)
    uint32_t rgbHandle = createTransformedTexture(rygbSourceHandle, width, height, ColorSpace::RGB);
    uint32_t ocvHandle = createTransformedTexture(rygbSourceHandle, width, height, ColorSpace::OCV);

    // 9. Store handles
    uint32_t combinedHandle = _nextHandle++;
    _rygbTextureMap[combinedHandle] = {rgbHandle, ocvHandle, rygbSourceHandle};

    INFO(
        "Created RYGB texture pair: handle={}, RGB={}, OCV={}", combinedHandle, rgbHandle, ocvHandle
    );
    return combinedHandle;
}

std::pair<ImGuiTexture, ImGuiTexture> TextureManager::GetRYGBImGuiTextures(uint32_t rygbHandle)
{
    auto it = _rygbTextureMap.find(rygbHandle);
    if (it == _rygbTextureMap.end()) {
        ERROR("Invalid RYGB texture handle: {}", rygbHandle);
        return {{}, {}};
    }

    // Load ImGui textures if not already loaded
    LoadImGuiTexture(it->second.rgbHandle);
    LoadImGuiTexture(it->second.ocvHandle);

    ImGuiTexture rgbTex = GetImGuiTexture(it->second.rgbHandle);
    ImGuiTexture ocvTex = GetImGuiTexture(it->second.ocvHandle);
    return {rgbTex, ocvTex};
}

void TextureManager::UnloadRYGBTexture(uint32_t rygbHandle)
{
    auto it = _rygbTextureMap.find(rygbHandle);
    if (it == _rygbTextureMap.end()) {
        WARN("Attempted to unload invalid RYGB texture handle: {}", rygbHandle);
        return;
    }

    UnLoadTexture(it->second.rgbHandle);
    UnLoadTexture(it->second.ocvHandle);
    UnLoadTexture(it->second.rygbSource);
    _rygbTextureMap.erase(it);
    DEBUG("Unloaded RYGB texture: {}", rygbHandle);
}

uint32_t TextureManager::createRYGBGPUTexture(
    const float* rygbData,
    uint32_t width,
    uint32_t height
)
{
    VkDevice device = _device->logicalDevice;
    VkPhysicalDevice physicalDevice = _device->physicalDevice;

    // Create staging buffer for CPU->GPU transfer
    VkDeviceSize imageSize = width * height * 4 * sizeof(float);

    VQBuffer stagingBuffer = _device->CreateBuffer(
        imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    // Copy data to staging buffer
    memcpy(stagingBuffer.bufferAddress, rygbData, static_cast<size_t>(imageSize));

    // Create GPU image (R32G32B32A32_SFLOAT)
    VkImage textureImage;
    VkDeviceMemory textureImageMemory;

    VulkanUtils::createImage(
        width,
        height,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        textureImage,
        textureImageMemory,
        physicalDevice,
        device
    );

    // Transition image layout: UNDEFINED -> TRANSFER_DST
    transitionImageLayout(
        textureImage,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );

    // Copy buffer to image
    copyBufferToImage(stagingBuffer.buffer, textureImage, width, height);

    // Transition image layout: TRANSFER_DST -> SHADER_READ_ONLY
    transitionImageLayout(
        textureImage,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

    // Cleanup staging buffer
    stagingBuffer.Cleanup();

    // Create image view
    VkImageView textureImageView = VulkanUtils::createImageView(
        textureImage, device, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT
    );

    // Create sampler
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    VkSampler textureSampler;
    if (vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) {
        PANIC("Failed to create texture sampler!");
    }

    // Store in texture map
    uint32_t handle = _nextHandle++;
    _textures[handle]
        = {.textureImage = textureImage,
           .textureImageView = textureImageView,
           .textureImageMemory = textureImageMemory,
           .textureSampler = textureSampler,
           .width = static_cast<int>(width),
           .height = static_cast<int>(height),
           .imguiTextureId = std::nullopt};

    return handle;
}

void TextureManager::initRYGBTransformContext()
{
    VkDevice device = _device->logicalDevice;
    vk::Device vkDevice = _device->logicalDevice;

    INFO("Initializing RYGB transform context...");

    // 1. Create render pass
    vk::AttachmentDescription colorAttachment{};
    colorAttachment.format = vk::Format::eR8G8B8A8Unorm;
    colorAttachment.samples = vk::SampleCountFlagBits::e1;
    colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
    colorAttachment.finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

    vk::AttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

    vk::SubpassDescription subpass{};
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    vk::SubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.srcAccessMask = vk::AccessFlagBits::eNone;
    dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

    vk::RenderPassCreateInfo renderPassInfo{};
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    _rygbTransformCtx.renderPass = vkDevice.createRenderPass(renderPassInfo);

    // 2. Create descriptor set layout
    vk::DescriptorSetLayoutBinding uboBinding{};
    uboBinding.binding = 0;
    uboBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
    uboBinding.descriptorCount = 1;
    uboBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;

    vk::DescriptorSetLayoutBinding samplerBinding{};
    samplerBinding.binding = 1;
    samplerBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    samplerBinding.descriptorCount = 1;
    samplerBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

    std::array<vk::DescriptorSetLayoutBinding, 2> bindings = {uboBinding, samplerBinding};
    vk::DescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    _rygbTransformCtx.descriptorSetLayout = vkDevice.createDescriptorSetLayout(layoutInfo);

    // 3. Create graphics pipeline
    std::vector<char> vertShaderCode
        = ShaderCreation::readFile(ASSETS_PATH + "shaders/rygb_transform.vert.spv");
    std::vector<char> fragShaderCode
        = ShaderCreation::readFile(ASSETS_PATH + "shaders/rygb_transform.frag.spv");

    vk::ShaderModule vertShaderModule(ShaderCreation::createShaderModule(device, vertShaderCode));
    vk::ShaderModule fragShaderModule(ShaderCreation::createShaderModule(device, fragShaderCode));

    vk::PipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    vk::PipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    vk::PipelineViewportStateCreateInfo viewportState{};
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    vk::PipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = vk::PolygonMode::eFill;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = vk::CullModeFlagBits::eNone;
    rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
    rasterizer.depthBiasEnable = VK_FALSE;

    vk::PipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

    vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask
        = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
          | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    colorBlendAttachment.blendEnable = VK_FALSE;

    vk::PipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    std::vector<vk::DynamicState> dynamicStates
        = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};

    vk::PipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &_rygbTransformCtx.descriptorSetLayout;

    _rygbTransformCtx.pipelineLayout = vkDevice.createPipelineLayout(pipelineLayoutInfo);

    vk::GraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = _rygbTransformCtx.pipelineLayout;
    pipelineInfo.renderPass = _rygbTransformCtx.renderPass;
    pipelineInfo.subpass = 0;

    auto result = vkDevice.createGraphicsPipeline(nullptr, pipelineInfo);
    if (result.result != vk::Result::eSuccess) {
        PANIC("Failed to create RYGB transform graphics pipeline!");
    }
    _rygbTransformCtx.pipeline = result.value;

    vkDevice.destroyShaderModule(vertShaderModule, nullptr);
    vkDevice.destroyShaderModule(fragShaderModule, nullptr);

    // 4. Create descriptor pool
    std::array<vk::DescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = vk::DescriptorType::eUniformBuffer;
    poolSizes[0].descriptorCount = NUM_FRAME_IN_FLIGHT * 100; // 100 textures max
    poolSizes[1].type = vk::DescriptorType::eCombinedImageSampler;
    poolSizes[1].descriptorCount = NUM_FRAME_IN_FLIGHT * 100;

    vk::DescriptorPoolCreateInfo poolInfo{};
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = NUM_FRAME_IN_FLIGHT * 100;

    _rygbTransformCtx.descriptorPool = vkDevice.createDescriptorPool(poolInfo);

    // 5. Create UBOs for each frame in flight
    for (int i = 0; i < NUM_FRAME_IN_FLIGHT; i++) {
        _device->CreateBufferInPlace(
            sizeof(RYGBToViewSpaceUBO),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            _rygbTransformCtx.ubo[i]
        );
    }

    // 6. Create samplers
    vk::SamplerCreateInfo samplerInfo{};
    samplerInfo.magFilter = vk::Filter::eLinear;
    samplerInfo.minFilter = vk::Filter::eLinear;
    samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;

    for (int i = 0; i < NUM_FRAME_IN_FLIGHT; i++) {
        _rygbTransformCtx.samplers[i] = vkDevice.createSampler(samplerInfo);
    }

    _rygbTransformCtx.initialized = true;
    INFO("RYGB transform context initialized successfully");
}

void TextureManager::cleanupRYGBTransformContext()
{
    if (!_rygbTransformCtx.initialized)
        return;

    vk::Device device = _device->logicalDevice;

    for (int i = 0; i < NUM_FRAME_IN_FLIGHT; i++) {
        device.destroySampler(_rygbTransformCtx.samplers[i], nullptr);
        _rygbTransformCtx.ubo[i].Cleanup();
    }

    device.destroyDescriptorPool(_rygbTransformCtx.descriptorPool, nullptr);
    device.destroyPipeline(_rygbTransformCtx.pipeline, nullptr);
    device.destroyPipelineLayout(_rygbTransformCtx.pipelineLayout, nullptr);
    device.destroyDescriptorSetLayout(_rygbTransformCtx.descriptorSetLayout, nullptr);
    device.destroyRenderPass(_rygbTransformCtx.renderPass, nullptr);

    _rygbTransformCtx.initialized = false;
    INFO("RYGB transform context cleaned up");
}

uint32_t TextureManager::createTransformedTexture(
    uint32_t rygbSourceHandle,
    uint32_t width,
    uint32_t height,
    ColorSpace colorSpace
)
{
    vk::Device device = _device->logicalDevice;
    VkPhysicalDevice physicalDevice = _device->physicalDevice;

    // 1. Create output framebuffer image (R8G8B8A8_UNORM)
    VkImage outputImage;
    VkDeviceMemory outputImageMemory;

    VulkanUtils::createImage(
        width,
        height,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        outputImage,
        outputImageMemory,
        physicalDevice,
        _device->logicalDevice
    );

    VkImageView outputImageView = VulkanUtils::createImageView(
        outputImage, _device->logicalDevice, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT
    );

    // 2. Create framebuffer
    vk::ImageView vkppOutputImageView(outputImageView);
    vk::FramebufferCreateInfo framebufferInfo{};
    framebufferInfo.renderPass = _rygbTransformCtx.renderPass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = &vkppOutputImageView;
    framebufferInfo.width = width;
    framebufferInfo.height = height;
    framebufferInfo.layers = 1;

    vk::Framebuffer framebuffer = device.createFramebuffer(framebufferInfo);

    // 3. Create descriptor set for this transformation
    vk::DescriptorSetAllocateInfo allocInfo{};
    allocInfo.descriptorPool = _rygbTransformCtx.descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &_rygbTransformCtx.descriptorSetLayout;

    vk::DescriptorSet descriptorSet = device.allocateDescriptorSets(allocInfo)[0];

    // 4. Update UBO with transformation matrix
    int frameIdx = 0; // Use first frame for one-time transformation
    RYGBToViewSpaceUBO* pUBO
        = reinterpret_cast<RYGBToViewSpaceUBO*>(_rygbTransformCtx.ubo[frameIdx].bufferAddress);
    pUBO->transformMatrix = (colorSpace == ColorSpace::RGB) ? _rygbToRGBMatrix : _rygbToOCVMatrix;

    // 5. Update descriptor set
    vk::DescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = _rygbTransformCtx.ubo[frameIdx].buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(RYGBToViewSpaceUBO);

    auto& rygbTex = _textures[rygbSourceHandle];
    vk::DescriptorImageInfo imageInfo{};
    imageInfo.sampler = _rygbTransformCtx.samplers[frameIdx];
    imageInfo.imageView = rygbTex.textureImageView;
    imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

    std::array<vk::WriteDescriptorSet, 2> descriptorWrites{};
    descriptorWrites[0].dstSet = descriptorSet;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = vk::DescriptorType::eUniformBuffer;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &bufferInfo;

    descriptorWrites[1].dstSet = descriptorSet;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pImageInfo = &imageInfo;

    device.updateDescriptorSets(descriptorWrites, nullptr);

    // 6. Execute rendering: RYGB -> RGB/OCV transformation
    VkCommandBuffer commandBuffer = _device->BeginSingleTimeCommands();

    // Note: Image layout transition from UNDEFINED to COLOR_ATTACHMENT_OPTIMAL
    // is handled by the render pass (initialLayout -> finalLayout)

    vk::RenderPassBeginInfo renderPassInfo{};
    renderPassInfo.renderPass = _rygbTransformCtx.renderPass;
    renderPassInfo.framebuffer = framebuffer;
    renderPassInfo.renderArea.offset = vk::Offset2D{0, 0};
    renderPassInfo.renderArea.extent = vk::Extent2D{width, height};

    vk::ClearValue clearColor{std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vk::CommandBuffer vkCmdBuffer(commandBuffer);
    vkCmdBuffer.beginRenderPass(&renderPassInfo, vk::SubpassContents::eInline);

    vkCmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, _rygbTransformCtx.pipeline);

    vk::Viewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(width);
    viewport.height = static_cast<float>(height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdBuffer.setViewport(0, 1, &viewport);

    vk::Rect2D scissor{};
    scissor.offset = vk::Offset2D{0, 0};
    scissor.extent = vk::Extent2D{width, height};
    vkCmdBuffer.setScissor(0, 1, &scissor);

    vkCmdBuffer.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        _rygbTransformCtx.pipelineLayout,
        0,
        1,
        &descriptorSet,
        0,
        nullptr
    );

    vkCmdBuffer.draw(3, 1, 0, 0); // Full-screen triangle

    vkCmdBuffer.endRenderPass();

    _device->EndSingleTimeCommands(commandBuffer);

    // 7. Destroy temporary framebuffer
    device.destroyFramebuffer(framebuffer, nullptr);

    // 8. Create sampler for output texture
    vk::SamplerCreateInfo samplerInfo{};
    samplerInfo.magFilter = vk::Filter::eLinear;
    samplerInfo.minFilter = vk::Filter::eLinear;
    samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;

    vk::Sampler textureSampler = device.createSampler(samplerInfo);

    // 9. Store output texture
    uint32_t handle = _nextHandle++;
    _textures[handle]
        = {.textureImage = outputImage,
           .textureImageView = outputImageView,
           .textureImageMemory = outputImageMemory,
           .textureSampler = textureSampler,
           .width = static_cast<int>(width),
           .height = static_cast<int>(height),
           .imguiTextureId = std::nullopt};

    DEBUG(
        "Created transformed texture: {} ({} -> {})",
        handle,
        rygbSourceHandle,
        (colorSpace == ColorSpace::RGB ? "RGB" : "OCV")
    );

    return handle;
}
