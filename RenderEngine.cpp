#include "RenderEngine.h"

// stl
#include <array>
#include <algorithm>
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
    unsigned instExtensionCount;
    SDL_CHECK(SDL_Vulkan_GetInstanceExtensions(window, &instExtensionCount, nullptr));

    std::vector<const char*> instExtensions(instExtensionCount);
    SDL_CHECK(SDL_Vulkan_GetInstanceExtensions(window, &instExtensionCount, instExtensions.data()));

    initInstance(instExtensions);

    // Create a Vulkan surface for rendering
    SDL_CHECK(SDL_Vulkan_CreateSurface(window, instance, &surface));

    initPhysicalDevice();
    initDevice();

    initSwapchain();

    initCommandBuffers();

    initGraphicsPipeline();
}

void RenderEngine::cleanup()
{
    for (FrameData& frame : frames)
    {
        vkDestroyCommandPool(device, frame.commandPool, nullptr);
    }

    vkDestroySwapchainKHR(device, swapchain, nullptr); // also destroys swapchain images

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
    // check that instance supports Vulkan 1.3
    uint32_t instanceAPI;
    vkEnumerateInstanceVersion(&instanceAPI);
    if (instanceAPI < VK_API_VERSION_1_3)
    {
        std::cout << "Detected Vulkan Error: Instance does not support Vulkan 1.3" << std::endl;
        std::abort();
    }

    // Use validation layers if this is a debug build
    std::vector<const char*> layers;
#if defined(_DEBUG)
    instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    layers.push_back("VK_LAYER_KHRONOS_validation");
#endif

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
    std::array<const char*, 1> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

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
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
    deviceCreateInfo.pEnabledFeatures = nullptr; 

    VK_CHECK(vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device));
    vkGetDeviceQueue(device, queueFamilyIndices.graphicsFamily, 0, &graphicsQueue);
}

void RenderEngine::initSwapchain()
{
    // check that double buffering is supported
    VkSurfaceCapabilitiesKHR surfaceCaps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCaps);
    if (surfaceCaps.maxImageCount < 2)
    {
        std::cout << "Detected Vulkan Error: Surface doesn't support double buffering" << std::endl;
        std::abort();
    }

    // check that FIFO present is supported
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);

    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data());
    if (std::find(presentModes.begin(), presentModes.end(), VK_PRESENT_MODE_FIFO_KHR) == presentModes.end())
    {
        std::cout << "Detected Vulkan Error: Surface doesn't support FIFO present mode" << std::endl;
        std::abort();
    }

    // query for surface format and colorspace
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
    if (formatCount == 0)
    {
        std::cout << "Detected Vulkan Error: Surface doesn't support any image formats" << std::endl;
        std::abort();
    }

    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());

    VkSwapchainCreateInfoKHR swapchainInfo{};
    swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.pNext = nullptr;
    swapchainInfo.flags = 0;
    swapchainInfo.surface = surface;
    swapchainInfo.minImageCount = std::min(surfaceCaps.minImageCount + 1, surfaceCaps.maxImageCount);
    swapchainInfo.imageFormat = formats[0].format;
    swapchainInfo.imageColorSpace = formats[0].colorSpace;
    swapchainInfo.imageExtent = surfaceCaps.currentExtent;
    swapchainInfo.imageArrayLayers = 1;
    swapchainInfo.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainInfo.queueFamilyIndexCount = 0;
    swapchainInfo.pQueueFamilyIndices = nullptr;
    swapchainInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR; // don't support screen rotation
    swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchainInfo.clipped = VK_TRUE;
    swapchainInfo.oldSwapchain = VK_NULL_HANDLE;

    VK_CHECK(vkCreateSwapchainKHR(device, &swapchainInfo, nullptr, &swapchain));

    // create swapchain image structs
    swapchainFormat = formats[0].format;
    swapchainExtent = surfaceCaps.currentExtent;

    uint32_t swapchainImageCount;
    vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr);

    swapchainImages.resize(swapchainImageCount);
    vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages.data());
}

void RenderEngine::initCommandBuffers()
{
    // create command pools
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.pNext = nullptr;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;
    for (size_t i = 0; i < NUM_FRAMES; i++)
        VK_CHECK(vkCreateCommandPool(device, &poolInfo, nullptr, &frames[i].commandPool));

    // create command buffers
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.pNext = nullptr;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    for (size_t i = 0; i < NUM_FRAMES; i++)
    {
        allocInfo.commandPool = frames[i].commandPool;
        VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &frames[i].commandBuffer));
    }
}

void RenderEngine::initGraphicsPipeline()
{
    // create shader modules


    // create shader stages
    VkPipelineShaderStageCreateInfo shaderInfo{};

    // create graphics pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = nullptr;
    pipelineInfo.flags = 0;
    pipelineInfo.stageCount = 2;

    //vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, );
}

bool RenderEngine::isPhysicalDeviceValid(
    VkPhysicalDevice device,
    VkPhysicalDeviceProperties2* deviceProperties)
{
    vkGetPhysicalDeviceProperties2(device, deviceProperties);

    if (deviceProperties->properties.apiVersion < VK_API_VERSION_1_3) return false;

    uint32_t queueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties2(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties2> queueFamilies(queueFamilyCount, { VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2 });
    vkGetPhysicalDeviceQueueFamilyProperties2(device, &queueFamilyCount, queueFamilies.data());

    bool graphicsFamilyFound = false;
    VkBool32 presentSupport = VK_FALSE; // use graphics queue to present
    for (uint32_t family = 0; family < queueFamilyCount; family++)
    {
        vkGetPhysicalDeviceSurfaceSupportKHR(device, family, surface, &presentSupport);

        if (!graphicsFamilyFound &&
            queueFamilies[family].queueFamilyProperties.queueFlags | VK_QUEUE_GRAPHICS_BIT &&
            presentSupport == VK_TRUE)
        {
            queueFamilyIndices.graphicsFamily = family;
            graphicsFamilyFound = true;
        }

        if (graphicsFamilyFound) break;
    }

    return graphicsFamilyFound && deviceProperties->properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
}

RenderEngine::FrameData& RenderEngine::getCurrentFrameData()
{
    return frames[currentFrameNumber % NUM_FRAMES];
}