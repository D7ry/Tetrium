#pragma once
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan.hpp>

struct VertexInstancedData
{
    glm::mat4 model; // model mat
    int textureOffset; // offset into the texture array
};
struct Vertex
{
    /**
     * @brief Vertex struct; note all vertex shaders that uses this struct must have the same layout
     *
     */
    // 8 bytes
    glm::vec3 pos;
    // 12 bytes
    glm::vec3 color;
    glm::vec2 texCoord;
    glm::vec3 normal;
    Vertex(glm::vec3 pos) : pos(pos) {};

    static VkVertexInputBindingDescription GetBindingDescription();

    static const std::array<VkVertexInputBindingDescription, 2>* GetBindingDescriptionsInstanced();

    static const std::array<VkVertexInputAttributeDescription, 4>* GetAttributeDescriptionsPtr();

    static const std::array<vk::VertexInputAttributeDescription, 4> GetAttributeDescriptions();


    static const std::array<VkVertexInputAttributeDescription, 9>*
    GetAttributeDescriptionsInstanced();

    bool operator==(const Vertex& other) const {
        return pos == other.pos && color == other.color && texCoord == other.texCoord;
    }
};

namespace std
{
template <>
struct hash<Vertex>
{
    size_t operator()(Vertex const& vertex) const {
        return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1)
               ^ (hash<glm::vec2>()(vertex.texCoord) << 1);
    }
};
} // namespace std

/** Format cheatsheet

float: VK_FORMAT_R32_SFLOAT
vec2: VK_FORMAT_R32G32_SFLOAT
vec3: VK_FORMAT_R32G32B32_SFLOAT
vec4: VK_FORMAT_R32G32B32A32_SFLOAT

*/
