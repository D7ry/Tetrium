// Utility functions for Vulkan boilterplate functions
#pragma once
#include "lib/VQDevice.h"
#include "structs/Vertex.h"
#include <vulkan/vulkan_core.h>

namespace VulkanUtils
{
/**
 * @brief RAII command buffer wrapper. Use it the same way you do for lock_guard.
 *
 */
struct QuickCommandBuffer
{
    QuickCommandBuffer(std::shared_ptr<VQDevice> device);

    ~QuickCommandBuffer();

    VkCommandBuffer cmdBuffer;

  private:
    std::shared_ptr<VQDevice> device;
};

VkCommandBuffer beginSingleTimeCommands(VkDevice device, VkCommandPool commandPool);

void endSingleTimeCommands(
    VkCommandBuffer commandBuffer,
    VkDevice device,
    VkQueue graphicsQueue,
    VkCommandPool commandPool
);

void createCommandPool(
    VkCommandPool* commandPool,
    VkCommandPoolCreateFlags flags,
    uint32_t queueFamilyIndex,
    VkDevice device
);

void createCommandBuffers(
    VkCommandBuffer* commandBuffer,
    uint32_t commandBufferCount,
    VkCommandPool commandPool,
    VkDevice device
);

void createCommandBuffers(
    std::vector<VkCommandBuffer>& commandBufferes,
    uint32_t commandBufferCount,
    VkCommandPool commandPool,
    VkDevice device
);

void createCommandPoolAndBuffers(
    VkCommandPool& commandPool,
    VkCommandBuffer& commandBuffer,
    uint32_t commandBufferCount,
    VkCommandPoolCreateFlags flags,
    uint32_t queueFamilyIndex,
    VkDevice device
);

/**
 * @brief Look for the memory flag that suits TYPEFILTER purposes.
 *
 * @param physicalDevice
 * @param typeFilter
 * @param properties
 * @return uint32_t
 */
uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);

void vkMemCopy(void* src, VkDeviceMemory dstMemory, VkDeviceSize size, VkDevice dstDevice);

void copyBuffer(
    VkDevice device,
    VkCommandPool commandPool,
    VkQueue queue,
    VkBuffer srcBuffer,
    VkBuffer dstBuffer,
    VkDeviceSize size
);

VkImageView createImageView(
    VkImage& textureImage,
    VkDevice logicalDevice,
    VkFormat format = VK_FORMAT_R8G8B8A8_SRGB,
    VkImageAspectFlags flags = VK_IMAGE_ASPECT_COLOR_BIT
);

VkFormat findBestFormat(
    const std::vector<VkFormat>& candidates,
    VkImageTiling tiling,
    VkFormatFeatureFlags features,
    VkPhysicalDevice physicalDevice
);

VkFormat findDepthFormat(VkPhysicalDevice physicalDevice);

void createImage(
    uint32_t width,
    uint32_t height,
    VkFormat format,
    VkImageTiling tiling,
    VkImageUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkImage& image,
    VkDeviceMemory& imageMemory,
    VkPhysicalDevice physicalDevice,
    VkDevice logicalDevice
);

} // namespace VulkanUtils
