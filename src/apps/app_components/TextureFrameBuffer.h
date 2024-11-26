#pragma once

#include <vulkan/vulkan.hpp>
#include "lib/VQDeviceImage.h"

// A "virtual" frame buffer that can be used as a texture in ImGui
class TextureFrameBuffer {
public:
    void Init(vk::Device device, vk::PhysicalDevice physicalDevice, vk::RenderPass renderPass, uint32_t width, uint32_t height, VkFormat imageFormat, VkFormat depthFormat);
    void Cleanup(vk::Device device);

    vk::Framebuffer GetFrameBuffer() const { return _frameBuffer; }
    void* GetImGuiTextureId() const { return _imguiTextureId; }

private:
    vk::Framebuffer _frameBuffer = VK_NULL_HANDLE;
    vk::Sampler _sampler = VK_NULL_HANDLE;
    VQDeviceImage _deviceImage;
    VQDeviceImage _depthImage;
    void* _imguiTextureId = nullptr;
};
