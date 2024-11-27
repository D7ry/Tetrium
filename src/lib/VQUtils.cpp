#include "VQUtils.h"
#include "lib/VQBuffer.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

// utilities not exposed
namespace CoreUtils
{
uint32_t findVulkanMemoryType(
    VkPhysicalDevice physicalDevice,
    uint32_t typeFilter,
    VkMemoryPropertyFlags properties
) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i))
            && (memProperties.memoryTypes[i].propertyFlags & properties)
                   == properties) {
            return i;
        }
    }

    FATAL("Failed to find suitable memory type!");
}

void createVulkanBuffer(
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkBuffer& buffer,
    VkDeviceMemory& bufferMemory
) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        FATAL("Failed to create VK buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findVulkanMemoryType(
        physicalDevice, memRequirements.memoryTypeBits, properties
    );

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory)
        != VK_SUCCESS) {
        FATAL("Failed to allocate device memory for buffer creation!");
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

std::pair<VkBuffer, VkDeviceMemory> createVulkanStagingBuffer(
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    VkDeviceSize bufferSize
) {
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createVulkanBuffer(
        physicalDevice,
        device,
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
            | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingBufferMemory
    );
    return {stagingBuffer, stagingBufferMemory};
}

VkCommandBuffer beginSingleTimeCommands(
    VkDevice device,
    VkCommandPool commandPool
) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void endSingleTimeCommands(
    VkCommandBuffer commandBuffer,
    VkDevice device,
    VkQueue graphicsQueue,
    VkCommandPool commandPool
) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void copyVulkanBuffer(
    VkDevice device,
    VkCommandPool commandPool,
    VkQueue queue,
    VkBuffer srcBuffer,
    VkBuffer dstBuffer,
    VkDeviceSize size,
    unsigned int srcOffset,
    unsigned int dstOffset
) {
    VkCommandBuffer commandBuffer
        = beginSingleTimeCommands(device, commandPool);

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    copyRegion.srcOffset = srcOffset;
    copyRegion.dstOffset = dstOffset;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    endSingleTimeCommands(commandBuffer, device, queue, commandPool);
}

void loadModel(
    const char* meshFilePath,
    std::vector<Vertex>& vertices,
    std::vector<uint32_t>& indices
) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;
    vertices.clear();
    indices.clear();

    // Load the OBJ file
    if (!tinyobj::LoadObj(
            &attrib, &shapes, &materials, &warn, &err, meshFilePath
        )) {
        throw std::runtime_error(warn + err);
    }

    // Deduplication map for unique vertices
    std::unordered_map<Vertex, uint32_t> uniqueVertices; // unique vertex -> index

    // Iterate through the shapes and process the mesh data
    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            // Construct the vertex
            Vertex vertex{
                glm::vec3{
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]
                }
            };

            // Load texture coordinates (if available)
            if (index.texcoord_index >= 0) {
                vertex.texCoord = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                };
            }

            // If normals are available, load them
            if (index.normal_index >= 0) {
                vertex.normal = glm::vec3{
                    attrib.normals[3 * index.normal_index + 0],
                    attrib.normals[3 * index.normal_index + 1],
                    attrib.normals[3 * index.normal_index + 2]
                };
            } else {
                // If no normal is provided, set the default normal
                vertex.normal = glm::vec3(0.0f, 0.0f, 0.0f); // Optional: You could set this to a default value
            }

            // Deduplicate vertices and add them to the list
            if (uniqueVertices.find(vertex) == uniqueVertices.end()) {
                uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                vertices.push_back(vertex);
            }

            // Add index to the indices list
            indices.push_back(uniqueVertices[vertex]);
        }
    }

    // Normals are now taken from the OBJ file, no need to calculate them manually
}
} // namespace CoreUtils

void VQUtils::createVertexBuffer(
    const std::vector<Vertex>& vertices,
    VQBuffer& vqBuffer,
    VQDevice& vqDevice
) {
    VkDeviceSize vertexBufferSize = sizeof(Vertex) * vertices.size();

    std::pair<VkBuffer, VkDeviceMemory> res
        = CoreUtils::createVulkanStagingBuffer(
            vqDevice.physicalDevice, vqDevice.logicalDevice, vertexBufferSize
        );
    VkBuffer stagingBuffer = res.first;
    VkDeviceMemory stagingBufferMemory = res.second;
    // copy over data from cpu memory to gpu memory(staging buffer)
    void* data;
    vkMapMemory(
        vqDevice.logicalDevice,
        stagingBufferMemory,
        0,
        vertexBufferSize,
        0,
        &data
    );
    memcpy(data, vertices.data(), (size_t)vertexBufferSize); // copy the data
    vkUnmapMemory(vqDevice.logicalDevice, stagingBufferMemory);

    // create vertex buffer
    CoreUtils::createVulkanBuffer(
        vqDevice.physicalDevice,
        vqDevice.logicalDevice,
        vertexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT // can be used as destination in a
                                         // memory transfer operation
            | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, // local to the GPU for faster
                                             // access
        vqBuffer.buffer,
        vqBuffer.bufferMemory
    );

    CoreUtils::copyVulkanBuffer(
        vqDevice.logicalDevice,
        vqDevice.graphicsCommandPool,
        vqDevice.graphicsQueue,
        stagingBuffer,
        vqBuffer.buffer,
        vertexBufferSize
    );

    vqBuffer.size = vertexBufferSize;
    vqBuffer.device = vqDevice.logicalDevice;
    // get rid of staging buffer, it is very much temproary
    vkDestroyBuffer(vqDevice.logicalDevice, stagingBuffer, nullptr);
    vkFreeMemory(vqDevice.logicalDevice, stagingBufferMemory, nullptr);
}

void VQUtils::meshToBuffer(
    const char* meshFilePath,
    VQDevice& vqDevice,
    VQBuffer& vertexBuffer,
    VQBufferIndex& indexBuffer
) {
    INFO("Loading mesh {}", meshFilePath);
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    CoreUtils::loadModel(meshFilePath, vertices, indices);
    DEBUG(
        "loaded mode {}, {} vertices, {} indices",
        meshFilePath,
        vertices.size(),
        indices.size()
    );
    createVertexBuffer(vertices, vertexBuffer, vqDevice);
    createIndexBuffer(indices, indexBuffer, vqDevice);
    INFO("Mesh loaded");
}

uint32_t VQUtils::findMemoryType(
    VkPhysicalDevice physicalDevice,
    uint32_t typeFilter,
    VkMemoryPropertyFlags properties
) {
    return CoreUtils::findVulkanMemoryType(
        physicalDevice, typeFilter, properties
    );
}
