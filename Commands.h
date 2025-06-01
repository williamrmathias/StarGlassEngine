#pragma once

// sge
#include "Resource.h"

// Vulkan
#include <vulkan/vulkan.h>

// stl
#include <optional>

namespace gfx
{

/*
* Submits a command to 'cmd' to put a pipeline barrier to
* transition 'image' from 'oldLayout' to 'newLayout'
*/
void transitionImageLayout(
    VkCommandBuffer cmd, VkImage image,
    VkImageLayout oldLayout, VkImageLayout newLayout,
    VkPipelineStageFlags2 srcStage, VkPipelineStageFlags2 dstStage,
    VkAccessFlags2 srcAccessMask, VkAccessFlags2 dstAccessMask,
    VkImageSubresourceRange subresourceRange
);

/*
* Overloaded version of function above
* Course grained - meant for debugging
*/
void transitionImageLayoutCoarse(
    VkCommandBuffer cmd, VkImage image,
    VkImageLayout oldLayout, VkImageLayout newLayout,
    VkImageSubresourceRange subresourceRange
);

void transitionImageLayoutCoarse(
    VkCommandBuffer cmd, VkImage image,
    VkImageLayout oldLayout, VkImageLayout newLayout,
    VkImageAspectFlags aspect
);

/*
* Submits a command to `cmd` to copy `srcBuffer` into `dstBuffer`
*/
void copyBufferToBuffer(
    VkCommandBuffer cmd,
    gfx::AllocatedBuffer srcBuffer, gfx::AllocatedBuffer dstBuffer,
    VkDeviceSize dataSize
);

/*
* Submits a command to 'cmd' to copy 'srcBuffer' into 'dstImage'
* dstImage layout ends up as TRANSFER_DST
*/
void copyBufferToImage(
    VkCommandBuffer cmd,
    AllocatedBuffer srcBuffer, AllocatedImage dstImage,
    VkImageLayout initialImageLayout, VkImageAspectFlags aspect
);

/*
* Submits a command to 'cmd' to copy `srcImage` into `dstImage` - applying a linear blit
* srcImage layout ends up as TRANSFER_SRC and dstImage layout ends up TRANSFER_DST
*/
void blitImageToImage(
    VkCommandBuffer cmd,
    VkImage srcImage, VkImage dstImage,
    VkExtent3D srcExtent, VkExtent3D dstExtent,
    VkImageLayout srcImageLayout, VkImageLayout dstImageLayout,
    VkImageSubresourceRange srcSubresource, VkImageSubresourceRange dstSubresource
);

void blitImageToImage(
    VkCommandBuffer cmd,
    VkImage srcImage, VkImage dstImage,
    VkExtent3D srcExtent, VkExtent3D dstExtent,
    VkImageLayout srcImageLayout, VkImageLayout dstImageLayout,
    VkImageAspectFlags srcAspect, VkImageAspectFlags dstAspect
);

/*
* Submits a command to `cmd` to generate a mip chain for `image`
* Assumes image object has chain allocated. Uses the extents to calculate the max mip levels
* source image ends up with all mips as TRANSFER_SRC
*/
void generateMipmaps(
    VkCommandBuffer cmd,
    VkImage image,
    VkExtent2D extents,
    VkImageLayout layout,
    VkImageAspectFlags aspect
);

} // namespace gfx