#pragma once
#include <map>
#include <unordered_set>
#include <vulkan/vulkan_core.h>

#include "lib/VQBuffer.h"

#include "structs/SharedEngineStructs.h"

#include "ecs/component/Component.h"
#include "ecs/component/TransformComponent.h"
#include "ecs/system/System.h"

struct RYGBImageComponent : IComponent
{
};

// system that renders a RYGB image to a quad
class TetraImageDisplaySystem : public ISystem
{
  public:
    // TODO: clean up the OOP mess
    void Init(const InitContext* ctx) override;

    void Tick(const TickContext* ctx) override
    {
        FATAL("Image Display System does not perform Tick(ctx)");
    }

    void Tick(const TickContext* ctx, const std::string& imagePath);

    void LoadTexture(const std::string& imagePath);

    void Cleanup() override;

    void AddEntity(Entity* e) override { FATAL("Image Display System does not accept entities"); }

  private:

    struct SystemUBOStatic {
        int textureIndex; // index of the RYGB image to be painted onto the quad.
    };
    struct RenderSystemContext
    {
        std::array<VQBuffer, NUM_FRAME_IN_FLIGHT> _systemUBOStatic;
        std::array<VkDescriptorSet, NUM_FRAME_IN_FLIGHT> _descriptorSets;
        VkPipeline _pipeline = VK_NULL_HANDLE;
        VkPipelineLayout _pipelineLayout = VK_NULL_HANDLE;
    };

    RenderSystemContext _renderCtx;

    // sysetm holds its own descriptor pool
    VkDescriptorSetLayout _descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool _descriptorPool = VK_NULL_HANDLE;

    VQDevice* _device = nullptr;

    // initialize resources for graphics pipeline,
    // - shaders
    // - descriptors(pool, layout, sets)
    // and the pipeline itself
    void createGraphicsPipeline(const VkRenderPass renderPass, const InitContext* initData);

    void buildPipelineForContext(
        const VkRenderPass pass,
        RenderSystemContext& ctx,
        const std::array<VkDescriptorBufferInfo, NUM_FRAME_IN_FLIGHT>& engineUboInfo
    );

    TextureManager* _textureManager;

    // an array of texture descriptors that gets filled up as textures are
    // loaded in
    std::array<VkDescriptorImageInfo, TEXTURE_ARRAY_SIZE> _textureDescriptorInfo;
    size_t _textureDescriptorInfoIdx
        = 0; // idx to the current texture descriptor that can be written in,
             // also represents the total # of valid texture descriptors,
             // starting from the beginning of the _textureDescriptorInfo array
    std::unordered_map<std::string, int> _textureDescriptorIndices; // texture name, index into the
                                                                    // texture descriptor array
                                                                    //
    void updateTextureDescriptorSet(); // flush the `_textureDescriptorInfo` into
                                       // device, updating the descriptor set

    void flushSystemUBO(uint32_t frame, int imageIndex);
    enum class BindingLocation : unsigned int
    {
        UBO_STATIC_ENGINE = 0,
        UBO_STATIC_SYSTEM = 1,
        TEXTURE_SAMPLER = 2
    };
};
