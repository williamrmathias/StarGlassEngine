#pragma once

// sge
#include "Resource.h"

// stl
#include <optional>
#include <vector>
#include <string_view>
#include <unordered_map>
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <filesystem>

// cgltf
#include <cgltf.h>

// glm
#include <glm/glm.hpp>

// vulkan
#include <vulkan/vulkan_core.h>

namespace gfx
{
class RenderEngine;
}

using AssetId = uint64_t;
static const AssetId invalidAssetId = UINT64_MAX;

using BufferHandle = uint64_t;
using ImageHandle = uint64_t;
using SamplerHandle = uint64_t;
using TextureHandle = uint64_t;
using MaterialHandle = uint64_t;
using MeshHandle = uint64_t;

static const uint64_t defaultHandle = 0;
static const uint64_t errorHandle = 1;

struct Texture
{
    ImageHandle image;
    SamplerHandle sampler;
    VkImageView view;

    void cleanup(gfx::Device* device);
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
    TextureHandle normalTex;

    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

    static Material initMaterial();
};

struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec4 tangent;
    glm::vec2 uv;
    glm::vec4 color;

    static VkVertexInputBindingDescription getInputBindingDescription();
    static std::array<VkVertexInputAttributeDescription, 5> getInputAttributeDescription();
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

    uint64_t getHash() const;
};

struct Mesh
{
    std::vector<MeshPrimitive> primitives;
};

struct MeshNode
{
    MeshHandle mesh;
    glm::mat4 transform;
};

struct Scene
{
    std::vector<MeshNode> nodes;
};

/*
* A GLTF file loaded into CPU memory
*/
class LoadedGltf
{
public:
    LoadedGltf(gfx::RenderEngine* renderEngine, std::string_view gltfPath);

    void cleanup();

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

    std::vector<Mesh> meshes;
    std::unordered_map<AssetId, MeshHandle> meshMap;

    Scene scene;

private:
    gfx::RenderEngine* engine;
    std::filesystem::path path;

    void initDefaultAssets();

    void loadImages(std::span<cgltf_image> gltfImages);
    void loadSamplers(std::span<cgltf_sampler> gltfSamplers);
    void loadTextures(std::span<cgltf_texture> gltfTextures);

    void setMaterialDescriptor(Material& material);
    void loadMaterials(std::span<cgltf_material> gltfMaterials);

    struct BufferDesc
    {
        BufferHandle handle;
        size_t numElements;
        size_t elementSize;
    };

    BufferDesc loadIndexBuffer(cgltf_accessor& accessor);
    BufferDesc loadVertexBuffer(const cgltf_primitive& primitive);
    MeshPrimitive createMeshPrimitive(const cgltf_primitive& primitive);
    void loadMeshes(std::span<cgltf_mesh> gltfMeshes);

    MeshNode createMeshNode(const cgltf_mesh& mesh, const glm::mat4& transform);
    void loadScene(const cgltf_scene& gltfScene);
};