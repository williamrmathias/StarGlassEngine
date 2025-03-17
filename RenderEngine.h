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
    glm::vec3 color;

    static VkVertexInputBindingDescription getInputBindingDescription();
    static std::array<VkVertexInputAttributeDescription, 2> getInputAttributeDescription();
};

struct MeshSurface
{
    VkBuffer vertexBuffer;
    VmaAllocation vertexAlloc;
    uint32_t vertexCount;

    VkBuffer indexBuffer;
    VmaAllocation indexAlloc;
    uint32_t indexCount;

    VkPrimitiveTopology topology;
    VkIndexType indexType;
};

struct StaticMesh
{
    std::vector<MeshSurface> surfaces;

    void cleanup(VmaAllocator allocator);
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

    struct FrameData
    {
        VkSemaphore swapchainSemaphore;
        VkSemaphore renderSemaphore;
        VkFence renderFence;

        VkCommandPool commandPool;
        VkCommandBuffer commandBuffer;

        void cleanup(VkDevice device);
    };
    FrameData frames[NUM_FRAMES];
    size_t currentFrameNumber = 0;

    VkBuffer vertexBuffer;
    VmaAllocation vertexBufferAlloc;

    VkBuffer indexBuffer;
    VmaAllocation indexBufferAlloc;

    std::optional<StaticMesh> staticMesh;

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
    void initFrameData();
    void initGeometryBuffers();
    void initGraphicsPipeline();

    bool containsExtensions(std::span<const char* const> extensionsRequired, std::span<VkExtensionProperties> extensionsAvailable);
    bool isPhysicalDeviceValid(VkPhysicalDevice device, VkPhysicalDeviceProperties2* deviceProperties);
    FrameData& getCurrentFrameData();
    void incrementFrameData();
    VkShaderModule loadShaderModule(const char* shaderPath);
    std::optional<StaticMesh> loadStaticMesh(const char* meshPath);
};

