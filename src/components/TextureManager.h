#pragma once
#include <vulkan/vulkan_core.h>
#include "lib/VQDevice.h"

class VQDevice;

// TODO: use a single command buffe for higher throughput; may implement our own command buffer "buffer".
class TextureManager
{

  public:
    TextureManager() { _device = nullptr; };

    ~TextureManager();

    void Init(std::shared_ptr<VQDevice> device);
    // clean up and deallocate everything.
    // suggested to call before cleaning up swapchain.
    void Cleanup();

    void GetDescriptorImageInfo(const std::string& texturePath, VkDescriptorImageInfo& imageInfo);

    void LoadTexture(const std::string& texturePath);

  private:
    struct __TextureInternal
    {
        VkImage textureImage;
        VkImageView textureImageView;
        VkDeviceMemory textureImageMemory; // gpu memory that holds the image.
        VkSampler textureSampler;          // sampler for shaders
    };

    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);

    // copy over content  in the staging buffer to the actual image
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

    std::unordered_map<std::string, __TextureInternal> _textures; // image path -> texture obj
    std::shared_ptr<VQDevice> _device;
};
