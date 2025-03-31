#pragma once

// Vulkan
#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

// VMA
#include <vma/vk_mem_alloc.h>

// SDL
// Tell SDL not to mess with main()
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

// stl
#include <vector>
#include <array>

namespace gfx
{

struct VulkanConfig
{
    const uint32_t vkApiVersion = VK_API_VERSION_1_3;

     const std::array<const char*, 3> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME
    };
};

static const VulkanConfig config;

struct Device
{
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;

    VkSurfaceKHR surface = VK_NULL_HANDLE;

    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

    VmaAllocator allocator = VK_NULL_HANDLE;

    struct QueueFamilyIndices
    {
        uint32_t graphicsFamily = VK_QUEUE_FAMILY_IGNORED;
    };
    QueueFamilyIndices queueFamilyIndices;

    VkQueue graphicsQueue = VK_NULL_HANDLE;

    struct Swapchain
    {
        VkSwapchainKHR swapchain = VK_NULL_HANDLE;
        std::vector<VkImage> swapchainImages;
        std::vector<VkImageView> swapchainImageViews;
        VkFormat swapchainFormat = VK_FORMAT_UNDEFINED;
        VkExtent2D swapchainExtent = VkExtent2D{0, 0};
    };
    Swapchain swapchain;

    Device() = default;
    Device(SDL_Window* window);
    ~Device();
};

Device createDevice(SDL_Window* window);
void cleanupDevice(Device* device);

} // namespace gfx