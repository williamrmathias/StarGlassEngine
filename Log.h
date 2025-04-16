// sdl
#include <SDL2/SDL_log.h>

// Vulkan
#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

// stl
#include <cstdlib>

inline void VK_Check(VkResult result)
{
    if (result != VK_SUCCESS)
    {
        SDL_LogError(0, "Detected Vulkan Error: %s\n", string_VkResult(result));
        std::abort();
    }
}

inline void SDL_Check(SDL_bool result)
{
    if (result == SDL_FALSE)
    {
        SDL_LogError(0, "Detected SDL Error\n");
        std::abort();
    }
}

constexpr VkDebugUtilsMessageSeverityFlagsEXT enabledMessageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
constexpr VkDebugUtilsMessageTypeFlagsEXT enabledMessageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT;

#if defined(_DEBUG)
static VKAPI_ATTR VkBool32 VKAPI_CALL vulkanDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    SDL_LogError(0, "Vulkan Validation Layer: %s\n", pCallbackData->pMessage);
    return VK_FALSE;
}
#endif