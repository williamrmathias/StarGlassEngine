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

// stl
#include <span>

struct ScopedGLTFData
{
    cgltf_data* data;

    cgltf_data* operator->() const { return data; }

    ~ScopedGLTFData()
    {
        cgltf_free(data);
    }
};

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

// all images are loaded in format r8g8b8a8 unorm
// they're all uploaded to the GPU at the return of this function
void LoadedGltf::loadImages(
    gfx::RenderEngine* engine, std::span<cgltf_image> gltfImages
)
{
    images.reserve(gltfImages.size());

    for (const cgltf_image& image : gltfImages)
    {
        stbi_uc* imageData = nullptr;
        VkDeviceSize imageDataSize = 0;
        int width, height, nChannels;

        if (image.uri)
        {
            imageData = stbi_load(image.uri, &width, &height, &nChannels, STBI_rgb_alpha);
        }
        else if (image.buffer_view)
        {
            cgltf_buffer_view* bufferView = image.buffer_view;
            cgltf_buffer* buffer = bufferView->buffer;
            const uint8_t* data = static_cast<uint8_t*>(buffer->data) + bufferView->offset;

            imageData = stbi_load_from_memory(
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

        gfx::writeToAllocatedBuffer(device, imageData, imageDataSize, stagingBuffer);

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

    return util::fastHash(data, 4 * sizeof(uint32_t));
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
}