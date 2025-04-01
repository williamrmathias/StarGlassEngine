// sge
#include "Resource.h"
#include "Log.h"

namespace gfx
{

Buffer createBuffer(
    Device* device, MemoryUsage memUsage, void* data, VkDeviceSize dataSize, VkBufferUsageFlags usage)
{
    Buffer buffer;

    VkBufferCreateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = dataSize,
        .usage = usage, // modified based on memUsage
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE, // no async transfer
        .queueFamilyIndexCount = VK_QUEUE_FAMILY_IGNORED,
        .pQueueFamilyIndices = nullptr
    };

    VmaAllocationCreateInfo allocInfo{
        .flags = 0, // modified based on memUsage
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = 0,
        .preferredFlags = 0,
        .memoryTypeBits = 0,
        .pool = VMA_NULL,
        .pUserData = nullptr,
        .priority = 0.f,
    };

    if (memUsage == MemoryUsage::GPUOnly)
    {
        // destination of transfer command
        bufferInfo.usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        VK_Check(vmaCreateBuffer(
            device->allocator,
            &bufferInfo, &allocInfo,
            &buffer.buffer, &buffer.alloc,
            nullptr
        ));
    }
    if (memUsage == MemoryUsage::Staging)
    {
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

        VK_Check(vmaCreateBuffer(
            device->allocator,
            &bufferInfo, &allocInfo,
            &buffer.buffer, &buffer.alloc,
            nullptr
        ));

        // copy data to allocation
        if (data)
        {
            vmaCopyMemoryToAllocation(device->allocator, data, buffer.alloc, 0, dataSize);
        }
    }
    if (memUsage == MemoryUsage::CPUWritable)
    {
        // todo: resizable BAR
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

        VK_Check(vmaCreateBuffer(
            device->allocator,
            &bufferInfo, &allocInfo,
            &buffer.buffer, &buffer.alloc,
            nullptr
        ));

        // copy data to allocation
        if (data)
        {
            vmaCopyMemoryToAllocation(device->allocator, data, buffer.alloc, 0, dataSize);
        }
    }

    return buffer;
}

void cleanupBuffer(Device* device, Buffer buffer)
{
    vmaDestroyBuffer(device->allocator, buffer.buffer, buffer.alloc);
}

// Not valid if the buffer is created with MemoryUsage::GPUOnly
void writeBuffer(Device* device, void* data, VkDeviceSize dataSize, Buffer dstBuffer)
{
    vmaCopyMemoryToAllocation(device->allocator, data, dstBuffer.alloc, 0, dataSize);
}

Image createImage(Device* device, VkImageUsageFlags usage, VkFormat format, VkExtent3D extent)
{
    Image image;

    // create image and allocation
    VkImageCreateInfo imageInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = extent,
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
        .pool = VMA_NULL,
        .pUserData = nullptr,
        .priority = 0.f
    };

    VK_Check(vmaCreateImage(device->allocator, &imageInfo, &allocInfo, &image.image, &image.alloc, nullptr));

    image.format = format;
    image.layout = VK_IMAGE_LAYOUT_UNDEFINED;
}

void cleanupImage(Device* device, Image image)
{
    vmaDestroyImage(device->allocator, image.image, image.alloc);
}

} // namespace gfx