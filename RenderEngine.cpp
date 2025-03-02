#include "RenderEngine.h"

// stl
#include <array>
#include <iostream>

#define VK_CHECK(result)                                     \
    do {                                                     \
        if ((result) != VK_SUCCESS) {                        \
            std::cout << "Detected Vulkan Error: "           \
                      << string_VkResult(result) << std::endl; \
            std::abort();                                    \
        }                                                    \
    } while (0)


#define SDL_CHECK(result)             \
    do {                              \
        if (!(result)) {              \
            std::cout << "Detected SDL Error" << std::endl; \
            std::abort();             \
        }                             \
    } while (0)


static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT * pCallbackData,
    void* pUserData)
{
    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

void RenderEngine::init(SDL_Window* window)
{
    // Get WSI extensions from SDL (we can add more if we like - we just can't remove these)
    unsigned extension_count;
    SDL_CHECK(SDL_Vulkan_GetInstanceExtensions(window, &extension_count, nullptr));

    std::vector<const char*> extensions(extension_count);
    SDL_CHECK(SDL_Vulkan_GetInstanceExtensions(window, &extension_count, extensions.data()));

    initInstance(extensions);

    // Create a Vulkan surface for rendering
    SDL_CHECK(SDL_Vulkan_CreateSurface(window, instance, &surface));

    initPhysicalDevice();
    initDevice();
}

void RenderEngine::cleanup()
{
    vkDestroyDevice(device, nullptr);

    vkDestroySurfaceKHR(instance, surface, nullptr);

#if defined(_DEBUG)
    auto destroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkDestroyDebugUtilsMessengerEXT");

    if (destroyDebugUtilsMessengerEXT) destroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
#endif

    vkDestroyInstance(instance, nullptr);
}

void RenderEngine::initInstance(std::vector<const char*>& instanceExtensions)
{
    // Use validation layers if this is a debug build
    std::vector<const char*> layers;
#if defined(_DEBUG)
    instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    layers.push_back("VK_LAYER_KHRONOS_validation");
#endif

    // check that instance supports Vulkan 1.4
    uint32_t instanceAPI;
    vkEnumerateInstanceVersion(&instanceAPI);
    if (instanceAPI < VK_API_VERSION_1_3)
    {
        std::cout << "Detected Vulkan Error: Instance does not support Vulkan 1.4" << std::endl;
        std::abort();
    }

    // VkApplicationInfo allows the programmer to specifiy some basic information about the
    // program, which can be useful for layers and tools to provide more debug information.
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pNext = nullptr;
    appInfo.pApplicationName = "Vulkan App";
    appInfo.applicationVersion = 1;
    appInfo.pEngineName = "StarGlassEngine";
    appInfo.engineVersion = 1;
    appInfo.apiVersion = VK_API_VERSION_1_3;

    // create debug messenger info
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugCreateInfo.pNext = nullptr;
    debugCreateInfo.flags = 0;
    debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
    debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
    debugCreateInfo.pfnUserCallback = debugCallback;
    debugCreateInfo.pUserData = nullptr;

    // VkInstanceCreateInfo is where the programmer specifies the layers and/or extensions that
    // are needed.
    VkInstanceCreateInfo instInfo = {};
    instInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
#if defined(_DEBUG)
    instInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
#else
    instInfo.pNext = nullptr;
#endif
    instInfo.flags = 0;
    instInfo.pApplicationInfo = &appInfo;
    instInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
    instInfo.ppEnabledExtensionNames = instanceExtensions.data();
    instInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
    instInfo.ppEnabledLayerNames = layers.data();

    // Create the Vulkan instance.
    VK_CHECK(vkCreateInstance(&instInfo, nullptr, &instance));

#if defined(_DEBUG)
    auto createDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkCreateDebugUtilsMessengerEXT");

    if (createDebugUtilsMessengerEXT == nullptr)
    {
        std::cout << "Detected Vulkan Error: 'vkCreateDebugUtilsMessengerEXT' not found" << std::endl;
        std::abort();
    }

    // create debug messenger
    VK_CHECK(createDebugUtilsMessengerEXT(instance, &debugCreateInfo, nullptr, &debugMessenger));
#endif
}

bool RenderEngine::isPhysicalDeviceValid(
    VkPhysicalDevice device,
    VkPhysicalDeviceProperties2* deviceProperties)
{
    vkGetPhysicalDeviceProperties2(device, deviceProperties);

    if (deviceProperties->properties.apiVersion < VK_API_VERSION_1_3) return false;

    uint32_t queueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties2(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties2> queueFamilies(queueFamilyCount, {VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2});
    vkGetPhysicalDeviceQueueFamilyProperties2(device, &queueFamilyCount, queueFamilies.data());

    bool graphicsFamilyFound = false;
    for (uint32_t family = 0; family < queueFamilyCount; family++)
    {
        if (!graphicsFamilyFound &&
            queueFamilies[family].queueFamilyProperties.queueFlags | VK_QUEUE_GRAPHICS_BIT)
        {
            queueFamilyIndices.graphicsFamily = family;
            graphicsFamilyFound = true;
        }

        if (graphicsFamilyFound) break;
    }

    return graphicsFamilyFound && deviceProperties->properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
}

void RenderEngine::initPhysicalDevice()
{
    uint32_t physicalDeviceCount;
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr));

    std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices.data()));

    VkPhysicalDeviceProperties2 deviceProperties{};
    deviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;

    bool physicalDeviceChosen = false;
    for (VkPhysicalDevice device : physicalDevices)
    {
        if (isPhysicalDeviceValid(device, &deviceProperties))
        {
            physicalDevice = device;
            physicalDeviceChosen = true;
            break;
        }
    }

    if (!physicalDeviceChosen)
    {
        std::cout << "Detected Vulkan Error: No Valid Physical Device" << std::endl;
        std::abort();
    }
}

void RenderEngine::initDevice()
{
    float graphicsPriority = 1.f; // max priority

    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.pNext = nullptr;
    queueCreateInfo.flags = 0;
    queueCreateInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &graphicsPriority;

    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = nullptr;
    deviceCreateInfo.flags = 0;
    deviceCreateInfo.queueCreateInfoCount = 1; // graphics
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.enabledExtensionCount = 0;
    deviceCreateInfo.ppEnabledExtensionNames = nullptr;
    deviceCreateInfo.pEnabledFeatures = nullptr; // TODO: add swapchain support    

    VK_CHECK(vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device));

    vkGetDeviceQueue(device, queueFamilyIndices.graphicsFamily, 0, &graphicsQueue);
}