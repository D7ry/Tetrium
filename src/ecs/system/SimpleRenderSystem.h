#pragma once
#include <map>
#include <unordered_set>
#include <vulkan/vulkan_core.h>

#include "lib/VQBuffer.h"

#include "structs/SharedEngineStructs.h"

#include "ecs/component/Component.h"
#include "ecs/component/TransformComponent.h"
#include "ecs/system/System.h"

// a blinn-phong mesh that lives on GPU
struct Mesh
{
    VQBuffer vertexBuffer;
    VQBufferIndex indexBuffer;
};

struct MeshComponent : IComponent
{
    Mesh* mesh;
    size_t dynamicUBOId; // id, id * sizeof(dynamicUBO) = offset of its own
                         // dynamic UBO
    int textureOffset;   // index into the texture array
};

// dynamic UBO of phong render, every mesh instance has its own dynamic UBO
struct UBODynamic
{
    glm::mat4 model;
    int textureOffset;
};

// simple blinn-phong rendering system
// the system is a POC and does not scale optimally
class SimpleRenderSystem : public ISystem
{
  public:
    MeshComponent* MakeMeshInstanceComponent(
        const std::string& meshPath,
        const std::string& texturePath
    );

    // destroy a mesh instance component that's initialized with
    // `MakePhongMeshInstanceComponent` freeing the pointer
    void DestroyMeshComponent(MeshComponent*& component);

    // TODO: clean up the OOP mess
    void Init(const InitContext* ctx) override;
    void Tick(const TickContext* ctx) override {};

    // TODO: clean up to make it more DOD
    void TickRGB(const TickContext* ctx);
    void TickCMY(const TickContext* ctx);

    void Cleanup() override;

  private:
    // spir-v source to vertex and fragment shader, relative to compiled binary
    const char* VERTEX_SHADER_SRC = "../shaders/phong/phong.vert.spv";
    const char* FRAGMENT_SHADER_SRC = "../shaders/phong/phong.frag.spv";

    struct UBO
    {
        VQBuffer dynamicUBO;
    };

    struct RenderSystemContext
    {
        std::array<VkDescriptorSet, NUM_FRAME_IN_FLIGHT> _descriptorSets;
        std::array<UBO, NUM_FRAME_IN_FLIGHT> _UBO;
        VkPipeline _pipeline = VK_NULL_HANDLE;
        VkPipelineLayout _pipelineLayout = VK_NULL_HANDLE;
    };

    struct
    {
        RenderSystemContext RGB;
        RenderSystemContext CMY;
    } _renderSystemContexts;


    void render(const TickContext* tickCtx, RenderSystemContext& renderCtx);


    // sysetm holds its own descriptor pool
    VkDescriptorSetLayout _descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool _descriptorPool = VK_NULL_HANDLE;

    size_t _numDynamicUBO = 0;
    ; // how many dynamic UBOs do we have
    size_t _currDynamicUBO = 0;
    ;                                              // dynamic ubo that is to be allocated
    std::vector<unsigned long> _freeDynamicUBOIdx; // free list for dynamic UBO

    size_t _dynamicUBOAlignmentSize; // actual size of the dynamic UBO that
                                     // satisfies device alignment

    void resizeDynamicUbo(RenderSystemContext& ctx, size_t dynamicUboCount);

    VQDevice* _device = nullptr;

    // initialize resources for graphics pipeline,
    // - shaders
    // - descriptors(pool, layout, sets)
    // and the pipeline itself
    void createGraphicsPipeline(
        const VkRenderPass renderPassRGB,
        const VkRenderPass renderPassCMY,
        const InitContext* initData
    );

    void buildPipelineForContext(
        const VkRenderPass pass,
        const InitContext* initData,
        RenderSystemContext& ctx
    );

    // all phong meshes created
    std::unordered_map<std::string, Mesh> _meshes;

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

    enum class BindingLocation : unsigned int
    {
        UBO_STATIC_ENGINE = 0,
        UBO_DYNAMIC = 1,
        TEXTURE_SAMPLER = 2
    };
};
