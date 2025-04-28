// sge
#include "GLTFLoader.h"
#include "Log.h"
#include "Commands.h"
#include "Utils.h"

// cgltf
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

// stb
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

// glm
#include <glm/gtc/type_ptr.hpp>

// stl
#include <span>
#include <utility>

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
void LoadedGltf::loadImages(
    gfx::RenderEngine* engine, std::span<cgltf_image> gltfImages
)
{
    images.reserve(gltfImages.size());

    for (const cgltf_image& image : gltfImages)
    {
        ScopedSTBImage imageData{ nullptr };
        VkDeviceSize imageDataSize = 0;
        int width, height, nChannels;

        if (image.uri)
        {
            imageData.data = stbi_load(image.uri, &width, &height, &nChannels, STBI_rgb_alpha);
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
            // TODO: Add proper image fallback
            SDL_LogError(0, "GLTF load error: input image has now buffer view: %s", image.name);
            std::abort();
        }

        imageDataSize = static_cast<VkDeviceSize>(width * height * STBI_rgb_alpha);
        gfx::Device* device = engine->device.get();

        gfx::AllocatedBuffer stagingBuffer = gfx::createAllocatedBuffer(
            device, imageDataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
        );

        gfx::writeToAllocatedBuffer(device, imageData.data, imageDataSize, stagingBuffer);

        gfx::AllocatedImage newImage = gfx::createAllocatedImage(
            device, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            VK_FORMAT_R8G8B8A8_UNORM, VkExtent2D{ static_cast<uint32_t>(width), static_cast<uint32_t>(height) }
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

        ImageHandle handle = images.size();
        AssetId id = getImageId(image);
        imageMap[id] = handle;

        images.push_back(newImage);
    }
}

static void extractSamplerFiltersAndWrap(
    const cgltf_sampler& sampler,
    VkFilter& magFilter, VkFilter& minFilter,
    VkSamplerAddressMode& uWrap, VkSamplerAddressMode& vWrap
)
{
    switch (sampler.mag_filter)
    {
    case cgltf_filter_type_nearest:
        magFilter = VK_FILTER_NEAREST;
        break;
    case cgltf_filter_type_linear:
    default:
        magFilter = VK_FILTER_LINEAR;
        break;
    }

    switch (sampler.min_filter)
    {
    case cgltf_filter_type_nearest:
    case cgltf_filter_type_nearest_mipmap_nearest:
    case cgltf_filter_type_nearest_mipmap_linear:
        minFilter = VK_FILTER_NEAREST;
        break;
    case cgltf_filter_type_linear:
    case cgltf_filter_type_linear_mipmap_nearest:
    case cgltf_filter_type_linear_mipmap_linear:
    default:
        minFilter = VK_FILTER_LINEAR;
        break;
    }

    switch (sampler.wrap_s)
    {
    case cgltf_wrap_mode_clamp_to_edge:
        uWrap = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        break;
    case cgltf_wrap_mode_mirrored_repeat:
        uWrap = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        break;
    case cgltf_wrap_mode_repeat:
        uWrap = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        break;
    }

    switch (sampler.wrap_t)
    {
    case cgltf_wrap_mode_clamp_to_edge:
        vWrap = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        break;
    case cgltf_wrap_mode_mirrored_repeat:
        vWrap = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        break;
    case cgltf_wrap_mode_repeat:
        vWrap = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        break;
    }
}

static AssetId getSamplerId(const cgltf_sampler& sampler)
{
    VkFilter magFilter, minFilter;
    VkSamplerAddressMode uWrap, vWrap;

    extractSamplerFiltersAndWrap(sampler, magFilter, minFilter, uWrap, vWrap);

    const uint32_t data[4] = { 
        static_cast<uint32_t>(magFilter), static_cast<uint32_t>(minFilter),
        static_cast<uint32_t>(uWrap) , static_cast<uint32_t>(vWrap) 
    };

    return util::fastHash(data, 4 * sizeof(data[0]));
}

void LoadedGltf::loadSamplers(
    gfx::RenderEngine* engine, std::span<cgltf_sampler> gltfSamplers
)
{
    samplers.reserve(gltfSamplers.size());

    for (const cgltf_sampler& sampler : gltfSamplers)
    {
        VkFilter magFilter, minFilter;
        VkSamplerAddressMode uWrap, vWrap;

        extractSamplerFiltersAndWrap(sampler, magFilter, minFilter, uWrap, vWrap);

        VkSampler newSampler = gfx::createSampler(
            engine->device.get(), magFilter, minFilter, uWrap, vWrap
        );

        SamplerHandle handle = samplers.size();
        AssetId id = getSamplerId(sampler);
        samplerMap[id] = handle;

        samplers.push_back(newSampler);
    }
}

static AssetId getTextureId(const cgltf_texture& texture)
{
    AssetId imageId = 0;
    AssetId samplerId = 0;
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

void LoadedGltf::loadTextures(
    gfx::RenderEngine* engine, std::span<cgltf_texture> gltfTextures
)
{
    textures.reserve(textures.size());

    for (const cgltf_texture& texture : gltfTextures)
    {
        Texture newTexture;
        if (texture.image)
        {
            newTexture.image = imageMap[getImageId(*texture.image)];
        }
        if (texture.sampler)
        {
            newTexture.sampler = samplerMap[getSamplerId(*texture.sampler)];
        }

        // TODO: Fallbacks

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
        const uint32_t constantData[6] = {
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
        else
        {
            // TODO: Base Color Fallback
        }

        if (material.pbr_metallic_roughness.metallic_roughness_texture.texture)
        {
            cgltf_texture& metalRoughTex =
                *material.pbr_metallic_roughness.metallic_roughness_texture.texture;
            metalRoughId = getTextureId(metalRoughTex);
        }
        else
        {
            // TODO: Metallic-Roughness Fallback
        }

        AssetId materialId = constantHash;
        util::hashCombine(materialId, baseColorId);
        util::hashCombine(materialId, metalRoughId);
        return materialId;
    }

    // TODO: PBR Material fallback?
    // TODO: Other material properties (alpha, double sided etc)
    // TODO: Other material types
    return invalidAssetId;
}

void LoadedGltf::setMaterialDescriptor(gfx::RenderEngine* engine, Material& material)
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

void LoadedGltf::loadMaterials(gfx::RenderEngine* engine, std::span<cgltf_material> gltfMaterials)
{
    materials.reserve(materials.size());

    for (const cgltf_material& material : gltfMaterials)
    {
        Material newMaterial;
        if (material.has_pbr_metallic_roughness)
        {
            // load PBR constants
            newMaterial.constants.baseColorFactor = glm::make_vec4(
                &material.pbr_metallic_roughness.base_color_factor
            );
            newMaterial.constants.metalnessFactor = material.pbr_metallic_roughness.metallic_factor;
            newMaterial.constants.roughnessFactor = material.pbr_metallic_roughness.roughness_factor;

            // get textures
            if (material.pbr_metallic_roughness.base_color_texture.texture)
            {
                cgltf_texture& baseColorTex = *material.pbr_metallic_roughness.base_color_texture.texture;
                newMaterial.baseColorTex = textureMap[getTextureId(baseColorTex)];
            }
            else
            {
                // TODO: Base Color Fallback
            }

            if (material.pbr_metallic_roughness.metallic_roughness_texture.texture)
            {
                cgltf_texture& metalRoughTex = 
                    *material.pbr_metallic_roughness.metallic_roughness_texture.texture;
                newMaterial.metalRoughTex = textureMap[getTextureId(metalRoughTex)];
            }
            else
            {
                // TODO: Metallic-Roughness Fallback
            }
        }

        // TODO: PBR Material fallback?
        // TODO: Other material properties (alpha, double sided etc)
        // TODO: Other material types

        // create descriptor set
        setMaterialDescriptor(engine, newMaterial);

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

LoadedGltf::BufferDesc LoadedGltf::loadIndices(
    gfx::RenderEngine* engine, cgltf_accessor& accessor
)
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

LoadedGltf::BufferDesc LoadedGltf::loadVertices(
    gfx::RenderEngine* engine, cgltf_primitive& primitive
)
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
        // index buffer already exists; return it
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

static AssetId getPrimitiveId(MeshPrimitive& primitive) 
{
    // you basically have to reconstruct a primitive to be able to get it's hash
    // seems slow, but still allows deduplication
    AssetId primitiveId = primitive.vertexBuffer;
    util::hashCombine(primitiveId, primitive.indexBuffer);
    util::hashCombine(primitiveId, primitive.material);
}

// Index and Vertex Buffers are loaded on demand (not ahead like images)
// Meaning they aren't loaded until the first primitive that needs it is processed
void LoadedGltf::loadMeshes(gfx::RenderEngine* engine, std::span<cgltf_mesh> gltfMeshes)
{
    for (const cgltf_mesh& mesh : gltfMeshes)
    {
        for (cgltf_size i = 0; i < mesh.primitives_count; i++)
        {
            cgltf_primitive& primitive = mesh.primitives[i];

            MeshPrimitive newPrimitive;

            // TODO: Implement rendering all topology types
            switch (primitive.type)
            {
            case cgltf_primitive_type_triangles:
                newPrimitive.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
                break;
            default:
                SDL_LogError(0, "Mesh load error: Mesh primitive contains invalid topology");
                return;
            }

            // get index buffer
            newPrimitive.indexBuffer = invalidAssetId;
            if (primitive.indices)
            {
                // TODO: Support non index geometrys
                BufferDesc indexDesc = loadIndices(engine, *primitive.indices);
                newPrimitive.indexBuffer = indexDesc.handle;
                newPrimitive.indexCount = indexDesc.numElements;
                newPrimitive.indexType = indexDesc.elementSize == 4 ? 
                    VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
            }

            // get vertex buffer
            {
                BufferDesc vertexDesc = loadVertices(engine, primitive);
                newPrimitive.vertexBuffer = vertexDesc.handle;
                newPrimitive.vertexCount = vertexDesc.numElements;
            }

            // get material
            newPrimitive.material = invalidAssetId;
            if (primitive.material)
            {
                newPrimitive.material = materialMap[getMaterialId(*primitive.material)];
            }
            // TODO: Default material
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

LoadedGltf::LoadedGltf(gfx::RenderEngine* engine, std::string_view gltfPath)
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

    loadImages(engine, { gltfData->images, gltfData->images_count });
    loadSamplers(engine, { gltfData->samplers, gltfData->samplers_count });
    loadTextures(engine, { gltfData->textures, gltfData->textures_count });
    loadMaterials(engine, { gltfData->materials, gltfData->materials_count });
}