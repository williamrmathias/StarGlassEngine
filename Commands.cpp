// sge
#include "Commands.h"

namespace gfx
{

void transitionImageLayout(
    VkCommandBuffer cmd, VkImage image,
    VkImageLayout oldLayout, VkImageLayout newLayout,
    VkPipelineStageFlags2 srcStage, VkPipelineStageFlags2 dstStage,
    VkAccessFlags2 srcAccessMask, VkAccessFlags2 dstAccessMask,
    VkImageSubresourceRange subresource
)
{
    VkImageMemoryBarrier2 imageBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext = nullptr,
        .srcStageMask = srcStage,
        .srcAccessMask = srcAccessMask,
        .dstStageMask = dstStage,
        .dstAccessMask = dstAccessMask,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = 0, // not changing families
        .dstQueueFamilyIndex = 0,
        .image = image,
        .subresourceRange = subresource
    };

    VkDependencyInfo dependencyInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = nullptr,
        .dependencyFlags = 0,
        .memoryBarrierCount = 0,
        .pMemoryBarriers = nullptr,
        .bufferMemoryBarrierCount = 0,
        .pBufferMemoryBarriers = nullptr,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &imageBarrier,
    };

    vkCmdPipelineBarrier2(cmd, &dependencyInfo);
}

void transitionImageLayoutCoarse(
    VkCommandBuffer cmd, VkImage image,
    VkImageLayout oldLayout, VkImageLayout newLayout,
    VkImageAspectFlags aspect
)
{
    VkImageSubresourceRange subresource{
        .aspectMask = aspect,
        .baseMipLevel = 0,
        .levelCount = VK_REMAINING_MIP_LEVELS,
        .baseArrayLayer = 0,
        .layerCount = VK_REMAINING_ARRAY_LAYERS
    };

    transitionImageLayout(
        cmd, image,
        oldLayout, newLayout,
        VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        VK_ACCESS_2_MEMORY_WRITE_BIT, VK_ACCESS_2_MEMORY_READ_BIT,
        subresource
    );
}

void copyBufferToImage(
    VkCommandBuffer cmd,
    AllocatedBuffer srcBuffer, AllocatedImage dstImage,
    VkImageLayout initialImageLayout, VkImageAspectFlags aspect
)
{
    transitionImageLayoutCoarse(
        cmd, dstImage.image,
        initialImageLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        aspect
    );

    VkBufferImageCopy bufferImageCopy{
        .bufferOffset = 0,
        .bufferRowLength = 0, // match image extent
        .bufferImageHeight = 0, // match image extent
        .imageSubresource = VkImageSubresourceLayers{aspect, 0, 0, VK_REMAINING_ARRAY_LAYERS},
        .imageOffset = VkOffset3D{0, 0, 0},
        .imageExtent = {dstImage.extents.width, dstImage.extents.height, 1}
    };

    vkCmdCopyBufferToImage(
        cmd,
        srcBuffer.buffer, dstImage.image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferImageCopy
    );
}

void blitImageToImage(
    VkCommandBuffer cmd,
    VkImage srcImage, VkImage dstImage,
    VkExtent3D srcExtent, VkExtent3D dstExtent,
    VkImageLayout srcImageLayout, VkImageLayout dstImageLayout,
    VkImageAspectFlags srcAspect, VkImageAspectFlags dstAspect
)
{
    // transfer image formats
    transitionImageLayoutCoarse(
        cmd, srcImage,
        srcImageLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        srcAspect
    );

    transitionImageLayoutCoarse(
        cmd, dstImage,
        dstImageLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        dstAspect
    );

    VkImageBlit blitRegion{
        .srcSubresource = {srcAspect, 0, 0, VK_REMAINING_ARRAY_LAYERS},
        .srcOffsets = {{0, 0, 0}, {static_cast<int32_t>(srcExtent.width), static_cast<int32_t>(srcExtent.height), static_cast<int32_t>(srcExtent.depth)}},
        .dstSubresource = {dstAspect, 0, 0, VK_REMAINING_ARRAY_LAYERS},
        .dstOffsets = {{0, 0, 0}, {static_cast<int32_t>(dstExtent.width), static_cast<int32_t>(dstExtent.height), static_cast<int32_t>(dstExtent.depth)}}
    };

    vkCmdBlitImage(
        cmd,
        srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &blitRegion, VK_FILTER_LINEAR
    );
}

} // namespace gfx