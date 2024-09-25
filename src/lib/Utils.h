#pragma once

// generate utils

namespace Utils
{
namespace ImageTransfer
{
// record the command that transfers content from one frame buffer image to another
// both SRC and DST needs to be image associated with a frame buffer, and should have the same extent.
void CmdTransferFB(VkCommandBuffer commandBuffer, VkImage& src, VkImage& dst, VkExtent2D extent);

} // namespace ImageTransfer

} // namespace Utils
