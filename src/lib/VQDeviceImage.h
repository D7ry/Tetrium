#pragma once
#include <vulkan/vulkan.hpp>

#include "VQDevice.h"

// an image that lives on a device
struct VQDeviceImage
{
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
};

namespace VQ
{
VQDeviceImage CreateDeviceImage(
    VQDevice device,
    vk::Extent2D extent,
    vk::Format format,
    vk::ImageUsageFlagBits usage
);

void DestroyDeviceImage(VQDevice& device, VQDeviceImage& image);
}; // namespace VQ
