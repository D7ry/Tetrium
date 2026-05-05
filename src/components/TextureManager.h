#pragma once
#include "lib/VQDevice.h"
#include "lib/VQBuffer.h"
#include "structs/ImGuiTexture.h"
#include "structs/ColorSpace.h"
#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan.hpp>
#include <array>

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
    uint32_t LoadTextureRGBA(
        const uint8_t* pixels,
        int width,
        int height,
        VkFilter filter = VK_FILTER_NEAREST
    );
    
    uint32_t LoadCubemapTexture(const std::string& imagePath);
    
    void UnLoadTexture(uint32_t handle);
    Texture GetTexture(uint32_t handle);

    void LoadImGuiTexture(uint32_t handle);
    ImGuiTexture GetImGuiTexture(uint32_t handle);

    // RYGB texture loading and transformation
    uint32_t LoadRYGBTexture(const std::string& tiffPath);
    std::pair<ImGuiTexture, ImGuiTexture> GetRYGBImGuiTextures(uint32_t rygbHandle);
    void SetRYGBTransformMatrices(const glm::mat4x4& toRGB, const glm::mat4x4& toOCV);
    void UnloadRYGBTexture(uint32_t rygbHandle);

  private:
    // RYGB-specific structures
    struct RYGBTextureHandle {
        uint32_t rgbHandle;    // Handle to RGB-transformed texture
        uint32_t ocvHandle;    // Handle to OCV-transformed texture
        uint32_t rygbSource;   // Handle to source RYGB texture (for cleanup)
    };

    struct RYGBTransformContext {
        vk::RenderPass renderPass;
        vk::Pipeline pipeline;
        vk::PipelineLayout pipelineLayout;
        vk::DescriptorPool descriptorPool;
        vk::DescriptorSetLayout descriptorSetLayout;
        std::array<VQBuffer, NUM_FRAME_IN_FLIGHT> ubo;
        std::array<vk::Sampler, NUM_FRAME_IN_FLIGHT> samplers;
        bool initialized = false;
    };

    struct RYGBToViewSpaceUBO {
        glm::mat4x4 transformMatrix;  // RYGB → RGB or RYGB → OCV
    };

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
        VkImageLayout newLayout,
        uint32_t baseArrayLayer = 0,
        uint32_t layerCount = 1
    );

    // copy over content  in the staging buffer to the actual image
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

    // RYGB private methods
    void initRYGBTransformContext();
    void cleanupRYGBTransformContext();
    uint32_t createRYGBGPUTexture(const float* rygbData, uint32_t width, uint32_t height);
    uint32_t createTransformedTexture(uint32_t rygbSourceHandle, uint32_t width, uint32_t height, ColorSpace colorSpace);

    uint32_t _nextHandle = 1;
    std::unordered_map<uint32_t, __TextureInternal> _textures; // handle -> texture obj
    std::shared_ptr<VQDevice> _device;

    // RYGB members
    RYGBTransformContext _rygbTransformCtx;
    glm::mat4x4 _rygbToRGBMatrix;
    glm::mat4x4 _rygbToOCVMatrix;
    std::unordered_map<uint32_t, RYGBTextureHandle> _rygbTextureMap;
};
