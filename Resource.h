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

} // namespace gfx