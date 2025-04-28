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

using BufferHandle = uint64_t;
using ImageHandle = uint64_t;
using SamplerHandle = uint64_t;
using TextureHandle = uint64_t;
using MaterialHandle = uint64_t;
using PrimitiveHandle = uint64_t;

struct Texture
{
    ImageHandle image;
    SamplerHandle sampler;
    VkImageView view;

    //void cleanup(gfx::Device* device);
};

struct MaterialConstants
{
    glm::vec4 baseColorFactor;
    float metalnessFactor;
    float roughnessFactor;
};

struct Material
{
    MaterialConstants constants;

    TextureHandle baseColorTex;
    TextureHandle metalRoughTex;

    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

    //void cleanup(gfx::Device* device);
};

struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec4 color;

    static VkVertexInputBindingDescription getInputBindingDescription();
    static std::array<VkVertexInputAttributeDescription, 4> getInputAttributeDescription();
};

struct MeshPrimitive
{
    BufferHandle vertexBuffer;
    BufferHandle indexBuffer;

    uint32_t vertexCount;
    uint32_t indexCount;

    VkPrimitiveTopology topology;
    VkIndexType indexType;

    MaterialHandle material;
};

/*
* A GLTF file loaded into CPU memory
*/
class LoadedGltf
{
public:
    LoadedGltf(gfx::RenderEngine* engine, std::string_view gltfPath);

    std::vector<gfx::AllocatedBuffer> buffers;
    std::unordered_map<AssetId, BufferHandle> bufferMap;

    std::vector<gfx::AllocatedImage> images;
    std::unordered_map<AssetId, ImageHandle> imageMap;

    std::vector<VkSampler> samplers;
    std::unordered_map<AssetId, SamplerHandle> samplerMap;

    std::vector<Texture> textures;
    std::unordered_map<AssetId, TextureHandle> textureMap;

    std::vector<Material> materials;
    std::unordered_map<AssetId, MaterialHandle> materialMap;

    std::vector<MeshPrimitive> primitives;
    std::unordered_map<AssetId, PrimitiveHandle> primitiveMap;

private:
    void loadImages(gfx::RenderEngine* engine, std::span<cgltf_image> gltfImages);
    void loadSamplers(gfx::RenderEngine* engine, std::span<cgltf_sampler> gltfSamplers);
    void loadTextures(gfx::RenderEngine* engine, std::span<cgltf_texture> gltfTextures);

    void setMaterialDescriptor(gfx::RenderEngine* engine, Material& material);
    void loadMaterials(gfx::RenderEngine* engine, std::span<cgltf_material> gltfMaterials);

    struct BufferDesc
    {
        BufferHandle handle;
        size_t numElements;
        size_t elementSize;
    };

    BufferDesc loadIndices(gfx::RenderEngine* engine, cgltf_accessor& accessor);
    BufferDesc loadVertices(gfx::RenderEngine* engine, cgltf_primitive& primitive);
    void loadMeshes(gfx::RenderEngine* engine, std::span<cgltf_mesh> gltfMeshes);
};