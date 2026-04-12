// sge
#include "Resource.h"
#include "Log.h"

// stl
#include <algorithm>
#include <cmath>

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
    Device* device, VkImageUsageFlags usage, VkFormat format, VkExtent2D extents, bool useMips)
{
    AllocatedImage image;

    uint32_t maxDimension = std::max(extents.width, extents.height);

    // create image and allocation
    VkImageCreateInfo imageInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = VkExtent3D{.width = extents.width, .height = extents.height, .depth = 1},
        .mipLevels = useMips ? static_cast<uint32_t>(std::floor(std::log2(maxDimension))) + 1 : 1,
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

AllocatedImage createAllocatedMultiSampleImage(
    Device* device, VkImageUsageFlags usage, VkFormat format, VkExtent2D extents, VkSampleCountFlagBits sampleCount)
{
    AllocatedImage image;

    uint32_t maxDimension = std::max(extents.width, extents.height);

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
        .samples = sampleCount,
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

AllocatedImage createAllocatedImageArray(
    Device* device, VkImageUsageFlags usage, VkFormat format, VkExtent2D extents, uint32_t arrayLayers)
{
    AllocatedImage image;

    uint32_t maxDimension = std::max(extents.width, extents.height);

    // create image and allocation
    VkImageCreateInfo imageInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = VkExtent3D{.width = extents.width, .height = extents.height, .depth = 1},
        .mipLevels = 1,
        .arrayLayers = arrayLayers,
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

AllocatedImage createAllocatedCubemapImage(
    Device* device, VkImageUsageFlags usage, VkFormat format, uint32_t dimension, bool useMips)
{
    AllocatedImage image;

    // create image and allocation
    VkImageCreateInfo imageInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = VkExtent3D{.width = dimension, .height = dimension, .depth = 1},
        .mipLevels = useMips ? static_cast<uint32_t>(std::floor(std::log2(dimension))) + 1 : 1,
        .arrayLayers = 6,
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
    image.extents = VkExtent2D{.width = dimension, .height = dimension};

    return image;
}

void destroyAllocatedImage(Device* device, AllocatedImage image)
{
    vmaDestroyImage(device->allocator, image.image, image.alloc);
    image.format = VK_FORMAT_UNDEFINED;
    image.extents = VkExtent2D{ .width = 0, .height = 0 };
}

VkImageView createImageView(
    Device* device, VkImage image, VkFormat format, VkImageAspectFlags aspect
)
{
    VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .components = VkComponentMapping{
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY
        },
        .subresourceRange = VkImageSubresourceRange{
            .aspectMask = aspect,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = VK_REMAINING_ARRAY_LAYERS
        },
    };

    VkImageView imageView;
    VK_Check(vkCreateImageView(device->device, &viewInfo, nullptr, &imageView));
    return imageView;
}

VkImageView createCubemapView(
    Device* device, VkImage image, VkFormat format, VkImageAspectFlags aspect
)
{
    VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_CUBE,
        .format = format,
        .components = VkComponentMapping{
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY
        },
        .subresourceRange = VkImageSubresourceRange{
            .aspectMask = aspect,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = VK_REMAINING_ARRAY_LAYERS
        },
    };

    VkImageView imageView;
    VK_Check(vkCreateImageView(device->device, &viewInfo, nullptr, &imageView));
    return imageView;
}

VkImageView createImageArrayView(
    Device* device, VkImage image, VkFormat format, VkImageAspectFlags aspect
)
{
    VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
        .format = format,
        .components = VkComponentMapping{
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY
        },
        .subresourceRange = VkImageSubresourceRange{
            .aspectMask = aspect,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = VK_REMAINING_ARRAY_LAYERS
        },
    };

    VkImageView imageView;
    VK_Check(vkCreateImageView(device->device, &viewInfo, nullptr, &imageView));
    return imageView;
}

VkImageView createImageView(
    Device* device, VkImage image, VkFormat format, VkImageAspectFlags aspect, uint32_t arrayLayer
)
{
    VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .components = VkComponentMapping{
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY
        },
        .subresourceRange = VkImageSubresourceRange{
            .aspectMask = aspect,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = arrayLayer,
            .layerCount = 1
        },
    };

    VkImageView imageView;
    VK_Check(vkCreateImageView(device->device, &viewInfo, nullptr, &imageView));
    return imageView;
}

VkImageView createImageView(
    Device* device, VkImage image, VkFormat format, VkImageAspectFlags aspect, uint32_t arrayLayer, uint32_t mipLevel
)
{
    VkImageViewCreateInfo viewInfo{
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .image = image,
    .viewType = VK_IMAGE_VIEW_TYPE_2D,
    .format = format,
    .components = VkComponentMapping{
        .r = VK_COMPONENT_SWIZZLE_IDENTITY,
        .g = VK_COMPONENT_SWIZZLE_IDENTITY,
        .b = VK_COMPONENT_SWIZZLE_IDENTITY,
        .a = VK_COMPONENT_SWIZZLE_IDENTITY
    },
    .subresourceRange = VkImageSubresourceRange{
        .aspectMask = aspect,
        .baseMipLevel = mipLevel,
        .levelCount = 1,
        .baseArrayLayer = arrayLayer,
        .layerCount = 1
    },
    };

    VkImageView imageView;
    VK_Check(vkCreateImageView(device->device, &viewInfo, nullptr, &imageView));
    return imageView;
}

VkSampler createSampler(
    Device* device,
    SamplerDesc samplerDescription
)
{
    VkSamplerMipmapMode mipmapMode = (samplerDescription.mipmapMode == MipmapMode::NearestNeighbor) 
        ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR;

    float maxLod = (samplerDescription.mipmapMode == MipmapMode::None) ? 0.f : VK_LOD_CLAMP_NONE;

    VkSamplerCreateInfo samplerInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .magFilter = samplerDescription.magFilter,
        .minFilter = samplerDescription.minFilter,
        .mipmapMode = mipmapMode,
        .addressModeU = samplerDescription.uWrap,
        .addressModeV = samplerDescription.vWrap,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT, // no 3d textures for now
        .mipLodBias = 0.f,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 0.f,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_NEVER,
        .minLod = 0.f,
        .maxLod = maxLod,
        .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        .unnormalizedCoordinates = VK_FALSE
    };

    VkSampler sampler;
    VK_Check(vkCreateSampler(device->device, &samplerInfo, nullptr, &sampler));
    return sampler;
}

} // namespace gfx