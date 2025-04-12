#pragma once

// sge
#include "Device.h"

// Vulkan
#include <vulkan/vulkan.h>

// VMA
#include <vma/vk_mem_alloc.h>

namespace gfx
{

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

struct AllocatedImage
{
    VkImage image = VK_NULL_HANDLE;
    VmaAllocation alloc = VK_NULL_HANDLE;

    VkFormat format = VK_FORMAT_UNDEFINED;
    VkExtent2D extents = { .width = 0, .height = 0 };
};

AllocatedImage createAllocatedImage(
    Device* device, VkImageUsageFlags usage, VkFormat format, VkExtent2D extents);

void destroyAllocatedImage(Device* device, AllocatedImage image);

} // namespace gfx