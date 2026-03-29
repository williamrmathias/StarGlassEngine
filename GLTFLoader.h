#pragma once

// sge
#include "Resource.h"
#include "ThreadPool.h"

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
#include <limits>

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

constexpr uint64_t kDefaultHandle = 0;
constexpr uint64_t kErrorHandle = 1;
constexpr uint64_t kDefaultNormalMapHandle = 1;
constexpr uint64_t kInvalidHandle = UINT64_MAX; // do not assign a material to this value

constexpr uint32_t kCubeMapDimension = 1024;
constexpr uint32_t kIrradianceMapDimension = 64;
constexpr uint32_t kPrefilteredEnvMapBaseDimension = 256;
constexpr uint32_t kBrdfLutDimension = 512;

constexpr VkFormat kSkyboxFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
constexpr VkFormat kIrradianceFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
constexpr VkFormat kPrefilteredEnvFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
constexpr VkFormat kBrdfLutFormat = VK_FORMAT_R16G16_SFLOAT;

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
    float alphaCutoff;
};

// indicates what draw pipeline to use
enum MaterialFlags
{
    MaterialFlag_Opaque,
    MaterialFlag_AlphaMask,
    MaterialFlag_AlphaBlend
};

struct Material
{
    MaterialConstants constants;
    MaterialFlags flags;

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
    glm::vec2 uv;
    glm::vec4 color;

    static VkVertexInputBindingDescription getInputBindingDescription();
    static std::array<VkVertexInputAttributeDescription, 4> getInputAttributeDescription();
};

namespace Vec3
{
constexpr glm::vec3 infinity = glm::vec3{ 
    std::numeric_limits<float>::infinity(),
    std::numeric_limits<float>::infinity(),
    std::numeric_limits<float>::infinity() 
};

constexpr glm::vec3 minusInfinity = glm::vec3{
    -std::numeric_limits<float>::infinity(),
    -std::numeric_limits<float>::infinity(),
    -std::numeric_limits<float>::infinity()
};
}

struct Extent
{
    glm::vec3 max = Vec3::minusInfinity;
    glm::vec3 min = Vec3::infinity;

    std::array<glm::vec3, 8> getCorners() const;
    glm::vec3 getCenter() const;

    void expandToContain(const Extent& extent);
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
    MaterialFlags flags;

    Extent boundingBox;

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

struct CubeMap
{
    gfx::AllocatedImage image;
    VkImageView view;
    VkDescriptorSet descriptorSet;

    void cleanup(gfx::Device* device);
};

struct TextureLUT
{
    gfx::AllocatedImage image;
    VkImageView view;
    VkDescriptorSet descriptorSet;

    void cleanup(gfx::Device* device);
};

struct Scene
{
    std::vector<MeshNode> nodes;
    CubeMap skybox;
    CubeMap irradianceMap;
    CubeMap prefilteredEnvMap;
    TextureLUT brdfLUT;
};

struct LoadingStats
{
    uint64_t loadGLTFTimeMS = 0;
    uint64_t loadHDRSkyboxTimeMS = 0;

    uint64_t imageLoadTime = 0;
    uint64_t imageLoadCount = 0;

    uint64_t meshLoadTime = 0;
    uint64_t bufferLoadCount = 0;

    uint64_t materialLoadTime = 0;
    uint64_t materialDescriptorCount = 0;
};

/*
* A GLTF file loaded into CPU memory
*/
class LoadedGltf
{
public:
    LoadedGltf(gfx::RenderEngine* renderEngine, std::string_view gltfPath);

    void loadHDRSkybox(std::string_view exrPath);

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
    ThreadPool threadPool;

    LoadingStats stats;

private:
    gfx::RenderEngine* engine;
    std::filesystem::path path;

    void initDefaultAssets();

    void loadImages(std::span<cgltf_image> gltfImages);
    void loadSamplers(std::span<cgltf_sampler> gltfSamplers);
    void loadTextures(std::span<cgltf_texture> gltfTextures);

    void setMaterialDescriptor(Material& material);
    void loadMaterials(std::span<cgltf_material> gltfMaterials);

    struct IndexBufferDesc
    {
        BufferHandle handle;
        size_t numIndices;
        size_t indexWidth;
    };

    IndexBufferDesc loadIndexBuffer(cgltf_accessor& accessor);

    struct VertexBufferDesc
    {
        BufferHandle handle;
        size_t numVertices;
        glm::vec3 maxPosition;
        glm::vec3 minPosition;
    };

    VertexBufferDesc loadVertexBuffer(const cgltf_primitive& primitive);
    MeshPrimitive createMeshPrimitive(const cgltf_primitive& primitive);
    void loadMeshes(std::span<cgltf_mesh> gltfMeshes);

    MeshNode createMeshNode(const cgltf_mesh& mesh, const glm::mat4& transform);
    void loadScene(const cgltf_scene& gltfScene);
};