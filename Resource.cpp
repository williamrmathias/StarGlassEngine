// sge
#include "Resource.h"
#include "Log.h"

namespace gfx
{

AllocatedBuffer createAllocatedBuffer(
    Device* device, VkDeviceSize size, VkBufferUsageFlags usage, VmaAllocationCreateFlags memFlags)
{
    AllocatedBuffer buffer;

    VkBufferCreateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE, // no async transfer
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr
    };

    VmaAllocationCreateInfo allocInfo{
        .flags = memFlags,
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = 0,
        .preferredFlags = 0,
        .memoryTypeBits = 0,
        .pool = VK_NULL_HANDLE,
        .pUserData = nullptr,
        .priority = 0.f,
    };

    VK_Check(vmaCreateBuffer(
        device->allocator,
        &bufferInfo, &allocInfo,
        &buffer.buffer, &buffer.alloc,
        nullptr
    ));

    return buffer;
}

void destroyAllocatedBuffer(Device* device, AllocatedBuffer buffer)
{
    vmaDestroyBuffer(device->allocator, buffer.buffer, buffer.alloc);
}

// only valid if buffer is writable
void writeToAllocatedBuffer(
    Device* device, void* data, VkDeviceSize dataSize, AllocatedBuffer dstBuffer)
{
    vmaCopyMemoryToAllocation(device->allocator, data, dstBuffer.alloc, 0, dataSize);
}

AllocatedImage createAllocatedImage(
    Device* device, VkImageUsageFlags usage, VkFormat format, VkExtent2D extents)
{
    AllocatedImage image;

    // create image and allocation
    VkImageCreateInfo imageInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = VkExtent3D{.width = extents.width, .height = extents.height, .depth = 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0, // not sharing
        .pQueueFamilyIndices = nullptr, // not sharing
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VmaAllocationCreateInfo allocInfo{
        .flags = 0,
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = 0,
        .preferredFlags = 0,
        .memoryTypeBits = 0,
        .pool = VK_NULL_HANDLE,
        .pUserData = nullptr,
        .priority = 0.f
    };

    VK_Check(vmaCreateImage(device->allocator, &imageInfo, &allocInfo, &image.image, &image.alloc, nullptr));

    image.format = format;
    image.extents = extents;

    return image;
}

void destroyAllocatedImage(Device* device, AllocatedImage image)
{
    vmaDestroyImage(device->allocator, image.image, image.alloc);
    image.format = VK_FORMAT_UNDEFINED;
    image.extents = VkExtent2D{ .width = 0, .height = 0 };
}

} // namespace gfx