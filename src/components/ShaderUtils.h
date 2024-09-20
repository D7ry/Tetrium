#pragma once
#include <fstream>
#include <vulkan/vulkan_core.h>

namespace ShaderCreation
{
static std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}

VkShaderModule createShaderModule(VkDevice logicalDevice, const std::vector<char>& shaderCode);

VkShaderModule createShaderModule(VkDevice logicalDevice, const char* shaderCodeFile);

} // namespace ShaderCreation