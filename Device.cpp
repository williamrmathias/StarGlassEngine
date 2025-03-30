// sge
#include "Device.h"
#include "Log.h"

// SDL
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_vulkan.h>

// stl
#include <span>
#include <optional>

namespace gfx
{

static bool containsExtensions(
    std::span<const char* const> extensionsRequired, std::span<VkExtensionProperties> extensionsAvailable)
{
    for (const char* extensionReq : extensionsRequired)
    {
        bool extensionFound = false;
        for (const VkExtensionProperties& extensionProp : extensionsAvailable)
        {
            if (strcmp(extensionReq, extensionProp.extensionName) == 0)
            {
                extensionFound = true;
                break;
            }
        }

        if (!extensionFound) { return false; }
    }

    return true;
}

static VkInstance initInstance(std::span<const char* const> instanceExtensions) 
{
    VkInstance instance;

    // check that instance supports Vulkan 1.3
    uint32_t instanceAPI;
    vkEnumerateInstanceVersion(&instanceAPI);
    if (instanceAPI < config.vkApiVersion)
    {
        SDL_LogError(0, "Detected Vulkan Error: Instance does not support Vulkan 1.3\n");
        std::abort();
    }

    // Use validation layers if this is a debug build
    std::vector<const char*> layers;
#if defined(_DEBUG)
    layers.push_back("VK_LAYER_KHRONOS_validation");
#endif

    // query instance extension support
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

    if (!containsExtensions(instanceExtensions, extensions))
    {
        SDL_LogError(0, "Detected Vulkan Error: Instance extensions not supported\n");
        std::abort();
    }

    // VkApplicationInfo allows us to specifiy some basic information about the
    // program, which can be useful for layers and tools to provide more debug information.
    VkApplicationInfo appInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .pApplicationName = "Vulkan App",
        .applicationVersion = 1,
        .pEngineName = "StarGlassEngine",
        .engineVersion = 1,
        .apiVersion = config.vkApiVersion,
    };

    // create debug messenger info
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .pNext = nullptr,
        .flags = 0,
        .messageSeverity = enabledMessageSeverity,
        .messageType = enabledMessageType,
        .pfnUserCallback = vulkanDebugCallback,
        .pUserData = nullptr
    };

    // VkInstanceCreateInfo allows us to specify the layers and/or extensions that are needed.
    VkInstanceCreateInfo instInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = static_cast<uint32_t>(layers.size()),
        .ppEnabledLayerNames = layers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size()),
        .ppEnabledExtensionNames = instanceExtensions.data()
    };

#if defined(_DEBUG)
    // when in debug mode, extent instance creation with the debug messenger
    instInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
#else
    instInfo.pNext = nullptr;
#endif

    // Create the Vulkan instance.
    VK_Check(vkCreateInstance(&instInfo, nullptr, &instance));
    return instance;
}

#if defined(_DEBUG)
static VkDebugUtilsMessengerEXT initDebugMessenger(const VkInstance instance) 
{
    VkDebugUtilsMessengerEXT debugMessenger;

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .pNext = nullptr,
        .flags = 0,
        .messageSeverity = enabledMessageSeverity,
        .messageType = enabledMessageType,
        .pfnUserCallback = vulkanDebugCallback,
        .pUserData = nullptr
    };

    auto createDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkCreateDebugUtilsMessengerEXT"
    );

    if (createDebugUtilsMessengerEXT == nullptr)
    {
        SDL_LogError(0, "Detected Vulkan Error: 'vkCreateDebugUtilsMessengerEXT' not found\n");
        std::abort();
    }

    // create debug messenger
    VK_Check(createDebugUtilsMessengerEXT(instance, &debugCreateInfo, nullptr, &debugMessenger));
    return debugMessenger;
}
#endif

static std::optional<Device::QueueFamilyIndices> findQueueFamilies(
    const VkPhysicalDevice physicalDevice,
    const VkSurfaceKHR surface)
{
    Device::QueueFamilyIndices indices;

    uint32_t queueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties2> queueFamilies(queueFamilyCount, { .sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2 });
    vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamilyCount, queueFamilies.data());

    bool graphicsFamilyFound = false;
    VkBool32 presentSupport = VK_FALSE; // use graphics queue to present
    for (uint32_t family = 0; family < queueFamilyCount; family++)
    {
        vkGetPhysicalDeviceSurfaceSupportKHR(
            physicalDevice, family, surface, &presentSupport
        );

        if (!graphicsFamilyFound &&
            queueFamilies[family].queueFamilyProperties.queueFlags | VK_QUEUE_GRAPHICS_BIT &&
            presentSupport == VK_TRUE)
        {
            indices.graphicsFamily = family;
            graphicsFamilyFound = true;
        }

        if (graphicsFamilyFound) break;
    }

    if (graphicsFamilyFound) { return indices; }
    else { return std::nullopt; }
}

