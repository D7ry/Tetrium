#pragma once
#include "components/ShaderUtils.h"
#include "lib/VQDevice.h"
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <xcb/xproto.h>

class VQPipelineBuilder
{
  public:
    VQPipelineBuilder(std::shared_ptr<VQDevice> device) : _device(device) { Clear(); };

    ~VQPipelineBuilder() { Clear(); };

  private:
    std::shared_ptr<VQDevice> _device;
    std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;

    VkPipelineInputAssemblyStateCreateInfo _inputAssembly;
    VkPipelineRasterizationStateCreateInfo _rasterizer;
    VkPipelineColorBlendAttachmentState _colorBlendAttachment;
    VkPipelineMultisampleStateCreateInfo _multisampling;
    VkPipelineLayout _pipelineLayout;
    VkPipelineDepthStencilStateCreateInfo _depthStencil;
    [[maybe_unused]] VkPipelineRenderingCreateInfo _renderInfo;
    [[maybe_unused]] VkFormat _colorAttachmentformat;

    VkPipelineLayoutCreateInfo _pipelineLayoutInfo;
    VkPipelineVertexInputStateCreateInfo _vertexInputInfo;
    VkRenderPass _renderPass = VK_NULL_HANDLE;

  public:
    void Clear() {
        _inputAssembly = {.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};

        _rasterizer = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};

        _colorBlendAttachment = {};

        _multisampling = {.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};

        _pipelineLayout = {};

        _depthStencil = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};

        _renderInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};

        _shaderStages.clear();
    }

    VkPipeline BuildPipeline(VkDevice device) {
        VkPipelineViewportStateCreateInfo viewportState = {};
        {
            viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            viewportState.pNext = nullptr;

            viewportState.viewportCount = 1;
            viewportState.scissorCount = 1;
        }

        // setup dummy color blending. We arent using transparent objects yet
        // the blending is just "no blend", but we do write to the color attachment
        VkPipelineColorBlendStateCreateInfo colorBlending = {};
        {
            colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            colorBlending.pNext = nullptr;

            colorBlending.logicOpEnable = VK_FALSE;
            colorBlending.logicOp = VK_LOGIC_OP_COPY;
            colorBlending.attachmentCount = 1;
            colorBlending.pAttachments = &_colorBlendAttachment;
        }

        // build the actual pipeline
        // we now use all of the info structs we have been writing into into this one
        // to create the pipeline
        VkGraphicsPipelineCreateInfo pipelineInfo = {.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        {
            // connect the renderInfo to the pNext extension mechanism
            pipelineInfo.pNext = nullptr; // unused ifw we specify render pass
            pipelineInfo.renderPass = _renderPass;

            pipelineInfo.stageCount = (uint32_t)_shaderStages.size();
            pipelineInfo.pStages = _shaderStages.data();
            pipelineInfo.pVertexInputState = &_vertexInputInfo;
            pipelineInfo.pInputAssemblyState = &_inputAssembly;
            pipelineInfo.pViewportState = &viewportState;
            pipelineInfo.pRasterizationState = &_rasterizer;
            pipelineInfo.pMultisampleState = &_multisampling;
            pipelineInfo.pColorBlendState = &colorBlending;
            pipelineInfo.pDepthStencilState = &_depthStencil;
            pipelineInfo.layout = _pipelineLayout;
        }
        VkPipelineDynamicStateCreateInfo dynamicInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        {
            VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
            dynamicInfo.pDynamicStates = &dynamicStates[0];
            dynamicInfo.dynamicStateCount = 2;
            pipelineInfo.pDynamicState = &dynamicInfo;
        }

        // its easy to error out on create graphics pipeline, so we handle it a bit
        // better than the common VK_CHECK case
        VkPipeline newPipeline;
        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline) != VK_SUCCESS) {
            FATAL("Failed to create graphics pipeline!");
            return VK_NULL_HANDLE; // failed to create graphics pipeline
        } else {
            return newPipeline;
        }
    }

    void SetShaders(const VkShaderModule vertexShader, const VkShaderModule fragmentShader) {
        _shaderStages.clear();
        VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertexShader;
        vertShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragmentShader;
        fragShaderStageInfo.pName = "main";

        _shaderStages.push_back(vertShaderStageInfo);
        _shaderStages.push_back(fragShaderStageInfo);
    }

    void SetInputTopology(VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST) {
        _inputAssembly.topology = topology;
        _inputAssembly.primitiveRestartEnable = VK_FALSE; // don't restart primitives
    }

    void SetPolygonMode(VkPolygonMode mode = VK_POLYGON_MODE_FILL) {
        _rasterizer.polygonMode = mode;
        _rasterizer.lineWidth = 1.0f;
    }

    void SetCullMode(VkCullModeFlags cullMode = VK_CULL_MODE_NONE) {
        _rasterizer.cullMode = cullMode;
        _rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    }

    void SetMultiSamplingDisabled() {
        _multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        _multisampling.sampleShadingEnable = VK_FALSE;
        _multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        _multisampling.minSampleShading = 1.0f;          // Optional
        _multisampling.pSampleMask = nullptr;            // Optional
        _multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
        _multisampling.alphaToOneEnable = VK_FALSE;      // Optional
    }

    void SetColorBlendingDisabled() {
        // default write mask
        _colorBlendAttachment.colorWriteMask
            = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        // no blending
        _colorBlendAttachment.blendEnable = VK_FALSE;
    }

    [[maybe_unused]] void SetColorAttachmentFormat(VkFormat format) {
        _colorAttachmentformat = format;
        // connect the format to the renderInfo  structure
        _renderInfo.colorAttachmentCount = 1;
        _renderInfo.pColorAttachmentFormats = &_colorAttachmentformat;
        // maybe useful for deferred rendering in the future
    }

    [[maybe_unused]] void SetDepthFormat(VkFormat format) { _renderInfo.depthAttachmentFormat = format; }

    void SetDepthStencil(VkPipelineDepthStencilStateCreateInfo depthStencil) { _depthStencil = depthStencil; }

    void SetVertexInputInfo(VkPipelineVertexInputStateCreateInfo vertexInputInfo) {
        _vertexInputInfo = vertexInputInfo;
    }

    void SetRenderPass(VkRenderPass renderPass) { _renderPass = renderPass; }

    void SetDescriptorLayoutCreateInfo(VkPipelineLayoutCreateInfo pipelineLayoutInfo) {
        _pipelineLayoutInfo = pipelineLayoutInfo;
    }
};