#pragma once

// sge
#include "Resource.h"
#include "RenderEngine.h"

// stl
#include <optional>
#include <vector>
#include <string_view>
#include <unordered_map>

using AssetId = uint64_t;
static const AssetId invalidAssetId = UINT64_MAX;

using ImageHandle = uint64_t;
using SamplerHandle = uint64_t;
using TextureHandle = uint64_t;

struct Texture
{
    ImageHandle image;
    SamplerHandle sampler;
    VkImageView view;

    void cleanup(gfx::Device* device);
};

/*
* A GLTF file loaded into CPU memory
*/
class LoadedGltf
{
public:
    LoadedGltf(gfx::RenderEngine* engine, std::string_view gltfPath);

    std::vector<gfx::AllocatedImage> images;
    std::unordered_map<AssetId, ImageHandle> imageMap;

    std::vector<VkSampler> samplers;
    std::unordered_map<AssetId, SamplerHandle> samplerMap;

    std::vector<Texture> textures;
    std::unordered_map<AssetId, TextureHandle> textureMap;

private:
    void loadImages(gfx::RenderEngine* engine, std::span<cgltf_image> gltfImages);
    void loadSamplers(gfx::RenderEngine* engine, std::span<cgltf_sampler> gltfSamplers);
    void loadTextures(gfx::RenderEngine* engine, std::span<cgltf_texture> gltfTextures);
};