static bool isPhysicalDeviceValid(
    const Device& device,
    VkPhysicalDevice physicalDevice,
    VkPhysicalDeviceProperties2* deviceProperties,
    std::span<const char* const> deviceExtensions)
{
    vkGetPhysicalDeviceProperties2(physicalDevice, deviceProperties);

    if (deviceProperties->properties.apiVersion < config.vkApiVersion) return false;

    // query device extensions
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, extensions.data());

    if (!containsExtensions(deviceExtensions, extensions)) { return false; }

    return findQueueFamilies(physicalDevice, device.surface).has_value();
}

static VkPhysicalDevice initPhysicalDevice(const Device& device)
{
    VkPhysicalDevice physicalDevice;

    uint32_t physicalDeviceCount;
    VK_Check(vkEnumeratePhysicalDevices(device.instance, &physicalDeviceCount, nullptr));

    std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
    VK_Check(vkEnumeratePhysicalDevices(device.instance, &physicalDeviceCount, physicalDevices.data()));

    VkPhysicalDeviceProperties2 deviceProperties{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };

    bool physicalDeviceChosen = false;
    for (VkPhysicalDevice physicalDevice : physicalDevices)
    {
        if (isPhysicalDeviceValid(device, physicalDevice, &deviceProperties, config.deviceExtensions))
        {
            physicalDevice = physicalDevice;
            physicalDeviceChosen = true;
            break;
        }
    }

    if (!physicalDeviceChosen)
    {
        SDL_LogError(0, "Detected Vulkan Error: No Valid Physical Device\n");
        std::abort();
    }

    return physicalDevice;
}

static VkDevice initLogicalDevice(
    const VkPhysicalDevice physicalDevice, 
    const Device::QueueFamilyIndices indices)
{
    VkDevice device;

    float graphicsPriority = 1.f; // max priority

    VkDeviceQueueCreateInfo queueCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .queueFamilyIndex = indices.graphicsFamily,
        .queueCount = 1,
        .pQueuePriorities = &graphicsPriority
    };

    // enable dynamic rendering
    VkPhysicalDeviceDynamicRenderingFeatures dynamicRendering{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
        .pNext = nullptr,
        .dynamicRendering = VK_TRUE
    };

    // enable synchronization 2
    VkPhysicalDeviceSynchronization2Features sync2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
        .pNext = &dynamicRendering,
        .synchronization2 = VK_TRUE
    };

    VkPhysicalDeviceFeatures2 features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &sync2,
        .features = VkPhysicalDeviceFeatures{ VK_FALSE }
    };

    VkDeviceCreateInfo deviceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &features,
        .flags = 0,
        .queueCreateInfoCount = 1, // graphics
        .pQueueCreateInfos = &queueCreateInfo,
        .enabledExtensionCount = static_cast<uint32_t>(config.deviceExtensions.size()),
        .ppEnabledExtensionNames = config.deviceExtensions.data(),
        .pEnabledFeatures = nullptr
    };

    VK_Check(vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device));
    return device;
}

static VkQueue initQueue(const VkDevice device, const uint32_t family)
{
    VkQueue queue;
    vkGetDeviceQueue(device, family, 0, &queue);
    return queue;
}

static VmaAllocator initAllocator(const Device& device)
{
    VmaAllocator allocator;

    VmaAllocatorCreateInfo allocInfo{
        .flags = 0,
        .physicalDevice = device.physicalDevice,
        .device = device.device,
        .preferredLargeHeapBlockSize = 0, // use default heap size
        .pAllocationCallbacks = nullptr,
        .pDeviceMemoryCallbacks = nullptr,
        .pHeapSizeLimit = nullptr,
        .pVulkanFunctions = nullptr,
        .instance = device.instance,
        .vulkanApiVersion = config.vkApiVersion,
        .pTypeExternalMemoryHandleTypes = nullptr
    };

    VK_Check(vmaCreateAllocator(&allocInfo, &allocator));

    return allocator;
}

