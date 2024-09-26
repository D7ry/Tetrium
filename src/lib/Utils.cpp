#include "Utils.h"

void Utils::ImageTransfer::CmdCopyToFB(
    VkCommandBuffer commandBuffer,
    VkImage& src,
    VkImage& dst,
    VkExtent2D extent
)
{
    VkImageMemoryBarrier transformSwapchainBarrier{};
    transformSwapchainBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    transformSwapchainBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    transformSwapchainBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    transformSwapchainBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    transformSwapchainBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    transformSwapchainBarrier.image = dst;
    transformSwapchainBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    transformSwapchainBarrier.subresourceRange.levelCount = 1;
    transformSwapchainBarrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &transformSwapchainBarrier
    );

    VkImageCopy copyRegion{};
    copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.srcSubresource.layerCount = 1;
    copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.dstSubresource.layerCount = 1;
    copyRegion.extent.width = extent.width;
    copyRegion.extent.height = extent.height;
    copyRegion.extent.depth = 1;

    vkCmdCopyImage(
        commandBuffer,
        src,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        dst,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &copyRegion
    );

    // transform FB to be presentable
    VkImageMemoryBarrier presentBarrier{};
    presentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    presentBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    presentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    presentBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    presentBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    presentBarrier.image = dst;
    presentBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    presentBarrier.subresourceRange.levelCount = 1;
    presentBarrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &presentBarrier
    );
}
