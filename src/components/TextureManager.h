#pragma once
#include "lib/VQDevice.h"
#include "structs/ImGuiTexture.h"
#include <vulkan/vulkan_core.h>

class VQDevice;

// TODO: use a single command buffe for higher throughput; may implement our own command buffer
// "buffer".
class TextureManager
{

  public:
    struct Texture
    {
        VkSampler sampler;
        VkImageView imageView;
        int width;
        int height;
    };

    TextureManager() { _device = nullptr; };

    ~TextureManager();

    void Init(std::shared_ptr<VQDevice> device);
    // clean up and deallocate everything.
    // suggested to call before cleaning up swapchain.
    void Cleanup();

    void GetDescriptorImageInfo(uint32_t handle, VkDescriptorImageInfo& imageInfo);

    uint32_t LoadTexture(const std::string& texturePath);
    void UnLoadTexture(uint32_t handle);
    Texture GetTexture(uint32_t handle);

    void LoadImGuiTexture(uint32_t handle);
    ImGuiTexture GetImGuiTexture(uint32_t handle);

  private:
    struct __TextureInternal
    {
        VkImage textureImage;
        VkImageView textureImageView;
        VkDeviceMemory textureImageMemory; // gpu memory that holds the image.
        VkSampler textureSampler;          // sampler for shaders
        int width;
        int height;
        std::optional<void*> imguiTextureId = std::nullopt;
    };

    void transitionImageLayout(
        VkImage image,
        VkFormat format,
        VkImageLayout oldLayout,
        VkImageLayout newLayout
    );

    // copy over content  in the staging buffer to the actual image
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

    uint32_t _nextHandle = 1;
    std::unordered_map<uint32_t, __TextureInternal> _textures; // handle -> texture obj
    std::shared_ptr<VQDevice> _device;
};