static Device::Swapchain initSwapchain(const Device& device)
{
    Device::Swapchain swapchain;

    // check that double buffering is supported
    VkSurfaceCapabilitiesKHR surfaceCaps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.physicalDevice, device.surface, &surfaceCaps);
    if (surfaceCaps.maxImageCount < 2)
    {
        SDL_LogError(0, "Detected Vulkan Error: Surface doesn't support double buffering\n");
        std::abort();
    }

    // check that FIFO present is supported
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device.physicalDevice, device.surface, &presentModeCount, nullptr);

    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device.physicalDevice, device.surface, &presentModeCount, presentModes.data());
    if (std::find(presentModes.begin(), presentModes.end(), VK_PRESENT_MODE_FIFO_KHR) == presentModes.end())
    {
        SDL_LogError(0, "Detected Vulkan Error: Surface doesn't support FIFO present mode\n");
        std::abort();
    }

    // query for surface format and colorspace
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device.physicalDevice, device.surface, &formatCount, nullptr);
    if (formatCount == 0)
    {
        SDL_LogError(0, "Detected Vulkan Error: Surface doesn't support any image formats\n");
        std::abort();
    }

    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device.physicalDevice, device.surface, &formatCount, formats.data());

    VkSwapchainCreateInfoKHR swapchainInfo{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .surface = device.surface,
        .minImageCount = std::min(surfaceCaps.minImageCount + 1, surfaceCaps.maxImageCount),
        .imageFormat = formats[0].format,
        .imageColorSpace = formats[0].colorSpace,
        .imageExtent = surfaceCaps.currentExtent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR, // don't support screen rotation
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE
    };

    VK_Check(vkCreateSwapchainKHR(device.device, &swapchainInfo, nullptr, &swapchain.swapchain));

    // create swapchain image structs
    swapchain.swapchainFormat = formats[0].format;
    swapchain.swapchainExtent = surfaceCaps.currentExtent;

    uint32_t swapchainImageCount;
    vkGetSwapchainImagesKHR(device.device, swapchain.swapchain, &swapchainImageCount, nullptr);

    swapchain.swapchainImages.resize(swapchainImageCount);
    vkGetSwapchainImagesKHR(device.device, swapchain.swapchain, &swapchainImageCount, swapchain.swapchainImages.data());

    swapchain.swapchainImageViews.resize(swapchainImageCount);

    VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = VK_NULL_HANDLE, // set for each view in array
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = swapchain.swapchainFormat,
        .components = VkComponentMapping{
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY
        },
        .subresourceRange = VkImageSubresourceRange{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };
    for (uint32_t i = 0; i < swapchainImageCount; i++)
    {
        viewInfo.image = swapchain.swapchainImages[i];
        VK_Check(vkCreateImageView(device.device, &viewInfo, nullptr, &swapchain.swapchainImageViews[i]));
    }

    return swapchain;
}

Device createDevice(SDL_Window* window)
{
    Device device;

    // Get WSI extensions from SDL
    unsigned instExtensionCount;
    SDL_Check(SDL_Vulkan_GetInstanceExtensions(window, &instExtensionCount, nullptr));

    std::vector<const char*> instExtensions(instExtensionCount);
    SDL_Check(SDL_Vulkan_GetInstanceExtensions(window, &instExtensionCount, instExtensions.data()));

#if defined(_DEBUG)
    // enable vulkan debugging utils in debug mode
    instExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

    device.instance = initInstance(instExtensions);
    device.debugMessenger = initDebugMessenger(device.instance);

    // create a Vulkan surface for rendering
    SDL_Check(SDL_Vulkan_CreateSurface(window, device.instance, &device.surface));

    device.physicalDevice = initPhysicalDevice(device);

    std::optional<Device::QueueFamilyIndices> queueFamilyIndices = findQueueFamilies(
        device.physicalDevice, device.surface
    );

    if (!queueFamilyIndices.has_value())
    {
        SDL_LogError(0, "Detected Vulkan Error: Queue Family Index Missing\n");
        std::abort();
    }
    else
    {
        device.queueFamilyIndices = findQueueFamilies(device.physicalDevice, device.surface).value();
    }

    device.device = initLogicalDevice(device.physicalDevice, device.queueFamilyIndices);
    device.graphicsQueue = initQueue(device.device, device.queueFamilyIndices.graphicsFamily);

    device.allocator = initAllocator(device);

    device.swapchain = initSwapchain(device);
    
    return device;
}

void cleanupDevice(Device* device)
{
    if (!device) { return; }

    for (VkImageView view : device->swapchain.swapchainImageViews)
        vkDestroyImageView(device->device, view, nullptr);

    vkDestroySwapchainKHR(device->device, device->swapchain.swapchain, nullptr); // also destroys swapchain images

    vmaDestroyAllocator(device->allocator);

    vkDestroyDevice(device->device, nullptr);

    vkDestroySurfaceKHR(device->instance, device->surface, nullptr);

#if defined(_DEBUG)
    auto destroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        device->instance, "vkDestroyDebugUtilsMessengerEXT");

    if (destroyDebugUtilsMessengerEXT)
    {
        destroyDebugUtilsMessengerEXT(device->instance, device->debugMessenger, nullptr);
    }
#endif

    vkDestroyInstance(device->instance, nullptr);
}

} // namespace gfx