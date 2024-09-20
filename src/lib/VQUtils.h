#pragma once
#include "lib/VQDevice.h"
#include <vulkan/vulkan.h>

#include "VQBuffer.h"
#include "structs/Vertex.h"

namespace CoreUtils
{

static void createVulkanBuffer(
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkBuffer& buffer,
    VkDeviceMemory& bufferMemory
);
std::pair<VkBuffer, VkDeviceMemory> createVulkanStagingBuffer(
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    VkDeviceSize bufferSize
);
void copyVulkanBuffer(
    VkDevice device,
    VkCommandPool commandPool,
    VkQueue queue,
    VkBuffer srcBuffer,
    VkBuffer dstBuffer,
    VkDeviceSize size,
    unsigned int srcOffset= 0,
    unsigned int dstOffset= 0
);

void loadModel(
    const char* meshFilePath,
    std::vector<Vertex>& vertices,
    std::vector<uint32_t>& indices
);
} // namespace CoreUtils

namespace VQUtils
{
uint32_t findMemoryType(
    VkPhysicalDevice physicalDevice,
    uint32_t typeFilter,
    VkMemoryPropertyFlags properties
);

template <typename T>
void createIndexBuffer(
    const std::vector<T>& indices,
    VQBufferIndex& vqBuffer,
    VQDevice& vqDevice
) {
    DEBUG("Creating index buffer...");
    VkDeviceSize indexBufferSize = sizeof(T) * indices.size();

    std::pair<VkBuffer, VkDeviceMemory> res
        = CoreUtils::createVulkanStagingBuffer(
            vqDevice.physicalDevice, vqDevice.logicalDevice, indexBufferSize
        );
    VkBuffer stagingBuffer = res.first;
    VkDeviceMemory stagingBufferMemory = res.second;
    // copy over data from cpu memory to gpu memory(staging buffer)
    void* data;
    vkMapMemory(
        vqDevice.logicalDevice,
        stagingBufferMemory,
        0,
        indexBufferSize,
        0,
        &data
    );
    memcpy(data, indices.data(), (size_t)indexBufferSize); // copy the data
    vkUnmapMemory(vqDevice.logicalDevice, stagingBufferMemory);

    // create index buffer
    CoreUtils::createVulkanBuffer(
        vqDevice.physicalDevice,
        vqDevice.logicalDevice,
        indexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        vqBuffer.buffer,
        vqBuffer.bufferMemory
    );

    CoreUtils::copyVulkanBuffer(
        vqDevice.logicalDevice,
        vqDevice.graphicsCommandPool,
        vqDevice.graphicsQueue,
        stagingBuffer,
        vqBuffer.buffer,
        indexBufferSize
    );

    vqBuffer.size = indexBufferSize;
    vqBuffer.device = vqDevice.logicalDevice;
    vqBuffer.indexSize = sizeof(T);
    vqBuffer.numIndices = indices.size();

    vkDestroyBuffer(vqDevice.logicalDevice, stagingBuffer, nullptr);
    vkFreeMemory(vqDevice.logicalDevice, stagingBufferMemory, nullptr);
}

void createVertexBuffer(
    const std::vector<Vertex>& vertices,
    VQBuffer& buffer,
    VQDevice& vqDevice
);

/**
 * @brief Loads from the mesh file a model's data into a vertex buffer and index
 * buffer. Caller is responsible for freeing the buffer's resources.
 */
void meshToBuffer(
    const char* meshFilePath,
    VQDevice& vqDevice,
    VQBuffer& vertexBuffer,
    VQBufferIndex& indexBuffer
);
} // namespace VQUtils
