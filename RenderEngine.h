#pragma once

// Tell SDL not to mess with main()
#define SDL_MAIN_HANDLED

// GLM
#include <glm/glm.hpp>

// SDL
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_vulkan.h>

// Vulkan
#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

// stl
#include <vector>

const size_t NUM_FRAMES = 2;

struct Vertex
{
    glm::vec2 position;
    glm::vec3 color;

    static VkVertexInputBindingDescription getInputBindingDescription();
    static std::array<VkVertexInputAttributeDescription, 2> getInputAttributeDescription();
};

class RenderEngine
{
public:
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;

    VkPhysicalDevice physicalDevice;
    VkDevice device;

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
        VkCommandPool commandPool;
        VkCommandBuffer commandBuffer;
    };
    FrameData frames[NUM_FRAMES];
    size_t currentFrameNumber = 0;

    VkPipelineLayout graphicsPipelineLayout;
    VkPipeline graphicsPipeline;

    void init(SDL_Window* window);
    void cleanup();

private:
    void initInstance(std::vector<const char*>& extensions);
    void initPhysicalDevice();
    void initDevice();
    void initSwapchain();
    void initCommandBuffers();
    void initGraphicsPipeline();

    bool isPhysicalDeviceValid(VkPhysicalDevice device, VkPhysicalDeviceProperties2* deviceProperties);
    FrameData& getCurrentFrameData();
    VkShaderModule loadShaderModule(const char* shaderPath);
};

