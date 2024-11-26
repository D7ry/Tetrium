#include "Vertex.h"
#include <vulkan/vulkan_core.h>

VkFormat FormatVec4 = VK_FORMAT_R32G32B32A32_SFLOAT;
VkFormat FormatVec3 = VK_FORMAT_R32G32B32_SFLOAT;
VkFormat FormatVec2 = VK_FORMAT_R32G32_SFLOAT;
VkFormat FormatVec1 = VK_FORMAT_R32_SFLOAT;

VkVertexInputBindingDescription Vertex::GetBindingDescription()
{
    // specifies number of bytes between data entries, and whether to move to
    // the next data entry after each vertex or instance
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex); // size of a single vertex object
    bindingDescription.inputRate
        = VK_VERTEX_INPUT_RATE_VERTEX; // set to VK_VERTEX_INPUT_RATE_INSTANCE
                                       // for instanced rendering(later)

    return bindingDescription;
}

const std::array<VkVertexInputAttributeDescription, 4>* Vertex::GetAttributeDescriptionsPtr()
{
    static bool initialized = false;
    // specifies how to extract a vertex attribute from a chunk of vertex data
    // originating from a binding description
    static std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions;
    if (!initialized) {
        // layout(location = 0) in vec3 inPosition;
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location
            = 0; // location directive of the input in the vertex shader
        attributeDescriptions[0].format = FormatVec3; // vec3
        attributeDescriptions[0].offset = offsetof(
            Vertex, pos
        ); // byte offset of the data from the start of the per-vertex data

        // layout(location = 1) in vec3 inColor;
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = FormatVec3; // vec3
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        // layout(location = 1) in vec3 inTexCoord
        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = FormatVec2; // vec2
        attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

        // layout(location = 3) in vec3 inNormal;
        attributeDescriptions[3].binding = 0;
        attributeDescriptions[3].location = 3;
        attributeDescriptions[3].format = FormatVec3; // vec3
        attributeDescriptions[3].offset = offsetof(Vertex, normal);
        initialized = true;
    }

    return std::addressof(attributeDescriptions);
}

const std::array<vk::VertexInputAttributeDescription, 4> Vertex::GetAttributeDescriptions() {
    auto attributeDescriptions = Vertex::GetAttributeDescriptionsPtr();

    std::array<vk::VertexInputAttributeDescription, 4> vkAttributeDescriptions;

    for (size_t i = 0; i < attributeDescriptions->size(); i++) {
        vkAttributeDescriptions[i].binding = attributeDescriptions->at(i).binding;
        vkAttributeDescriptions[i].location = attributeDescriptions->at(i).location;
        vkAttributeDescriptions[i].format = vk::Format(attributeDescriptions->at(i).format);
        vkAttributeDescriptions[i].offset = attributeDescriptions->at(i).offset;
    }
    return vkAttributeDescriptions;
}

const std::array<VkVertexInputAttributeDescription, 9>* Vertex::GetAttributeDescriptionsInstanced()
{
    static bool initialized = false;
    static std::array<VkVertexInputAttributeDescription, 9> attributeDescriptionsInstanced;
    if (!initialized) {
        // copy from the original attrDescription array
        const std::array<VkVertexInputAttributeDescription, 4>* attrDescription
            = Vertex::GetAttributeDescriptionsPtr();
        // copy over the first 4 elements from attrDescription
        memcpy(
            attributeDescriptionsInstanced.data(),                   // dst
            attrDescription->data(),                                 // src
            attrDescription->size() * sizeof(attrDescription->at(0)) // bytes
        );
        // initialize the rest 2

        { // layout(location=4) in mat4 inInstModelMat;
            // reserve 4 vec4
            // in shader, simply access location 4 as a mat4
            // https://www.reddit.com/r/vulkan/comments/8zx1hn/comment/e2m7xd8/?utm_source=share&utm_medium=web3x&utm_name=web3xcss&utm_term=1&utm_content=share_button
            attributeDescriptionsInstanced[4].binding = 1;
            attributeDescriptionsInstanced[4].location = 4;
            attributeDescriptionsInstanced[4].format = FormatVec4;
            attributeDescriptionsInstanced[4].offset = 0;

            attributeDescriptionsInstanced[5].binding = 1;
            attributeDescriptionsInstanced[5].location = 5;
            attributeDescriptionsInstanced[5].format = FormatVec4;
            attributeDescriptionsInstanced[5].offset = sizeof(glm::vec4);

            attributeDescriptionsInstanced[6].binding = 1;
            attributeDescriptionsInstanced[6].location = 6;
            attributeDescriptionsInstanced[6].format = FormatVec4;
            attributeDescriptionsInstanced[6].offset = 2 * sizeof(glm::vec4);

            attributeDescriptionsInstanced[7].binding = 1;
            attributeDescriptionsInstanced[7].location = 7;
            attributeDescriptionsInstanced[7].format = FormatVec4;
            attributeDescriptionsInstanced[7].offset = 3 * sizeof(glm::vec4);
        }

        { // layout(location=8) in int inInstTextureID;
            attributeDescriptionsInstanced[8].binding = 1;
            attributeDescriptionsInstanced[8].location = 8;
            attributeDescriptionsInstanced[8].format = VK_FORMAT_R32_UINT; // int
            attributeDescriptionsInstanced[8].offset = 4 * sizeof(glm::vec4);
        }

        initialized = true;
    }

    return std::addressof(attributeDescriptionsInstanced);
};

const std::array<VkVertexInputBindingDescription, 2>* Vertex::GetBindingDescriptionsInstanced()
{
    static std::array<VkVertexInputBindingDescription, 2> bindingDescriptionsInstanced;

    static bool initialized = false;
    if (!initialized) {
        // bind point 0: just use the non-instanced counterpart
        bindingDescriptionsInstanced[0] = GetBindingDescription();

        // bind point 1: instanced data
        bindingDescriptionsInstanced[1].binding = 1;
        bindingDescriptionsInstanced[1].stride = sizeof(VertexInstancedData);
        bindingDescriptionsInstanced[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

        initialized = true;
    }

    return std::addressof(bindingDescriptionsInstanced);
}
