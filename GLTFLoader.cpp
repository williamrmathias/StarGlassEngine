// sge
#include "Log.h"
#include "Commands.h"
#include "Utils.h"
#include "GLTFLoader.h"
#include "RenderEngine.h"
#include "Resource.h"

// cgltf
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

// stb
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

// glm
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

// stl
#include <span>
#include <utility>
#include <filesystem>

VkVertexInputBindingDescription Vertex::getInputBindingDescription()
{
    VkVertexInputBindingDescription bindingDesc{};
    bindingDesc.binding = 0;
    bindingDesc.stride = sizeof(Vertex);
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return bindingDesc;
}

std::array<VkVertexInputAttributeDescription, 4> Vertex::getInputAttributeDescription()
{
    std::array<VkVertexInputAttributeDescription, 4> attribDesc;

    // position attrib
    attribDesc[0].location = 0;
    attribDesc[0].binding = 0;
    attribDesc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribDesc[0].offset = offsetof(Vertex, position);

    // normal attrib
    attribDesc[1].location = 1;
    attribDesc[1].binding = 0;
    attribDesc[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribDesc[1].offset = offsetof(Vertex, normal);

    // uv attrib
    attribDesc[2].location = 2;
    attribDesc[2].binding = 0;
    attribDesc[2].format = VK_FORMAT_R32G32_SFLOAT;
    attribDesc[2].offset = offsetof(Vertex, uv);

    // color attrib
    attribDesc[3].location = 3;
    attribDesc[3].binding = 0;
    attribDesc[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attribDesc[3].offset = offsetof(Vertex, color);

    return attribDesc;
}

void LoadedGltf::initDefaultAssets()
{
    {
        // default white image
        uint32_t whiteData = glm::packUnorm4x8(glm::vec4(1.f, 1.f, 1.f, 1.f));
        VkDeviceSize imageDataSize = static_cast<VkDeviceSize>(1 * 1 * STBI_rgb_alpha);

        gfx::Device* device = engine->device.get();

        gfx::AllocatedBuffer stagingBuffer = gfx::createAllocatedBuffer(
            device, imageDataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
        );

        gfx::writeToAllocatedBuffer(device, &whiteData, imageDataSize, stagingBuffer);

        gfx::AllocatedImage newImage = gfx::createAllocatedImage(
            device, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            VK_FORMAT_R8G8B8A8_UNORM, VkExtent2D{ 1, 1 }, /*useMips*/false
        );

        VkImageSubresourceLayers subresource{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1
        };

        VkCommandBuffer cmd = engine->startImmediateCommands();
        gfx::copyBufferToImage(
            cmd,
            stagingBuffer, newImage,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_ASPECT_COLOR_BIT
        );
        gfx::transitionImageLayoutCoarse(
            cmd, newImage.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_ASPECT_COLOR_BIT
        );
        engine->endAndSubmitImmediateCommands();

        gfx::destroyAllocatedBuffer(device, stagingBuffer);
        images.push_back(newImage);

        assert(images.size() == 1);

        AssetId id = util::fastHash(&whiteData, static_cast<int>(imageDataSize));
        imageMap[id] = defaultHandle;
    }

    {
        // checker board error image
        uint32_t magenta = glm::packUnorm4x8(glm::vec4(1.f, 0.f, 1.f, 1.f));
        uint32_t black = glm::packUnorm4x8(glm::vec4(0.f, 0.f, 0.f, 1.f));
        uint32_t checkerData[4] = {
            magenta, black,
            black,   magenta
        };

        VkDeviceSize imageDataSize = static_cast<VkDeviceSize>(2 * 2 * STBI_rgb_alpha);

        gfx::Device* device = engine->device.get();

        gfx::AllocatedBuffer stagingBuffer = gfx::createAllocatedBuffer(
            device, imageDataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
        );

        gfx::writeToAllocatedBuffer(device, checkerData, imageDataSize, stagingBuffer);

        gfx::AllocatedImage newImage = gfx::createAllocatedImage(
            device, 
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            VK_FORMAT_R8G8B8A8_UNORM, VkExtent2D{ 2, 2 }, /*useMips*/true
        );

        VkImageSubresourceLayers subresource{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1
        };

        VkCommandBuffer cmd = engine->startImmediateCommands();
        gfx::copyBufferToImage(
            cmd,
            stagingBuffer, newImage,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_ASPECT_COLOR_BIT
        );
        gfx::generateMipmaps(
            cmd,
            newImage.image, newImage.extents,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_ASPECT_COLOR_BIT
        );
        gfx::transitionImageLayoutCoarse(
            cmd, newImage.image,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_ASPECT_COLOR_BIT
        );
        engine->endAndSubmitImmediateCommands();

        gfx::destroyAllocatedBuffer(device, stagingBuffer);
        images.push_back(newImage);

        assert(images.size() == 2);

        // map invalidAssetId here
        imageMap[invalidAssetId] = errorHandle;

        // also map content hash here
        AssetId id = util::fastHash(checkerData, static_cast<int>(imageDataSize));
        imageMap[id] = errorHandle;
    }

    {
        // default sampler (linear filter - repeat wrap)
        gfx::SamplerDesc samplerDescription = gfx::SamplerDesc::initDefault();

        VkSampler newSampler = gfx::createSampler(engine->device.get(), samplerDescription);

        samplers.push_back(newSampler);
        samplerMap[invalidAssetId] = defaultHandle;

        // also map content hash here
        AssetId id = util::fastHash(&samplerDescription, sizeof(samplerDescription));
        samplerMap[id] = defaultHandle;
    }

    {
        // default texture (white texture - default sampler)
        Texture newTexture = { .image = defaultHandle, .sampler = defaultHandle };

        // create image view
        newTexture.view = gfx::createImageView(
            engine->device.get(), images[newTexture.image].image,
            VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT
        );

        textures.push_back(newTexture);
        textureMap[invalidAssetId] = defaultHandle;
    }

    {
        // default material
        Material newMaterial = Material::initMaterial();
        setMaterialDescriptor(newMaterial);

        materials.push_back(newMaterial);
        materialMap[invalidAssetId] = defaultHandle;
    }
}

static AssetId getImageId(const cgltf_image& image)
{
    if (image.uri)
    {
        return std::hash<std::string_view>{}(image.uri);
    }
    else if (image.buffer_view)
    {
        cgltf_buffer_view* bufferView = image.buffer_view;
        cgltf_buffer* buffer = bufferView->buffer;
        const uint8_t* data = static_cast<uint8_t*>(buffer->data) + bufferView->offset;

        return util::fastHash(data, static_cast<int>(bufferView->size));
    }

    return invalidAssetId;
}

struct ScopedSTBImage
{
    stbi_uc* data;

    stbi_uc* operator->() const { return data; }

    ~ScopedSTBImage()
    {
        stbi_image_free(data);
    }
};

// all images are loaded in format r8g8b8a8 unorm
// they're all uploaded to the GPU at the return of this function
void LoadedGltf::loadImages(std::span<cgltf_image> gltfImages)
{
    images.reserve(images.size() + gltfImages.size());
    std::filesystem::path imageUriPath;
    std::string decodedURI;

    for (const cgltf_image& image : gltfImages)
    {
        ScopedSTBImage imageData{ nullptr };
        VkDeviceSize imageDataSize = 0;
        int width, height, nChannels;

        if (image.uri)
        {
            util::scratchDecodeURI(image.uri, decodedURI);
            imageUriPath = path.parent_path() / decodedURI;
            imageData.data = stbi_load(
                imageUriPath.string().c_str(),
                &width, &height, &nChannels, STBI_rgb_alpha
            );
            if (!imageData.data)
            {
                // use error fallback image
                SDL_LogError(0, "GLTF load error: input image has invalid uri: %s", image.name);
                continue;
            }
        }
        else if (image.buffer_view)
        {
            cgltf_buffer_view* bufferView = image.buffer_view;
            cgltf_buffer* buffer = bufferView->buffer;
            const uint8_t* data = static_cast<uint8_t*>(buffer->data) + bufferView->offset;

            imageData.data = stbi_load_from_memory(
                data, static_cast<int>(bufferView->size),
                &width, &height, &nChannels, STBI_rgb_alpha
            );
        }
        else
        {
            // use error fallback image
            SDL_LogError(0, "GLTF load error: input image has no resource view: %s", image.name);
            continue;
        }

        imageDataSize = static_cast<VkDeviceSize>(width * height * STBI_rgb_alpha);
        gfx::Device* device = engine->device.get();

        gfx::AllocatedBuffer stagingBuffer = gfx::createAllocatedBuffer(
            device, imageDataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
        );

        gfx::writeToAllocatedBuffer(device, imageData.data, imageDataSize, stagingBuffer);

        gfx::AllocatedImage newImage = gfx::createAllocatedImage(
            device, 
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            VK_FORMAT_R8G8B8A8_UNORM, VkExtent2D{ static_cast<uint32_t>(width), static_cast<uint32_t>(height) },
            /*useMips*/true
        );

        VkImageSubresourceLayers subresource{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1
        };

        VkCommandBuffer cmd = engine->startImmediateCommands();
        gfx::copyBufferToImage(
            cmd,
            stagingBuffer, newImage,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_ASPECT_COLOR_BIT
        );
        gfx::generateMipmaps(
            cmd,
            newImage.image, newImage.extents,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_ASPECT_COLOR_BIT
        );
        gfx::transitionImageLayoutCoarse(
            cmd, newImage.image,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_ASPECT_COLOR_BIT
        );
       engine->endAndSubmitImmediateCommands();

        gfx::destroyAllocatedBuffer(device, stagingBuffer);

        ImageHandle handle = images.size();
        AssetId id = getImageId(image);
        imageMap[id] = handle;

        images.push_back(newImage);
    }
}

static gfx::SamplerDesc extractSamplerDesc(
    const cgltf_sampler& sampler
)
{
    gfx::SamplerDesc samplerDescription;

    switch (sampler.mag_filter)
    {
    case cgltf_filter_type_nearest:
        samplerDescription.magFilter = VK_FILTER_NEAREST;
        break;
    case cgltf_filter_type_linear:
    default:
        samplerDescription.magFilter = VK_FILTER_LINEAR;
        break;
    }

    switch (sampler.min_filter)
    {
    case cgltf_filter_type_nearest:
    case cgltf_filter_type_nearest_mipmap_nearest:
    case cgltf_filter_type_nearest_mipmap_linear:
        samplerDescription.minFilter = VK_FILTER_NEAREST;
        break;
    case cgltf_filter_type_linear:
    case cgltf_filter_type_linear_mipmap_nearest:
    case cgltf_filter_type_linear_mipmap_linear:
    default:
        samplerDescription.minFilter = VK_FILTER_LINEAR;
        break;
    }

    switch (sampler.min_filter)
    {
    case cgltf_filter_type_nearest:
    case cgltf_filter_type_linear:
        samplerDescription.mipmapMode = gfx::MipmapMode::None;
        break;
    case cgltf_filter_type_nearest_mipmap_nearest:
    case cgltf_filter_type_linear_mipmap_nearest:
        samplerDescription.mipmapMode = gfx::MipmapMode::NearestNeighbor;
        break;
    case cgltf_filter_type_nearest_mipmap_linear:
    case cgltf_filter_type_linear_mipmap_linear:
    default:
        samplerDescription.mipmapMode = gfx::MipmapMode::Linear;
        break;
    }

    switch (sampler.wrap_s)
    {
    case cgltf_wrap_mode_clamp_to_edge:
        samplerDescription.uWrap = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        break;
    case cgltf_wrap_mode_mirrored_repeat:
        samplerDescription.uWrap = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        break;
    case cgltf_wrap_mode_repeat:
        samplerDescription.uWrap = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        break;
    }

    switch (sampler.wrap_t)
    {
    case cgltf_wrap_mode_clamp_to_edge:
        samplerDescription.vWrap = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        break;
    case cgltf_wrap_mode_mirrored_repeat:
        samplerDescription.vWrap = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        break;
    case cgltf_wrap_mode_repeat:
        samplerDescription.vWrap = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        break;
    }

    return samplerDescription;
}

static AssetId getSamplerId(const cgltf_sampler& sampler)
{
    gfx::SamplerDesc desc = extractSamplerDesc(sampler);
    return util::fastHash(&desc, sizeof(desc));
}

void LoadedGltf::loadSamplers(std::span<cgltf_sampler> gltfSamplers)
{
    samplers.reserve(samplers.size() + gltfSamplers.size());

    for (const cgltf_sampler& sampler : gltfSamplers)
    {
        gfx::SamplerDesc desc = extractSamplerDesc(sampler);

        VkSampler newSampler = gfx::createSampler(engine->device.get(), desc);

        SamplerHandle handle = samplers.size();
        AssetId id = getSamplerId(sampler);
        samplerMap[id] = handle;

        samplers.push_back(newSampler);
    }
}

static AssetId getTextureId(const cgltf_texture& texture)
{
    AssetId imageId = invalidAssetId;
    AssetId samplerId = invalidAssetId;
    if (texture.image)
    {
        imageId = getImageId(*texture.image);
    }
    if (texture.sampler)
    {
        samplerId = getSamplerId(*texture.sampler);
    }

    util::hashCombine(imageId, samplerId);
    return imageId;
}

void LoadedGltf::loadTextures(std::span<cgltf_texture> gltfTextures)
{
    textures.reserve(textures.size() + gltfTextures.size());

    for (const cgltf_texture& texture : gltfTextures)
    {
        Texture newTexture = {.image = errorHandle, .sampler = defaultHandle };
        if (texture.image)
        {
            newTexture.image = imageMap[getImageId(*texture.image)];
        }
        if (texture.sampler)
        {
            newTexture.sampler = samplerMap[getSamplerId(*texture.sampler)];
        }

        // create image view
        newTexture.view = gfx::createImageView(
            engine->device.get(), images[newTexture.image].image,
            VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT
        );

        TextureHandle handle = textures.size();
        AssetId id = getTextureId(texture);
        textureMap[id] = handle;

        textures.push_back(newTexture);
    }
}

static AssetId getMaterialId(const cgltf_material& material)
{
    if (material.has_pbr_metallic_roughness)
    {
        const cgltf_pbr_metallic_roughness& pbr = material.pbr_metallic_roughness;

        // hash PBR constants
        const cgltf_float constantData[6] = {
            pbr.base_color_factor[0], pbr.base_color_factor[1],
            pbr.base_color_factor[2], pbr.base_color_factor[3],
            pbr.metallic_factor, pbr.roughness_factor
        };

        uint64_t constantHash = util::fastHash(constantData, 6 * sizeof(constantData[0]));

        // get texture asset ids

        AssetId baseColorId = invalidAssetId;
        AssetId metalRoughId = invalidAssetId;
        if (material.pbr_metallic_roughness.base_color_texture.texture)
        {
            cgltf_texture& baseColorTex = *material.pbr_metallic_roughness.base_color_texture.texture;
            baseColorId = getTextureId(baseColorTex);
        }

        if (material.pbr_metallic_roughness.metallic_roughness_texture.texture)
        {
            cgltf_texture& metalRoughTex =
                *material.pbr_metallic_roughness.metallic_roughness_texture.texture;
            metalRoughId = getTextureId(metalRoughTex);
        }

        AssetId materialId = constantHash;
        util::hashCombine(materialId, baseColorId);
        util::hashCombine(materialId, metalRoughId);
        return materialId;
    }

    // TODO: Other material properties (alpha, double sided etc)
    // TODO: Other material types
    return invalidAssetId;
}

void LoadedGltf::setMaterialDescriptor(Material& material)
{
    // allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = engine->globalDescriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &engine->materialLayout
    };

    VK_Check(vkAllocateDescriptorSets(engine->device->device, &allocInfo, &material.descriptorSet));

    Texture& baseColorTex = textures[material.baseColorTex];
    Texture& metalRoughTex = textures[material.metalRoughTex];

    // write to descriptor set
    VkDescriptorImageInfo baseColorInfo{
        .sampler = samplers[baseColorTex.sampler],
        .imageView = baseColorTex.view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkDescriptorImageInfo metalRoughInfo{
        .sampler = samplers[metalRoughTex.sampler],
        .imageView = metalRoughTex.view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    std::array<VkWriteDescriptorSet, 2> writeSets;

    writeSets[0] = VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = material.descriptorSet,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &baseColorInfo,
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr
    };

    writeSets[1] = VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = material.descriptorSet,
        .dstBinding = 1,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &metalRoughInfo,
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr
    };

    vkUpdateDescriptorSets(
        engine->device->device,
        static_cast<uint32_t>(writeSets.size()), writeSets.data(),
        0, nullptr
    );
}

Material Material::initMaterial()
{
    // initializes material properties with proper fallbacks
    Material newMaterial = {};
    newMaterial.constants.baseColorFactor = glm::vec4{ 1.f, 1.f, 1.f, 1.f };
    newMaterial.constants.metalnessFactor = 1.f;
    newMaterial.constants.roughnessFactor = 1.f;

    newMaterial.baseColorTex = defaultHandle;
    newMaterial.metalRoughTex = defaultHandle;

    return newMaterial;
}

void LoadedGltf::loadMaterials(std::span<cgltf_material> gltfMaterials)
{
    materials.reserve(gltfMaterials.size());

    for (const cgltf_material& material : gltfMaterials)
    {
        Material newMaterial = Material::initMaterial();
        if (material.has_pbr_metallic_roughness)
        {
            // load PBR constants
            newMaterial.constants.baseColorFactor = glm::make_vec4(
                reinterpret_cast<const float*>(material.pbr_metallic_roughness.base_color_factor)
            );
            newMaterial.constants.metalnessFactor = material.pbr_metallic_roughness.metallic_factor;
            newMaterial.constants.roughnessFactor = material.pbr_metallic_roughness.roughness_factor;

            // get textures
            if (material.pbr_metallic_roughness.base_color_texture.texture)
            {
                cgltf_texture& baseColorTex = *material.pbr_metallic_roughness.base_color_texture.texture;
                newMaterial.baseColorTex = textureMap[getTextureId(baseColorTex)];
            }

            if (material.pbr_metallic_roughness.metallic_roughness_texture.texture)
            {
                cgltf_texture& metalRoughTex = 
                    *material.pbr_metallic_roughness.metallic_roughness_texture.texture;
                newMaterial.metalRoughTex = textureMap[getTextureId(metalRoughTex)];
            }
        }

        // TODO: Other material properties (alpha, double sided etc)
        // TODO: Other material types

        // create descriptor set
        setMaterialDescriptor(newMaterial);

        MaterialHandle handle = materials.size();
        AssetId id = getMaterialId(material);
        materialMap[id] = handle;

        materials.push_back(newMaterial);
    }
}

static AssetId getBufferId(const void* data, size_t dataSize)
{
    return util::fastHash(data, dataSize);
}

LoadedGltf::BufferDesc LoadedGltf::loadIndexBuffer(cgltf_accessor& accessor)
{
    std::vector<uint8_t> indexData;

    cgltf_size outComponentSize = cgltf_component_size(accessor.component_type);
    cgltf_size indexCount = cgltf_accessor_unpack_indices(&accessor, nullptr, 0, 0);
    indexData.resize(outComponentSize * indexCount);

    // base vulkan only supports 16u and 32u indices
    VkIndexType indexType = VK_INDEX_TYPE_UINT16;
    switch (accessor.component_type)
    {
    case cgltf_component_type_r_16u:
        indexType = VK_INDEX_TYPE_UINT16;
        break;
    case cgltf_component_type_r_32u:
        indexType = VK_INDEX_TYPE_UINT32;
        break;
    default:
        SDL_LogError(0, "Mesh load error: Mesh primitive contains invalid index format");
        break;
    }

    cgltf_accessor_unpack_indices(&accessor, indexData.data(), outComponentSize, indexCount);

    AssetId indexBufferId = getBufferId(indexData.data(), indexData.size() * sizeof(indexData[0]));

    auto entry = bufferMap.find(indexBufferId);
    if (entry != bufferMap.end())
    {
        // index buffer already exists; return it
        return { entry->second, indexCount, outComponentSize };
    }

    // else, create the buffer
    VkDeviceSize dataSize = static_cast<VkDeviceSize>(outComponentSize * indexCount);

    // create and upload gpu buffer
    gfx::AllocatedBuffer stagingBuffer = gfx::createAllocatedBuffer(
        engine->device.get(), dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
    );

    gfx::writeToAllocatedBuffer(engine->device.get(), indexData.data(), dataSize, stagingBuffer);

    gfx::AllocatedBuffer newBuffer = gfx::createAllocatedBuffer(
        engine->device.get(), dataSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 0
    );

    // copy staging to gpu only buffer
    VkCommandBuffer cmd = engine->startImmediateCommands();
    copyBufferToBuffer(cmd, stagingBuffer, newBuffer, dataSize);
    engine->endAndSubmitImmediateCommands();

    gfx::destroyAllocatedBuffer(engine->device.get(), stagingBuffer);

    BufferHandle handle = buffers.size();
    bufferMap[indexBufferId] = handle;
    buffers.push_back(newBuffer);

    return { handle, indexCount };
}

LoadedGltf::BufferDesc LoadedGltf::loadVertexBuffer(const cgltf_primitive& primitive)
{
    std::vector<Vertex> vertexData;

    std::vector<float> positionData;
    std::vector<float> normalData;
    std::vector<float> uvData;
    std::vector<float> colorData;

    // position
    {
        const cgltf_accessor* positionAcc = cgltf_find_accessor(
            &primitive, cgltf_attribute_type_position, 0
        );

        if (positionAcc == nullptr)
        {
            SDL_LogError(0, "Mesh load error: Surface position attrib invalid");
            return { invalidAssetId, 0 };
        }

        cgltf_size positionCount = cgltf_accessor_unpack_floats(positionAcc, nullptr, 0);
        positionData.resize(positionCount);
        cgltf_accessor_unpack_floats(positionAcc, positionData.data(), positionCount);
    }

    // normal
    bool hasNormal = false;
    {
        const cgltf_accessor* normalAcc = cgltf_find_accessor(
            &primitive, cgltf_attribute_type_normal, 0
        );

        if (normalAcc != nullptr)
        {
            hasNormal = true;
            cgltf_size normalCount = cgltf_accessor_unpack_floats(normalAcc, nullptr, 0);
            normalData.resize(normalCount);
            cgltf_accessor_unpack_floats(normalAcc, normalData.data(), normalCount);
        }
    }

    // uv
    bool hasUv = false;
    {
        const cgltf_accessor* uvAcc = cgltf_find_accessor(
            &primitive, cgltf_attribute_type_texcoord, 0
        );

        if (uvAcc != nullptr)
        {
            hasUv = true;
            cgltf_size uvCount = cgltf_accessor_unpack_floats(uvAcc, nullptr, 0);
            uvData.resize(uvCount);
            cgltf_accessor_unpack_floats(uvAcc, uvData.data(), uvCount);
        }
    }

    // color
    cgltf_size colorChannels = 0;
    {
        const cgltf_accessor* colorAcc = cgltf_find_accessor(
            &primitive, cgltf_attribute_type_color, 0
        );

        if (colorAcc != nullptr)
        {
            colorChannels = cgltf_num_components(colorAcc->type);
            cgltf_size colorCount = cgltf_accessor_unpack_floats(colorAcc, nullptr, 0);
            colorData.resize(colorCount);
            cgltf_accessor_unpack_floats(colorAcc, colorData.data(), colorCount);
        }
    }

    // assemble attribs
    size_t vertexCount = positionData.size() / 3;
    {
        vertexData.resize(vertexCount);
        for (size_t i = 0; i < vertexCount; i++)
        {
            vertexData[i].position = glm::make_vec3(&positionData[3 * i]);

            if (hasNormal)
                vertexData[i].normal = glm::make_vec3(&normalData[3 * i]);
            else
                vertexData[i].normal = glm::vec3{ 0.f, 0.f, 1.f };

            if (hasUv)
                vertexData[i].uv = glm::make_vec2(&uvData[2 * i]);
            else
                vertexData[i].uv = glm::vec2{ 0.f, 0.f };

            if (colorChannels == 4)
                vertexData[i].color = glm::make_vec4(&colorData[4 * i]);
            else if (colorChannels == 3)
                vertexData[i].color = glm::vec4{ glm::make_vec3(&colorData[3 * i]), 1.f };
            else
                vertexData[i].color = glm::vec4{ 1.f, 1.f, 1.f, 1.f }; // colors aren't specified with less than 3 channels
        }
    }

    // check asset store
    AssetId vertexBufferId = getBufferId(vertexData.data(), vertexData.size() * sizeof(vertexData[0]));

    auto entry = bufferMap.find(vertexBufferId);
    if (entry != bufferMap.end())
    {
        // vertex buffer already exists; return it
        return { entry->second, vertexCount };
    }

    // else, create and upload the buffer
    VkDeviceSize dataSize = static_cast<VkDeviceSize>(sizeof(vertexData[0]) * vertexCount);

    // create and upload gpu buffer
    gfx::AllocatedBuffer stagingBuffer = gfx::createAllocatedBuffer(
        engine->device.get(), dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
    );

    gfx::writeToAllocatedBuffer(engine->device.get(), vertexData.data(), dataSize, stagingBuffer);

    gfx::AllocatedBuffer newBuffer = gfx::createAllocatedBuffer(
        engine->device.get(), dataSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 0
    );

    // copy staging to gpu only buffer
    VkCommandBuffer cmd = engine->startImmediateCommands();
    copyBufferToBuffer(cmd, stagingBuffer, newBuffer, dataSize);
    engine->endAndSubmitImmediateCommands();

    gfx::destroyAllocatedBuffer(engine->device.get(), stagingBuffer);

    // store buffer
    BufferHandle handle = buffers.size();
    bufferMap[vertexBufferId] = handle;
    buffers.push_back(newBuffer);

    return { handle, vertexCount };
}

uint64_t MeshPrimitive::getHash() const
{
    assert(sizeof(MeshPrimitive) == 40); // if size changes, check hash
    return util::fastHash(this, sizeof(MeshPrimitive));
}

MeshPrimitive LoadedGltf::createMeshPrimitive(const cgltf_primitive& primitive)
{
    MeshPrimitive newPrimitive;

    // TODO: Implement rendering all topology types
    switch (primitive.type)
    {
    case cgltf_primitive_type_triangles:
        newPrimitive.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        break;
    default:
        SDL_LogError(0, "Mesh load error: Mesh primitive contains invalid topology");
        std::abort();
        return MeshPrimitive{};
    }

    // get index buffer
    newPrimitive.indexBuffer = defaultHandle;
    if (primitive.indices)
    {
        // TODO: Support non index geometrys
        BufferDesc indexDesc = loadIndexBuffer(*primitive.indices);
        newPrimitive.indexBuffer = indexDesc.handle;
        newPrimitive.indexCount = static_cast<uint32_t>(indexDesc.numElements);
        newPrimitive.indexType = indexDesc.elementSize == 4 ?
            VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
        }
    else
    {
        SDL_LogError(0, "Mesh load error: Mesh primitive is non indexed");
        std::abort();
    }

    // get vertex buffer
    {
        BufferDesc vertexDesc = loadVertexBuffer(primitive);
        newPrimitive.vertexBuffer = vertexDesc.handle;
        newPrimitive.vertexCount = static_cast<uint32_t>(vertexDesc.numElements);
        }

    // get material
    newPrimitive.material = defaultHandle;
    if (primitive.material)
    {
        newPrimitive.material = materialMap[getMaterialId(*primitive.material)];
    }

    return newPrimitive;
}

static AssetId getMeshId(std::span<MeshPrimitive> primitives)
{
    AssetId meshId = 0;
    for (const auto& primitive : primitives)
        util::hashCombine(meshId, primitive.getHash());

    return meshId;
}

// Index and Vertex Buffers are loaded on demand (not ahead like images)
// Meaning they aren't loaded until the first primitive that needs it is processed
void LoadedGltf::loadMeshes(std::span<cgltf_mesh> gltfMeshes)
{
    for (const cgltf_mesh& mesh : gltfMeshes)
    {
        Mesh newMesh;
        newMesh.primitives.reserve(mesh.primitives_count);
        AssetId meshId = 0;

        for (cgltf_size i = 0; i < mesh.primitives_count; i++)
        {
            cgltf_primitive& primitive = mesh.primitives[i];
            MeshPrimitive newPrimitive = createMeshPrimitive(primitive);

            // store primitive
            newMesh.primitives.push_back(newPrimitive);
            util::hashCombine(meshId, newPrimitive.getHash());
        }

        // store mesh
        MeshHandle handle = meshes.size();
        meshMap[meshId] = handle;
        meshes.push_back(newMesh);
    }
}

static glm::mat4 getNodeMatrix(const cgltf_node& node, const glm::mat4& parentTransform)
{
    if (node.has_matrix)
        return parentTransform * glm::make_mat4(node.matrix);

    glm::mat4 transform = glm::identity<glm::mat4>();
    if (node.has_scale)
        transform = glm::scale(transform, glm::make_vec3(node.scale));

    if (node.has_rotation)
    {
        glm::quat rotation = glm::make_quat(node.rotation);
        transform *= glm::toMat4(rotation);
    }

    if (node.has_translation)
        transform = glm::translate(transform, glm::make_vec3(node.translation));

    return parentTransform * transform;
}

MeshNode LoadedGltf::createMeshNode(const cgltf_mesh& mesh, const glm::mat4& transform)
{
    MeshNode newMeshNode{ .mesh = defaultHandle, .transform = transform };

    std::vector<MeshPrimitive> primitives(mesh.primitives_count);
    for (cgltf_size i = 0; i < mesh.primitives_count; i++)
        primitives[i] = createMeshPrimitive(mesh.primitives[i]);

    AssetId meshId = getMeshId(primitives);
    newMeshNode.mesh = meshMap[meshId];

    return newMeshNode;
}

void LoadedGltf::loadScene(const cgltf_scene& gltfScene)
{
    Scene& newScene = scene;
    std::vector<cgltf_node*> nodeStack;
    std::vector<glm::mat4> transformStack;
    for (cgltf_size i = 0; i < gltfScene.nodes_count; i++)
    {
        nodeStack.push_back(gltfScene.nodes[i]);
        transformStack.push_back(glm::identity<glm::mat4>());
        while (!nodeStack.empty())
        {
            cgltf_node* node = nodeStack.back();
            nodeStack.pop_back();

            glm::mat4 parentTransform = transformStack.back();
            transformStack.pop_back();

            glm::mat4 transform = getNodeMatrix(*node, parentTransform);
            if (node->mesh)
            {
                newScene.nodes.push_back(createMeshNode(*node->mesh, transform));
            }

            // add children to stack to recurse
            for (cgltf_size child = 0; child < node->children_count; child++)
            {
                nodeStack.push_back(node->children[child]);
                transformStack.push_back(transform); // a copy of the transform for each child
            }
        }
    }
}

struct ScopedGLTFData
{
    cgltf_data* data;

    cgltf_data* operator->() const { return data; }

    ~ScopedGLTFData()
    {
        cgltf_free(data);
    }
};

LoadedGltf::LoadedGltf(gfx::RenderEngine* renderEngine, std::string_view gltfPath)
    : engine(renderEngine), path(gltfPath)
{
    cgltf_options options{}; // default loading options
    ScopedGLTFData gltfData;

    cgltf_result result = cgltf_parse_file(&options, gltfPath.data(), &gltfData.data);
    if (result != cgltf_result_success || gltfData.data == nullptr)
    {
        SDL_LogError(0, "GLTF load error: Could not read file data: %s\n", gltfPath.data());
        SDL_LogError(0, "GLTF load error code: %i\n", result);
        std::abort();
    }

    result = cgltf_load_buffers(&options, gltfData.data, gltfPath.data());
    if (result != cgltf_result_success)
    {
        SDL_LogError(0, "GLTF load error: Could not read buffer data: %s\n", gltfPath.data());
        SDL_LogError(0, "GLTF load error code: %i\n", result);
        std::abort();
    }

    initDefaultAssets();

    loadImages({ gltfData->images, gltfData->images_count });
    loadSamplers({ gltfData->samplers, gltfData->samplers_count });
    loadTextures({ gltfData->textures, gltfData->textures_count });
    loadMaterials({ gltfData->materials, gltfData->materials_count });
    loadMeshes({ gltfData->meshes, gltfData->meshes_count });

    if (gltfData->scene)
        loadScene(*gltfData->scene);
}

void Texture::cleanup(gfx::Device* device)
{
    vkDestroyImageView(device->device, view, nullptr);
}

void LoadedGltf::cleanup()
{
    {
        // buffers
        for (auto& buffer : buffers)
            gfx::destroyAllocatedBuffer(engine->device.get(), buffer);

        buffers.clear();
        bufferMap.clear();
    }

    {
        // images
        for (auto& image : images)
            gfx::destroyAllocatedImage(engine->device.get(), image);

        images.clear();
        imageMap.clear();
    }

    {
        // samplers
        for (auto& sampler : samplers)
            vkDestroySampler(engine->device->device, sampler, nullptr);

        samplers.clear();
        samplerMap.clear();
    }

    {
        // textures
        for (auto& texture : textures)
            texture.cleanup(engine->device.get());

        textures.clear();
        textureMap.clear();
    }

    {
        // materials
        materials.clear();
        materialMap.clear();
    }

    {
        // meshes
        meshes.clear();
        meshMap.clear();
    }

    scene.nodes.clear();
}