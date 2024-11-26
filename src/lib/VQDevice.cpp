#include "VQDevice.h"
#include "VQBuffer.h"
#include "VQUtils.h"
#include <cstdint>
#include <set>
#include <vulkan/vulkan_core.h>


#include "VulkanUtils.h"

void VQDevice::CreateLogicalDeviceAndQueue(const std::vector<const char*>& extensions) {
    if (!this->queueFamilyIndices.isComplete()) {
        FATAL("Queue family indices incomplete! Call InitQueueFamilyIndices().");
    }
    // check extension support
    std::unordered_set<std::string>extensionsNeeded(extensions.begin(), extensions.end());
    for (auto extension : supportedExtensions) {
        extensionsNeeded.erase(extension);
    }
    if (!extensionsNeeded.empty()) {
        for (const std::string& extension : extensionsNeeded) {
            ERROR("{} required but not supported", extension);
        }
        PANIC("device does not support all required extensions!");
    }
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilyIndices;
    uniqueQueueFamilyIndices.insert(this->queueFamilyIndices.graphicsFamily.value());
    uniqueQueueFamilyIndices.insert(this->queueFamilyIndices.presentationFamily.value());

    DEBUG("Found {} unique queue families.", uniqueQueueFamilyIndices.size());

    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.multiDrawIndirect = true; // we enable multi-draw on everything -- 99% of desktop GPUs supports it
    
    vk::PhysicalDeviceVulkan12Features deviceFeaturesVk12;
    deviceFeaturesVk12.timelineSemaphore = true;

    VkDeviceCreateInfo createInfo{};
    float queuePriority = 1.f;
    for (uint32_t queueFamily : uniqueQueueFamilyIndices) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }
    // populate createInfo
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.pNext = &deviceFeaturesVk12;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data(); // enable swapchain extension
    VK_CHECK_RESULT(vkCreateDevice(this->physicalDevice, &createInfo, nullptr, &this->logicalDevice));
    vkGetDeviceQueue(this->logicalDevice, queueFamilyIndices.graphicsFamily.value(), 0, &this->graphicsQueue);
    vkGetDeviceQueue(this->logicalDevice, queueFamilyIndices.presentationFamily.value(), 0, &this->presentationQueue);
    vkGetDeviceQueue(this->logicalDevice, queueFamilyIndices.computeFamily.value(), 0, &this->computeQueue);
}

void VQDevice::InitQueueFamilyIndices(VkSurfaceKHR surface) {
    DEBUG("Finding graphics and presentation queue families...");
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount); // initialize vector to store queue familieis
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());
    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        VkBool32 presentationSupport = false;
        if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) {
            this->queueFamilyIndices.computeFamily = i;
            DEBUG("Compute family found at {}", i);
        }
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            this->queueFamilyIndices.graphicsFamily = i;
            DEBUG("Graphics family found at {}", i);
        }
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentationSupport);
        if (presentationSupport) {
            this->queueFamilyIndices.presentationFamily = i;
            DEBUG("Presentation family found at {}", i);
        }
        if (this->queueFamilyIndices.isComplete()) {
            break;
        }
        i++;
    }
}

void VQDevice::CreateGraphicsCommandPool() {
    if (!queueFamilyIndices.graphicsFamily.has_value()) {
        FATAL("Graphics queue family not initialized! Call InitQueueFamilyIndices().");
    }
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // ALlow command buffers to be re-recorded
                                                                      // individually
    // we want to re-record the command buffer every single frame.
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(this->logicalDevice, &poolInfo, nullptr, &this->graphicsCommandPool) != VK_SUCCESS) {
        FATAL("Failed to create command pool!");
    }
}

void VQDevice::CreateGraphicsCommandBuffer(uint32_t commandBufferCount) {
    if (graphicsCommandPool == VK_NULL_HANDLE) {
        FATAL("Graphics command pool not initialized! Call CreateGraphicsCommandPool().");
    }
    if (!graphicsCommandBuffers.empty()) {
        FATAL("Graphics command buffers already initialized!");
    }
    this->graphicsCommandBuffers.resize(commandBufferCount);
    this->appCommandBuffers.resize(commandBufferCount);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = graphicsCommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)this->graphicsCommandBuffers.size();

    if (vkAllocateCommandBuffers(logicalDevice, &allocInfo, this->graphicsCommandBuffers.data()) != VK_SUCCESS) {
        FATAL("Failed to allocate command buffers!");
    }

    if (vkAllocateCommandBuffers(logicalDevice, &allocInfo, this->appCommandBuffers.data()) != VK_SUCCESS) {
        FATAL("Failed to allocate command buffers!");
    }
}

