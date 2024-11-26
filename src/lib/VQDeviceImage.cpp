#include "VQDeviceImage.h"
#include "VulkanUtils.h"

VQDeviceImage VQ::CreateDeviceImage(
    VQDevice device,
    vk::Extent2D extent,
    vk::Format format,
    vk::ImageUsageFlagBits usage
)
{
    NEEDS_IMPLEMENTATION();
    VQDeviceImage image{};

    vk::ImageCreateInfo imageInfo(
        {},
        vk::ImageType::e2D,
        format,
        vk::Extent3D(extent, 1),
        1,
        1,
        vk::SampleCountFlagBits::e1,
        vk::ImageTiling::eOptimal,
        usage,
        vk::SharingMode::eExclusive,
        0,
        nullptr,
        vk::ImageLayout::eUndefined
    );

    auto vkDevice = vk::Device(device.logicalDevice);
    image.image = vkDevice.createImage(imageInfo);

    vk::MemoryRequirements memRequirements = vkDevice.getImageMemoryRequirements(image.image);

    vk::MemoryAllocateInfo allocInfo(
        memRequirements.size,
        VulkanUtils::findMemoryType(
            device.physicalDevice,
            memRequirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        )
    );

    image.memory = vkDevice.allocateMemory(allocInfo);
    ASSERT(image.memory);

    vkDevice.bindImageMemory(image.image, image.memory, 0);

    return image;
};
