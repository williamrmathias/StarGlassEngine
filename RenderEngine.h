#pragma once

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

static const std::array<const char*, 3> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
    VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME
};

static const uint32_t vkApiVersion = VK_API_VERSION_1_3;

static const size_t NUM_FRAMES = 2;

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
    VkImage image;
    VmaAllocation alloc;

    VkImageView view;

    VkSampler sampler;
};

struct Material
{
    glm::vec4 baseColorFactor;
    float metalnessFactor;
    float roughnessFactor;

    Texture baseColorTex;
    Texture metalRoughTex;
};

struct MeshSurface
{
    VkBuffer vertexBuffer;
    VmaAllocation vertexAlloc;

    VkBuffer indexBuffer;
    VmaAllocation indexAlloc;

    uint32_t vertexCount;
    uint32_t indexCount;

    VkPrimitiveTopology topology;
    VkIndexType indexType;

    Material material;
};

struct StaticMesh
{
    std::vector<MeshSurface> surfaces;

    void cleanup(VmaAllocator allocator);
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

struct PushConstants
{
    glm::mat4 model;
    Material material;
};

class RenderEngine
{
public:
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;

    VkPhysicalDevice physicalDevice;
    VkDevice device;

    VmaAllocator allocator;

    struct QueueFamilyIndices
    {
        uint32_t graphicsFamily;
    };
    QueueFamilyIndices queueFamilyIndices;
    VkQueue graphicsQueue;

    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    VkFormat swapchainFormat;
    VkExtent2D swapchainExtent;

    VkImage depthImage;
    VmaAllocation depthAlloc;
    VkImageView depthView;
    VkFormat depthFormat = VK_FORMAT_D16_UNORM;

    GlobalSceneData globalSceneData;

    VkDescriptorPool globalDescriptorPool;
    VkDescriptorSetLayout globalSceneDataLayout;

    struct FrameData
    {
        VkSemaphore swapchainSemaphore;
        VkSemaphore renderSemaphore;
        VkFence renderFence;

        VkCommandPool commandPool;
        VkCommandBuffer commandBuffer;

        VkBuffer uniformBuffer;
        VmaAllocation uniformAlloc;

        VkDescriptorSet descriptorSet;

        void cleanup(VkDevice device, VmaAllocator allocator);
    };
    FrameData frames[NUM_FRAMES];
    size_t currentFrameNumber = 0;

    std::optional<StaticMesh> staticMesh;

    VkBuffer uniformBuffer;
    VmaAllocation uniformAlloc;

    VkPipelineLayout graphicsPipelineLayout;
    VkPipeline graphicsPipeline;

    void init(SDL_Window* window);
    void render();
    void cleanup();

private:
    void initInstance(std::vector<const char*>& extensions);
    void initPhysicalDevice();
    void initDevice();
    void initVmaAllocator();
    void initSwapchain();
    void initDepth();
    void initDescriptorPool();
    void initFrameData();
    void initGeometryBuffers();
    void initGraphicsPipeline();

    bool containsExtensions(std::span<const char* const> extensionsRequired, std::span<VkExtensionProperties> extensionsAvailable);
    bool isPhysicalDeviceValid(VkPhysicalDevice device, VkPhysicalDeviceProperties2* deviceProperties);
    FrameData& getCurrentFrameData();
    void incrementFrameData();
    VkShaderModule loadShaderModule(const char* shaderPath);
    Texture loadTexture(cgltf_texture* texture);
    std::optional<StaticMesh> loadStaticMesh(const char* meshPath);
};

