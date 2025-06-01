// sge
#include "Commands.h"

// stl
#include <cmath>

namespace gfx
{

void transitionImageLayout(
    VkCommandBuffer cmd, VkImage image,
    VkImageLayout oldLayout, VkImageLayout newLayout,
    VkPipelineStageFlags2 srcStage, VkPipelineStageFlags2 dstStage,
    VkAccessFlags2 srcAccessMask, VkAccessFlags2 dstAccessMask,
    VkImageSubresourceRange subresourceRange
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
        .subresourceRange = subresourceRange
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
    VkImageSubresourceRange subresourceRange
)
{
    transitionImageLayout(
        cmd, image,
        oldLayout, newLayout,
        VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        VK_ACCESS_2_MEMORY_WRITE_BIT, VK_ACCESS_2_MEMORY_READ_BIT,
        subresourceRange
    );
}

void transitionImageLayoutCoarse(
    VkCommandBuffer cmd, VkImage image,
    VkImageLayout oldLayout, VkImageLayout newLayout,
    VkImageAspectFlags aspect
)
{
    VkImageSubresourceRange subresourceRange{
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
        subresourceRange
    );
}

void copyBufferToBuffer(
    VkCommandBuffer cmd,
    gfx::AllocatedBuffer srcBuffer, gfx::AllocatedBuffer dstBuffer,
    VkDeviceSize dataSize
)
{
    VkBufferCopy copyRegion{
        .srcOffset = 0,
        .dstOffset = 0,
        .size = dataSize
    };
    vkCmdCopyBuffer(cmd, srcBuffer.buffer, dstBuffer.buffer, 1, &copyRegion);
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
        .imageSubresource = VkImageSubresourceLayers{aspect, 0, 0, 1},
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
    VkImageSubresourceRange srcSubresource, VkImageSubresourceRange dstSubresource
)
{
    // transfer image formats
    transitionImageLayoutCoarse(
        cmd, srcImage,
        srcImageLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        srcSubresource
    );

    transitionImageLayoutCoarse(
        cmd, dstImage,
        dstImageLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        dstSubresource
    );

    VkImageBlit blitRegion{
        .srcSubresource = {
            srcSubresource.aspectMask, 
            srcSubresource.baseMipLevel,
            srcSubresource.baseArrayLayer, 
            srcSubresource.layerCount
        },
        .srcOffsets = {{0, 0, 0}, {static_cast<int32_t>(srcExtent.width), static_cast<int32_t>(srcExtent.height), static_cast<int32_t>(srcExtent.depth)}},
        .dstSubresource = {
            dstSubresource.aspectMask,
            dstSubresource.baseMipLevel,
            dstSubresource.baseArrayLayer,
            dstSubresource.layerCount
        },
        .dstOffsets = {{0, 0, 0}, {static_cast<int32_t>(dstExtent.width), static_cast<int32_t>(dstExtent.height), static_cast<int32_t>(dstExtent.depth)}}
    };

    vkCmdBlitImage(
        cmd,
        srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &blitRegion, VK_FILTER_LINEAR
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
    blitImageToImage(
        cmd,
        srcImage, dstImage,
        srcExtent, dstExtent,
        srcImageLayout, dstImageLayout,
        VkImageSubresourceRange{
            .aspectMask = srcAspect,
            .baseMipLevel = 0, .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0, .layerCount = 1
        },
        VkImageSubresourceRange{
            .aspectMask = dstAspect,
            .baseMipLevel = 0, .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0, .layerCount = 1
        }
    );
}

void generateMipmaps(
    VkCommandBuffer cmd,
    VkImage image,
    VkExtent2D extents, 
    VkImageLayout initialLayout,
    VkImageAspectFlags aspect
)
{
    uint32_t mipWidth = extents.width;
    uint32_t mipHeight = extents.height;

    uint32_t maxDimension = std::max(extents.width, extents.height);
    uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(maxDimension))) + 1;

    for (uint32_t level = 0; level < mipLevels - 1; ++level)
    {
        // blit current level to lower level
        VkImageSubresourceRange srcSubresource = {
            .aspectMask = aspect,
            .baseMipLevel = level,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        };
        VkImageSubresourceRange dstSubresource = {
            .aspectMask = aspect,
            .baseMipLevel = level + 1,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        };

        uint32_t nextMipWidth = (mipWidth == 1) ? 1 : mipWidth / 2;
        uint32_t nextMipHeight = (mipHeight == 1) ? 1 : mipHeight / 2;

        VkImageLayout srcLayout = (level == 0) 
            ? initialLayout : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

        blitImageToImage(
            cmd,
            image, image,
            VkExtent3D{ .width = mipWidth, .height = mipHeight, .depth = 1 },
            VkExtent3D{ .width = nextMipWidth, .height = nextMipHeight, .depth = 1 },
            srcLayout, initialLayout,
            srcSubresource, dstSubresource
        );

        mipWidth = nextMipWidth;
        mipHeight = nextMipHeight;
    }

    VkImageSubresourceRange dstSubresource = {
        .aspectMask = aspect,
        .baseMipLevel = mipLevels - 1,
        .levelCount = VK_REMAINING_MIP_LEVELS,
        .baseArrayLayer = 0,
        .layerCount = VK_REMAINING_ARRAY_LAYERS
    };

    transitionImageLayoutCoarse(
        cmd,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 
        dstSubresource
    );
}

} // namespace gfx