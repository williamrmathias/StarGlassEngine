#pragma once

// sge
#include "Device.h"
#include "Resource.h"

// Tell SDL not to mess with main()
#define SDL_MAIN_HANDLED

// GLM
#include <glm/glm.hpp>

// SDL
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_vulkan.h>
#include <SDL2/SDL_filesystem.h>

// cgltf
#include <cgltf.h>

// VMA
#include <vma/vk_mem_alloc.h>

// Vulkan
#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

// stl
#include <vector>
#include <array>
#include <span>
#include <optional>
#include <memory>

namespace gfx
{

static const size_t NUM_FRAMES = 2;
static const uint32_t NUM_MATERIALS_MAX = 10;

struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec4 color;

    static VkVertexInputBindingDescription getInputBindingDescription();
    static std::array<VkVertexInputAttributeDescription, 4> getInputAttributeDescription();
};

struct Texture
{
    AllocatedImage image;

    VkSampler sampler;
    VkImageView view;

    void cleanup(Device* device);
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

    Texture baseColorTex;
    Texture metalRoughTex;

    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

    void cleanup(Device* device);
};

struct MeshSurface
{
    AllocatedBuffer vertexBuffer;
    AllocatedBuffer indexBuffer;

    uint32_t vertexCount;
    uint32_t indexCount;

    VkPrimitiveTopology topology;
    VkIndexType indexType;

    Material material;
};

struct StaticMesh
{
    std::vector<MeshSurface> surfaces;

    void cleanup(Device* device);
};

struct GlobalSceneData
{
    glm::mat4 viewproj;

    glm::vec3 viewPosition;
    float padding1;

    glm::vec3 lightDirection;
    float padding2;

    glm::vec3 lightColor;
    float padding3;
};

// must be 128 bytes
struct PushConstants
{
    glm::mat4 model;
    MaterialConstants material;

    float padding[10];
};

class RenderEngine
{
public:

    std::unique_ptr<Device> device;

    VkCommandPool immediateCommandPool;
    VkCommandBuffer immediateCommandBuffer;
    VkFence immediateFence;

    AllocatedImage colorImage;
    VkImageView colorView;

    AllocatedImage depthImage;
    VkImageView depthView;

    GlobalSceneData globalSceneData;

    VkDescriptorPool globalDescriptorPool;
    VkDescriptorSetLayout globalSceneDataLayout;
    VkDescriptorSetLayout materialLayout;

    struct FrameData
    {
        VkSemaphore swapchainSemaphore;
        VkSemaphore renderSemaphore;
        VkFence renderFence;

        VkCommandPool commandPool;
        VkCommandBuffer commandBuffer;

        AllocatedBuffer uniformBuffer;

        VkDescriptorSet descriptorSet;

        void cleanup(Device* device);
    };
    FrameData frames[NUM_FRAMES];
    size_t currentFrameNumber = 0;

    std::optional<StaticMesh> staticMesh;

    VkPipelineLayout graphicsPipelineLayout;
    VkPipeline graphicsPipeline;

    void init(SDL_Window* window);
    void render();
    void cleanup();

private:
    void initColorTarget();
    void initDepthTarget();
    void initDescriptorPool();
    void initImmediateStructures();
    void initFrameData();
    void initGeometryBuffers();
    void initGraphicsPipeline();

    VkCommandBuffer startImmediateCommands();
    void endAndSubmitImmediateCommands();

    void copyBufferToBuffer(
        VkCommandBuffer cmd, 
        AllocatedBuffer srcBuffer, AllocatedBuffer dstBuffer,
        VkDeviceSize dataSize
    );

    FrameData& getCurrentFrameData();
    void incrementFrameData();
    VkShaderModule loadShaderModule(const char* shaderPath);
    void initMaterialDescriptor(Material& material);
    Texture loadWhiteTexture();
    Texture loadTexture(cgltf_texture* texture);
    std::optional<StaticMesh> loadStaticMesh(const char* meshPath);
};

} // namespace gfx