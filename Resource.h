#pragma once

// sge
#include "Device.h"

// Vulkan
#include <vulkan/vulkan.h>

// VMA
#include <vma/vk_mem_alloc.h>

namespace gfx
{

enum class MemoryUsage : uint8_t
{
    GPUOnly = 0,
    Staging = 1,
    CPUWritable = 2
};

struct Buffer
{
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation alloc = VK_NULL_HANDLE;

    Buffer() = default;
};

Buffer createBuffer(
    Device* device, MemoryUsage memUsage, void* data, VkDeviceSize dataSize, VkBufferUsageFlags usage
);

void cleanupBuffer(Device* device, Buffer buffer);

void writeBuffer(Device* device, void* data, VkDeviceSize dataSize, Buffer dstBuffer);

struct Image
{
    VkImage image = VK_NULL_HANDLE;
    VmaAllocation alloc = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;

    Image() = default;
};

Image createImage(Device* device, VkImageUsageFlags usage, VkFormat format, VkExtent3D extent);

void cleanupImage(Device* device, Image image);

} // namespace gfx