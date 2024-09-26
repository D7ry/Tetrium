#pragma once
#include "glm/glm.hpp"
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>

#include "lib/VQBuffer.h"
class Camera;

struct GraphicsContext
{
    int currentFrameInFlight; // used to index into command buffer of render
                              // systems
    int currentSwapchainImageIndex;
    vk::CommandBuffer CB; // a active command buffer.
                          // vkBeginCommandBuffer should've been called on it
    VkExtent2D currentFBextend;
    glm::mat4 mainProjectionMatrix;
};

class Profiler;

struct TickContext
{
    const Camera* mainCamera;
    // std::vector<VkFramebuffer>* swapChainFB;
    double deltaTime;
    GraphicsContext graphics;
    Profiler* profiler;
};

class VQDevice;
class TextureManager;

struct InitContext
{
    VQDevice* device;
    VkFormat swapChainImageFormat;
    TextureManager* textureManager;

    struct
    {
        VkRenderPass RGB;
        VkRenderPass CMY;
    } renderPass;

    // temporary
    // TODO: clean up
    const char* VERTEX_SHADER_SRC = "../shaders/phong/phong.vert.spv";
    const char* FRAGMENT_SHADER_RGB_SRC = "../shaders/phong/phong_rgb.frag.spv";
    const char* FRAGMENT_SHADER_CMY_SRC = "../shaders/phong/phong_cmy.frag.spv";

    /**
     * points to initialized buffer of static engine ubo
     * the callee can bind static ubo to its descriptor set by using:
     *
     * InitData initData->engineUBOStatic[i]
     *
     */
    std::array<VkDescriptorBufferInfo, NUM_FRAME_IN_FLIGHT> engineUBOStaticDescriptorBufferInfo;
};
