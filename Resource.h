#pragma once

// sge
#include "Device.h"

// Vulkan
#include <vulkan/vulkan.h>

// VMA
#include <vma/vk_mem_alloc.h>

namespace gfx
{

/*
* AllocatedBuffer
*/
struct AllocatedBuffer
{
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation alloc = VK_NULL_HANDLE;
};

AllocatedBuffer createAllocatedBuffer(
    Device* device, VkDeviceSize size, VkBufferUsageFlags usage, VmaAllocationCreateFlags memFlags);

void destroyAllocatedBuffer(Device* device, AllocatedBuffer buffer);

void writeToAllocatedBuffer(
    Device* device, void* data, VkDeviceSize dataSize, AllocatedBuffer dstBuffer);

/*
* AllocatedImage
*/
struct AllocatedImage
{
    VkImage image = VK_NULL_HANDLE;
    VmaAllocation alloc = VK_NULL_HANDLE;

    VkFormat format = VK_FORMAT_UNDEFINED;
    VkExtent2D extents = { .width = 0, .height = 0 };
};

AllocatedImage createAllocatedImage(
    Device* device, VkImageUsageFlags usage, VkFormat format, VkExtent2D extents, bool useMips);

AllocatedImage createAllocatedCubemapImage(
    Device* device, VkImageUsageFlags usage, VkFormat format, uint32_t dimension, bool useMips);

void destroyAllocatedImage(Device* device, AllocatedImage image);

/*
* VkImageView
*/
VkImageView createImageView(
    Device* device, VkImage image, VkFormat format, VkImageAspectFlags aspect
);

VkImageView createCubemapView(
    Device* device, VkImage image, VkFormat format, VkImageAspectFlags aspect
);

VkImageView createImageView(
    Device* device, VkImage image, VkFormat format, VkImageAspectFlags aspect, uint32_t arrayLayer
);

VkImageView createImageView(
    Device* device, VkImage image, VkFormat format, VkImageAspectFlags aspect, uint32_t arrayLayer, uint32_t mipLevel
);

/*
* VkSampler
*/

// use 4 bytes to fill out padding of struct below
enum class MipmapMode : uint32_t
{
    None,
    Linear,
    NearestNeighbor
};

struct SamplerDesc
{
    VkFilter magFilter = VK_FILTER_LINEAR;
    VkFilter minFilter = VK_FILTER_LINEAR;

    VkSamplerAddressMode uWrap = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode vWrap = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    MipmapMode mipmapMode = MipmapMode::Linear;

    static SamplerDesc initDefault() { return SamplerDesc(); }
};

VkSampler createSampler(
    Device* device,
    SamplerDesc samplerDescription
);

} // namespace gfx