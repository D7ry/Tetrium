// Handles all RYGB transform logic

#include "Tetrium.h"

// internal context of RYGB->ROCV transform
namespace Tetrium_RYGB
{

void Init() {}
} // namespace Tetrium_RYGB

// Given the content in rgyb frame buffer,
// transform the content onto either RGB or OCV representation, and paint onto
// the swapchain's frame buffer.
//
// Internally, the remapping is done by post-processing a full-screen quad:
// the full-screen quad samples the rgyb FB's texture and performs matmul on the color

void Tetrium::transformToROCVframeBuffer(
    VirtualFrameBuffer& rgybFrameBuffer,
    SwapChainContext& rocvSwapChain,
    uint32_t swapChainImageIndex,
    ColorSpace colorSpace,
    vk::CommandBuffer CB
)
{
    vk::Extent2D extend = _swapChain.extent;
    vk::Rect2D renderArea(VkOffset2D{0, 0}, extend);
    vk::RenderPassBeginInfo renderPassBeginInfo(
        _rocvTransformRenderPass,
        rocvSwapChain.frameBuffer[swapChainImageIndex],
        renderArea,
        _clearValues.size(),
        _clearValues.data(),
        nullptr
    );

    CB.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);

    CB.endRenderPass();
}