VQDevice::VQDevice(VkPhysicalDevice physicalDevice) {
    this->physicalDevice = physicalDevice;
    // Store Properties features, limits and properties of the physical device for later use
    // Device properties also contain limits and sparse properties
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);
    // Features should be checked by the examples before using them
    vkGetPhysicalDeviceFeatures(physicalDevice, &features);
    // Memory properties are used regularly for creating all kinds of buffers
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
    // Queue family properties, used for setting up requested queues upon device creation
    uint32_t queueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    assert(queueFamilyCount > 0);
    queueFamilyProperties.resize(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProperties.data());

    // Get list of supported extensions
    uint32_t extCount = 0;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, nullptr);
    if (extCount > 0) {
        std::vector<VkExtensionProperties> extensions(extCount);
        if (vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, &extensions.front())
            == VK_SUCCESS) {
            for (auto ext : extensions) {
                supportedExtensions.push_back(ext.extensionName);
            }
        }
    }

    this->depthFormat = VulkanUtils::findDepthFormat(physicalDevice);
}

void VQDevice::CreateBufferInPlace(
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VQBuffer& vqBuffer
) {
    vqBuffer.device = this->logicalDevice;
    vqBuffer.size = size;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(this->logicalDevice, &bufferInfo, nullptr, &vqBuffer.buffer) != VK_SUCCESS) {
        FATAL("Failed to create VK buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(this->logicalDevice, vqBuffer.buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = VQUtils::findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(this->logicalDevice, &allocInfo, nullptr, &vqBuffer.bufferMemory) != VK_SUCCESS) {
        FATAL("Failed to allocate device memory for buffer creation!");
    }

    vkBindBufferMemory(this->logicalDevice, vqBuffer.buffer, vqBuffer.bufferMemory, 0);

    // only map to the memory pointer if the creation has HOST_VISIBLE_BIT,
    // otherwise mapping wouldn't work anyways.
    if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        VK_CHECK_RESULT(vkMapMemory(this->logicalDevice, vqBuffer.bufferMemory, 0, vqBuffer.size, 0, &vqBuffer.bufferAddress));
    }
}

VQBuffer VQDevice::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) {
    VQBuffer vqBuffer{};
    CreateBufferInPlace(size, usage, properties, vqBuffer);
    return vqBuffer;
}

VQDevice::~VQDevice() {}

void VQDevice::Cleanup() {
    if (graphicsCommandBuffers.size() > 0) {
        vkFreeCommandBuffers(
            logicalDevice, graphicsCommandPool, graphicsCommandBuffers.size(), graphicsCommandBuffers.data()
        );
        vkFreeCommandBuffers(
            logicalDevice, graphicsCommandPool, appCommandBuffers.size(), appCommandBuffers.data()
        );
    }
    if (graphicsCommandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(logicalDevice, graphicsCommandPool, nullptr);
    }
    vkDestroyDevice(logicalDevice, nullptr);
}

size_t VQDevice::GetDynamicUBOAlignedSize(size_t dynamicUBOSize) {
    // figure out actual alignment of dynamic UBO
    size_t dynamicAlignment = dynamicUBOSize;
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(this->physicalDevice, &properties);
    size_t minUboAlignment = properties.limits.minUniformBufferOffsetAlignment;

    if (minUboAlignment > 0) {
        size_t remainder = dynamicAlignment % minUboAlignment;
        if (remainder > 0) {
            size_t padding = minUboAlignment - remainder;
            dynamicAlignment += padding;
        }
    }
    return dynamicAlignment;
}

VQDevice::SwapChainSupport VQDevice::GetSwapChainSupportForSurface(const VkSurfaceKHR surface)
{
    VkPhysicalDevice device = physicalDevice;
    ASSERT(device);

    SwapChainSupport details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    DEBUG("present mode count: {}", presentModeCount);

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            device, surface, &presentModeCount, details.presentModes.data()
        );
    }

    return details;
}
