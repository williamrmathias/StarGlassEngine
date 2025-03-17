#include "RenderEngine.h"

// sdl
#include <SDL2/SDL_log.h>

// vma
#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>

// glm
#include <glm/gtc/type_ptr.hpp>

// stl
#include <algorithm>
#include <iostream>
#include <fstream>
#include <filesystem>

// cgltf
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

static inline void VK_Check(VkResult result)
{
    if (result != VK_SUCCESS)
    {
        SDL_LogError(0, "Detected Vulkan Error: %s\n", string_VkResult(result));
        std::abort();
    }
}

static inline void SDL_Check(SDL_bool result)
{
    if (result == SDL_FALSE)
    {
        SDL_LogError(0, "Detected SDL Error\n");
        std::abort();
    }
}

#if defined(_DEBUG)
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT * pCallbackData,
    void* pUserData)
{
    SDL_LogError(0, "Vulkan Validation Layer: %s\n", pCallbackData->pMessage);
    return VK_FALSE;
}
#endif

void RenderEngine::init(SDL_Window* window)
{
    // Get WSI extensions from SDL (we can add more if we like - we just can't remove these)
    unsigned instExtensionCount;
    SDL_Check(SDL_Vulkan_GetInstanceExtensions(window, &instExtensionCount, nullptr));

    std::vector<const char*> instExtensions(instExtensionCount);
    SDL_Check(SDL_Vulkan_GetInstanceExtensions(window, &instExtensionCount, instExtensions.data()));

    initInstance(instExtensions);

    // Create a Vulkan surface for rendering
    SDL_Check(SDL_Vulkan_CreateSurface(window, instance, &surface));

    initPhysicalDevice();
    initDevice();

    initVmaAllocator();

    initSwapchain();

    initFrameData();

    initGeometryBuffers();

    initGraphicsPipeline();
}

static void transitionImageLayout(
    VkCommandBuffer cmd,
    VkImage image,
    VkImageLayout oldLayout, VkImageLayout newLayout,
    VkPipelineStageFlags2 srcStage, VkPipelineStageFlags2 dstStage, 
    VkAccessFlags2 srcAccessMask, VkAccessFlags2 dstAccessMask)
{
    VkImageMemoryBarrier2 imageBarrier{};
    imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    imageBarrier.pNext = nullptr;
    imageBarrier.srcStageMask = srcStage;
    imageBarrier.srcAccessMask = srcAccessMask;
    imageBarrier.dstStageMask = dstStage;
    imageBarrier.dstAccessMask = dstAccessMask;
    imageBarrier.oldLayout = oldLayout;
    imageBarrier.newLayout = newLayout;
    imageBarrier.srcQueueFamilyIndex = 0; // not changing families
    imageBarrier.dstQueueFamilyIndex = 0;
    imageBarrier.image = image;
    imageBarrier.subresourceRange = VkImageSubresourceRange{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1
    };

    VkDependencyInfo dependencyInfo{};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.pNext = nullptr;
    dependencyInfo.dependencyFlags = 0;
    dependencyInfo.memoryBarrierCount = 0;
    dependencyInfo.pBufferMemoryBarriers = nullptr;
    dependencyInfo.bufferMemoryBarrierCount = 0;
    dependencyInfo.pBufferMemoryBarriers = nullptr;
    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers = &imageBarrier;

    vkCmdPipelineBarrier2(cmd, &dependencyInfo);
}

void RenderEngine::render()
{
    FrameData& frame = getCurrentFrameData();

    // wait for previous frame to finish
    VK_Check(vkWaitForFences(
        device, 1, &frame.renderFence, VK_TRUE, 1'000'000'000));

    // acquire image to draw to
    uint32_t swapchainIdx;
    VK_Check(vkAcquireNextImageKHR(
        device, swapchain, 1'000'000'000, frame.renderSemaphore,
        VK_NULL_HANDLE, &swapchainIdx));

    VK_Check(vkResetFences(device, 1, &frame.renderFence));

    // reset command pool
    VkCommandBuffer cmd = frame.commandBuffer;
    VK_Check(vkResetCommandBuffer(cmd, 0));

    // begin and start recording to the command buffer
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.pNext = nullptr;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo = nullptr;

    VK_Check(vkBeginCommandBuffer(cmd, &beginInfo));

    // transition swapchain image to color attachment layout
    transitionImageLayout(
        cmd, swapchainImages[swapchainIdx],
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 
        0, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
    );

    // begin rendering
    VkRenderingAttachmentInfo colorAttachInfo{};
    colorAttachInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachInfo.pNext = nullptr;
    colorAttachInfo.imageView = swapchainImageViews[swapchainIdx];
    colorAttachInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachInfo.resolveMode = VK_RESOLVE_MODE_NONE;
    colorAttachInfo.resolveImageView = VK_NULL_HANDLE;
    colorAttachInfo.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachInfo.clearValue = VkClearValue{ VkClearColorValue{0.f, 0.f, 0.f, 1.f} };

    VkRenderingInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.pNext = nullptr;
    renderInfo.flags = 0;
    renderInfo.renderArea = VkRect2D{ VkOffset2D{0, 0}, swapchainExtent};
    renderInfo.layerCount = 1;
    renderInfo.viewMask = 0;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = &colorAttachInfo;
    renderInfo.pDepthAttachment = nullptr;
    renderInfo.pStencilAttachment = nullptr;

    vkCmdBeginRendering(cmd, &renderInfo);

    // set dynamic viewport and scissor state
    VkViewport viewport{};
    viewport.x = 0.f;
    viewport.y = 0.f;
    viewport.width = static_cast<float>(swapchainExtent.width);
    viewport.height = static_cast<float>(swapchainExtent.height);
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = VkOffset2D{ 0, 0 };
    scissor.extent = swapchainExtent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // bind pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

    // draw static mesh
    if (staticMesh.has_value())
    {
        for (MeshSurface& surface : staticMesh.value().surfaces)
        {
            VkDeviceSize vertexBufferOffset{ 0 };
            vkCmdBindVertexBuffers(cmd, 0, 1, &surface.vertexBuffer, &vertexBufferOffset);

            VkDeviceSize indexBufferOffset{ 0 };
            vkCmdBindIndexBuffer(cmd, surface.indexBuffer, indexBufferOffset, surface.indexType);

            vkCmdDrawIndexed(cmd, surface.indexCount, 1, 0, 0, 0);
        }
    }

    //VkDeviceSize vertexBufferOffset{ 0 };
    //vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer, &vertexBufferOffset);

    //VkDeviceSize indexBufferOffset{ 0 };
    //vkCmdBindIndexBuffer(cmd, indexBuffer, indexBufferOffset, VK_INDEX_TYPE_UINT16);

    //// draw
    //vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);

    // end rendering
    vkCmdEndRendering(cmd);

    // transition swapchain image to presentable layout
    transitionImageLayout(
        cmd, swapchainImages[swapchainIdx],
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, 0
    );

    // end command buffer
    VK_Check(vkEndCommandBuffer(cmd));

    // submit queue
    VkSemaphoreSubmitInfo waitInfo{};
    waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    waitInfo.pNext = nullptr;
    waitInfo.semaphore = frame.renderSemaphore;
    waitInfo.value = 0; // not a timeline semaphore
    waitInfo.stageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
    waitInfo.deviceIndex = 0;

    VkCommandBufferSubmitInfo cmdSubmitInfo{};
    cmdSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    cmdSubmitInfo.pNext = nullptr;
    cmdSubmitInfo.commandBuffer = cmd;
    cmdSubmitInfo.deviceMask = 0;

    VkSemaphoreSubmitInfo signalInfo{};
    signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signalInfo.pNext = nullptr;
    signalInfo.semaphore = frame.swapchainSemaphore;
    signalInfo.value = 0; // not a timeline semaphore
    signalInfo.stageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
    signalInfo.deviceIndex = 0;

    VkSubmitInfo2 submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.pNext = 0;
    submitInfo.flags = 0;
    submitInfo.waitSemaphoreInfoCount = 1;
    submitInfo.pWaitSemaphoreInfos = &waitInfo;
    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos = &cmdSubmitInfo;
    submitInfo.signalSemaphoreInfoCount = 1;
    submitInfo.pSignalSemaphoreInfos = &signalInfo;

    VK_Check(vkQueueSubmit2(graphicsQueue, 1, &submitInfo, frame.renderFence));

    // present to swapchain
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &frame.swapchainSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &swapchainIdx;
    presentInfo.pResults = nullptr;

    VK_Check(vkQueuePresentKHR(graphicsQueue, &presentInfo));

    // update frame
    incrementFrameData();
}

void RenderEngine::FrameData::cleanup(VkDevice device)
{
    vkDestroySemaphore(device, swapchainSemaphore, nullptr);
    vkDestroySemaphore(device, renderSemaphore, nullptr);

    vkDestroyFence(device, renderFence, nullptr);

    vkDestroyCommandPool(device, commandPool, nullptr);
}

void StaticMesh::cleanup(VmaAllocator allocator)
{
    for (MeshSurface& surface : surfaces)
    {
        vmaDestroyBuffer(allocator, surface.indexBuffer, surface.indexAlloc);
        vmaDestroyBuffer(allocator, surface.vertexBuffer, surface.vertexAlloc);
    }
}

void RenderEngine::cleanup()
{
    vkDeviceWaitIdle(device);

    for (FrameData& frame : frames)
        frame.cleanup(device);

    vkDestroyPipelineLayout(device, graphicsPipelineLayout, nullptr);
    vkDestroyPipeline(device, graphicsPipeline, nullptr);

    for (VkImageView view : swapchainImageViews)
        vkDestroyImageView(device, view, nullptr);

    vkDestroySwapchainKHR(device, swapchain, nullptr); // also destroys swapchain images

    vmaDestroyBuffer(allocator, vertexBuffer, vertexBufferAlloc);
    vmaDestroyBuffer(allocator, indexBuffer, indexBufferAlloc);

    if (staticMesh.has_value())
        staticMesh.value().cleanup(allocator);

    vmaDestroyAllocator(allocator);

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
    if (instanceAPI < vkApiVersion)
    {
        SDL_LogError(0, "Detected Vulkan Error: Instance does not support Vulkan 1.3\n");
        std::abort();
    }

    // Use validation layers if this is a debug build
    std::vector<const char*> layers;
#if defined(_DEBUG)
    instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    layers.push_back("VK_LAYER_KHRONOS_validation");
#endif

    // query device extensions
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

    if (!containsExtensions(instanceExtensions, extensions))
    {
        SDL_LogError(0, "Detected Vulkan Error: Instance extensions not supported\n");
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
    appInfo.apiVersion = vkApiVersion;

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
    VK_Check(vkCreateInstance(&instInfo, nullptr, &instance));

#if defined(_DEBUG)
    auto createDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkCreateDebugUtilsMessengerEXT");

    if (createDebugUtilsMessengerEXT == nullptr)
    {
        SDL_LogError(0, "Detected Vulkan Error: 'vkCreateDebugUtilsMessengerEXT' not found\n");
        std::abort();
    }

    // create debug messenger
    VK_Check(createDebugUtilsMessengerEXT(instance, &debugCreateInfo, nullptr, &debugMessenger));
#endif
}

void RenderEngine::initPhysicalDevice()
{
    uint32_t physicalDeviceCount;
    VK_Check(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr));

    std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
    VK_Check(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices.data()));

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
        SDL_LogError(0, "Detected Vulkan Error: No Valid Physical Device\n");
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

    // enable dynamic rendering
    VkPhysicalDeviceDynamicRenderingFeatures dynamicRendering{};
    dynamicRendering.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    dynamicRendering.pNext = nullptr;
    dynamicRendering.dynamicRendering = VK_TRUE;

    // enable synchronization 2
    VkPhysicalDeviceSynchronization2Features sync2{};
    sync2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
    sync2.pNext = &dynamicRendering;
    sync2.synchronization2 = VK_TRUE;

    VkPhysicalDeviceFeatures2 features{};
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features.pNext = &sync2;
    features.features = VkPhysicalDeviceFeatures{ VK_FALSE };

    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = &features;
    deviceCreateInfo.flags = 0;
    deviceCreateInfo.queueCreateInfoCount = 1; // graphics
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
    deviceCreateInfo.pEnabledFeatures = nullptr;

    VK_Check(vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device));

    vkGetDeviceQueue(device, queueFamilyIndices.graphicsFamily, 0, &graphicsQueue);
}

void RenderEngine::initVmaAllocator()
{
    VmaAllocatorCreateInfo allocInfo{};
    allocInfo.flags = 0;
    allocInfo.physicalDevice = physicalDevice;
    allocInfo.device = device;
    allocInfo.preferredLargeHeapBlockSize = 0; // use default heap size
    allocInfo.pAllocationCallbacks = nullptr;
    allocInfo.pDeviceMemoryCallbacks = nullptr;
    allocInfo.pHeapSizeLimit = nullptr;
    allocInfo.pVulkanFunctions = nullptr;
    allocInfo.instance = instance;
    allocInfo.vulkanApiVersion = vkApiVersion;
    allocInfo.pTypeExternalMemoryHandleTypes = nullptr;

    VK_Check(vmaCreateAllocator(&allocInfo, &allocator));
}

void RenderEngine::initSwapchain()
{
    // check that double buffering is supported
    VkSurfaceCapabilitiesKHR surfaceCaps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCaps);
    if (surfaceCaps.maxImageCount < 2)
    {
        SDL_LogError(0, "Detected Vulkan Error: Surface doesn't support double buffering\n");
        std::abort();
    }

    // check that FIFO present is supported
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);

    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data());
    if (std::find(presentModes.begin(), presentModes.end(), VK_PRESENT_MODE_FIFO_KHR) == presentModes.end())
    {
        SDL_LogError(0, "Detected Vulkan Error: Surface doesn't support FIFO present mode\n");
        std::abort();
    }

    // query for surface format and colorspace
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
    if (formatCount == 0)
    {
        SDL_LogError(0, "Detected Vulkan Error: Surface doesn't support any image formats\n");
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
    swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainInfo.queueFamilyIndexCount = 0;
    swapchainInfo.pQueueFamilyIndices = nullptr;
    swapchainInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR; // don't support screen rotation
    swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchainInfo.clipped = VK_TRUE;
    swapchainInfo.oldSwapchain = VK_NULL_HANDLE;

    VK_Check(vkCreateSwapchainKHR(device, &swapchainInfo, nullptr, &swapchain));

    // create swapchain image structs
    swapchainFormat = formats[0].format;
    swapchainExtent = surfaceCaps.currentExtent;

    uint32_t swapchainImageCount;
    vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr);

    swapchainImages.resize(swapchainImageCount);
    vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages.data());

    swapchainImageViews.resize(swapchainImageCount);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.pNext = nullptr;
    viewInfo.flags = 0;
    viewInfo.image = VK_NULL_HANDLE; // set for each view in array
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = swapchainFormat;
    viewInfo.components = VkComponentMapping{ 
        .r = VK_COMPONENT_SWIZZLE_IDENTITY, 
        .g = VK_COMPONENT_SWIZZLE_IDENTITY, 
        .b = VK_COMPONENT_SWIZZLE_IDENTITY, 
        .a = VK_COMPONENT_SWIZZLE_IDENTITY 
    };
    viewInfo.subresourceRange = VkImageSubresourceRange{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1
    };
    for (uint32_t i = 0; i < swapchainImageCount; i++)
    {
        viewInfo.image = swapchainImages[i];
        VK_Check(vkCreateImageView(device, &viewInfo, nullptr, &swapchainImageViews[i]));
    }
}

void RenderEngine::initFrameData()
{
    // create command pools
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.pNext = nullptr;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;
    for (size_t i = 0; i < NUM_FRAMES; i++)
        VK_Check(vkCreateCommandPool(device, &poolInfo, nullptr, &frames[i].commandPool));

    // create command buffers
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.pNext = nullptr;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    for (size_t i = 0; i < NUM_FRAMES; i++)
    {
        allocInfo.commandPool = frames[i].commandPool;
        VK_Check(vkAllocateCommandBuffers(device, &allocInfo, &frames[i].commandBuffer));
    }

    // create sync structures
    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semInfo.pNext = nullptr;
    semInfo.flags = 0;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.pNext = nullptr;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // fence starts signaled

    for (size_t i = 0; i < NUM_FRAMES; i++)
    {
        VK_Check(vkCreateSemaphore(device, &semInfo, nullptr, &frames[i].swapchainSemaphore));
        VK_Check(vkCreateSemaphore(device, &semInfo, nullptr, &frames[i].renderSemaphore));
        VK_Check(vkCreateFence(device, &fenceInfo, nullptr, &frames[i].renderFence));
    }
}

/*
* creates, allocates, and copies data for a Buffer object
*/
static void createBuffer(
    VmaAllocator allocator,
    void* data, VkDeviceSize dataSize, VkBufferUsageFlags usage,
    VkBuffer& buffer, VmaAllocation& allocation)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.pNext = nullptr;
    bufferInfo.flags = 0;
    bufferInfo.size = dataSize;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // exclusive to graphics queue
    bufferInfo.queueFamilyIndexCount = 0;
    bufferInfo.pQueueFamilyIndices = nullptr;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.requiredFlags = 0;
    allocInfo.preferredFlags = 0;
    allocInfo.memoryTypeBits = 0;
    allocInfo.pool = VMA_NULL;
    allocInfo.pUserData = nullptr;
    allocInfo.priority = 0.f;

    VK_Check(vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &buffer, &allocation, nullptr));
    vmaCopyMemoryToAllocation(allocator, data, allocation, 0, dataSize);
}

void RenderEngine::initGeometryBuffers()
{
    // create vertex buffer
    std::array<Vertex, 4> vertexData;
    vertexData[0] = Vertex{ glm::vec3(-0.5f, -0.5f, 0.f), glm::vec3(1.f, 0.f, 0.f) };
    vertexData[1] = Vertex{ glm::vec3(-0.5f, 0.5f, 0.f), glm::vec3(0.f, 1.f, 0.f) };
    vertexData[2] = Vertex{ glm::vec3(0.5f, 0.5f, 0.f), glm::vec3(0.f, 0.f, 1.f) };
    vertexData[3] = Vertex{ glm::vec3(0.5, -0.5f, 0.f), glm::vec3(1.f, 1.f, 1.f) };

    VkDeviceSize vertexDataSize = 
        static_cast<VkDeviceSize>(sizeof(vertexData[0]) * vertexData.size());

    createBuffer(
        allocator, 
        vertexData.data(), vertexDataSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
        vertexBuffer, vertexBufferAlloc
    );

    // create index buffer
    std::array<uint16_t, 6> indexData{ 0, 1, 2, 0, 2, 3 };

    VkDeviceSize indexDataSize = static_cast<VkDeviceSize>(sizeof(indexData[0]) * indexData.size());

    createBuffer(
        allocator,
        indexData.data(), indexDataSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        indexBuffer, indexBufferAlloc
    );

    // load cube mesh
    std::filesystem::path boxPath = std::filesystem::current_path() / std::filesystem::path("Assets/Box.glb");
    staticMesh = loadStaticMesh(boxPath.string().c_str());
}

void RenderEngine::initGraphicsPipeline()
{
    std::filesystem::path vertexShaderPath = std::filesystem::current_path() / std::filesystem::path("Shaders/SimpleShader_simpleVS.spirv");
    std::filesystem::path fragmentShaderPath = std::filesystem::current_path() / std::filesystem::path("Shaders/SimpleShader_simplePS.spirv");

    VkShaderModule vertShader = loadShaderModule(vertexShaderPath.string().c_str());
    VkShaderModule fragShader = loadShaderModule(fragmentShaderPath.string().c_str());

    // create shader stages
    std::array<VkPipelineShaderStageCreateInfo, 2> stageInfos;
    for (auto& stage : stageInfos)
    {
        stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage.pNext = nullptr;
        stage.flags = 0;
        stage.pSpecializationInfo = nullptr;
    }
    stageInfos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stageInfos[0].module = vertShader;
    stageInfos[0].pName = "simpleVS";

    stageInfos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stageInfos[1].module = fragShader;
    stageInfos[1].pName = "simplePS";

    // create vertex input
    VkVertexInputBindingDescription vertexBindingDesc = Vertex::getInputBindingDescription();
    std::array<VkVertexInputAttributeDescription, 2> vertexAttribDesc = Vertex::getInputAttributeDescription();

    VkPipelineVertexInputStateCreateInfo vertexInfo{};
    vertexInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInfo.pNext = nullptr;
    vertexInfo.flags = 0;
    vertexInfo.vertexBindingDescriptionCount = 1;
    vertexInfo.pVertexBindingDescriptions = &vertexBindingDesc;
    vertexInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttribDesc.size());
    vertexInfo.pVertexAttributeDescriptions = vertexAttribDesc.data();

    // create input assembly
    VkPipelineInputAssemblyStateCreateInfo iaInfo{};
    iaInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    iaInfo.pNext = nullptr;
    iaInfo.flags = 0;
    iaInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    iaInfo.primitiveRestartEnable = VK_FALSE;

    // create viewport state
    // Note: We're using dynamic viewport and scissor state
    VkPipelineViewportStateCreateInfo viewportInfo{};
    viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportInfo.pNext = nullptr;
    viewportInfo.flags = 0;
    viewportInfo.viewportCount = 1;
    viewportInfo.pViewports = nullptr;
    viewportInfo.scissorCount = 1;
    viewportInfo.pScissors = nullptr;

    // create rasterization state
    VkPipelineRasterizationStateCreateInfo rasterInfo{};
    rasterInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterInfo.pNext = nullptr;
    rasterInfo.flags = 0;
    rasterInfo.depthClampEnable = VK_FALSE; // disable depth test
    rasterInfo.rasterizerDiscardEnable = VK_FALSE;
    rasterInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterInfo.cullMode = VK_CULL_MODE_NONE; // disable backface culling
    rasterInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterInfo.depthBiasEnable = VK_FALSE; // disable depth bias
    rasterInfo.depthBiasConstantFactor = 0.f;
    rasterInfo.depthBiasClamp = 0.f;
    rasterInfo.depthBiasSlopeFactor = 0.f;
    rasterInfo.lineWidth = 1.f;

    // create multisample state
    // only use 1 sample
    VkPipelineMultisampleStateCreateInfo multiInfo{};
    multiInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multiInfo.pNext = nullptr;
    multiInfo.flags = 0;
    multiInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multiInfo.sampleShadingEnable = VK_FALSE;
    multiInfo.minSampleShading = 0.f;
    multiInfo.pSampleMask = nullptr; // disable sample mask test
    multiInfo.alphaToCoverageEnable = VK_FALSE;
    multiInfo.alphaToOneEnable = VK_FALSE;

    // create depth stencil state
    VkPipelineDepthStencilStateCreateInfo dsInfo{};
    dsInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    dsInfo.pNext = nullptr;
    dsInfo.flags = 0;
    dsInfo.depthTestEnable = VK_FALSE; // disable depth test
    dsInfo.depthWriteEnable = VK_FALSE; // disable depth buffer writes
    dsInfo.depthCompareOp = VK_COMPARE_OP_NEVER;
    dsInfo.depthBoundsTestEnable = VK_FALSE; // disable depth bounds test
    dsInfo.stencilTestEnable = VK_FALSE; // disable stencil test
    dsInfo.front = VkStencilOpState{};
    dsInfo.back = VkStencilOpState{};
    dsInfo.minDepthBounds = 0.f;
    dsInfo.maxDepthBounds = 1.f;

    // create color blend state
    VkPipelineColorBlendAttachmentState blendState{};
    blendState.blendEnable = VK_FALSE;
    blendState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blendState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    blendState.colorBlendOp = VK_BLEND_OP_ADD;
    blendState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blendState.alphaBlendOp = VK_BLEND_OP_ADD;
    blendState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT
        | VK_COLOR_COMPONENT_G_BIT
        | VK_COLOR_COMPONENT_B_BIT
        | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo blendInfo{};
    blendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blendInfo.pNext = nullptr;
    blendInfo.flags = 0;
    blendInfo.logicOpEnable = VK_FALSE;
    blendInfo.logicOp = VK_LOGIC_OP_COPY;
    blendInfo.attachmentCount = 1;
    blendInfo.pAttachments = &blendState;
    blendInfo.blendConstants[0] = 0.f;
    blendInfo.blendConstants[1] = 0.f;
    blendInfo.blendConstants[2] = 0.f;
    blendInfo.blendConstants[3] = 0.f;

    // specify dynamic state (viewport and scissor)
    std::array<VkDynamicState, 2> dynamicStates{
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicInfo{};
    dynamicInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicInfo.pNext = nullptr;
    dynamicInfo.flags = 0;
    dynamicInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicInfo.pDynamicStates = dynamicStates.data();

    // make basic pipeline layout with no descriptors
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pNext = nullptr;
    layoutInfo.flags = 0;
    layoutInfo.setLayoutCount = 0;
    layoutInfo.pSetLayouts = nullptr;
    layoutInfo.pushConstantRangeCount = 0;
    layoutInfo.pPushConstantRanges = nullptr;

    VK_Check(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &graphicsPipelineLayout));

    // rendering create info
    VkPipelineRenderingCreateInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderInfo.pNext = nullptr;
    renderInfo.viewMask = 0;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachmentFormats = &swapchainFormat;
    renderInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
    renderInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

    // create graphics pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = &renderInfo;
    pipelineInfo.flags = 0;
    pipelineInfo.stageCount = static_cast<uint32_t>(stageInfos.size());
    pipelineInfo.pStages = stageInfos.data();
    pipelineInfo.pVertexInputState = &vertexInfo;
    pipelineInfo.pInputAssemblyState = &iaInfo;
    pipelineInfo.pTessellationState = nullptr;
    pipelineInfo.pViewportState = &viewportInfo;
    pipelineInfo.pRasterizationState = &rasterInfo;
    pipelineInfo.pMultisampleState = &multiInfo;
    pipelineInfo.pDepthStencilState = &dsInfo;
    pipelineInfo.pDynamicState = &dynamicInfo;
    pipelineInfo.pColorBlendState = &blendInfo;
    pipelineInfo.layout = graphicsPipelineLayout;
    pipelineInfo.renderPass = VK_NULL_HANDLE; // use dynamic rendering
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // don't derive pipeline
    pipelineInfo.basePipelineIndex = 0;

    VK_Check(vkCreateGraphicsPipelines(
        device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline));

    // Shader modules can be destroyed after the pipeline is created
    vkDestroyShaderModule(device, vertShader, nullptr);
    vkDestroyShaderModule(device, fragShader, nullptr);
}

bool RenderEngine::containsExtensions(
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

bool RenderEngine::isPhysicalDeviceValid(
    VkPhysicalDevice device,
    VkPhysicalDeviceProperties2* deviceProperties)
{
    vkGetPhysicalDeviceProperties2(device, deviceProperties);

    if (deviceProperties->properties.apiVersion < vkApiVersion) return false;

    // query device extensions
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data());

    if (!containsExtensions(deviceExtensions, extensions)) { return false; }

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

    return graphicsFamilyFound;
}

RenderEngine::FrameData& RenderEngine::getCurrentFrameData()
{
    return frames[currentFrameNumber % NUM_FRAMES];
}

void RenderEngine::incrementFrameData()
{
    currentFrameNumber = (currentFrameNumber + 1) % NUM_FRAMES;
}

VkShaderModule RenderEngine::loadShaderModule(const char* shaderPath)
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    
    // load in file
    // load as binary and place file cursor at the end to get the file size
    std::ifstream shader(shaderPath, std::ios::binary | std::ios::ate);
    if (!shader)
    {
        SDL_LogError(0, "Shader load error: Could not open file: %s\n", shaderPath);
        std::abort();
    }

    size_t codeSize = static_cast<size_t>(shader.tellg());
    size_t codeSizeAdjusted = codeSize + (codeSize % 4); // ensure code size is a multiple of 4

    // go back to the begining of the file to load it
    shader.seekg(0, std::ios::beg);

    std::vector<char> shaderCode(codeSizeAdjusted, '\0');
    if (!shader.read(shaderCode.data(), codeSize))
    {
        SDL_LogError(0, "Shader load error: Could not read file data: %s\n", shaderPath);
        std::abort();
    }

    createInfo.codeSize = codeSizeAdjusted;
    createInfo.pCode = reinterpret_cast<uint32_t*>(shaderCode.data());

    VkShaderModule resultShader;
    VK_Check(vkCreateShaderModule(device, &createInfo, nullptr, &resultShader));

    return resultShader;
}

struct ScopedGLTFData
{
    cgltf_data* data;

    cgltf_data* operator->() const { return data; }

    ~ScopedGLTFData()
    {
        cgltf_free(data);
    }
};

std::optional<StaticMesh> RenderEngine::loadStaticMesh(const char* meshPath)
{
    cgltf_options options{}; // default loading options
    ScopedGLTFData gltfData;

    cgltf_result result = cgltf_parse_file(&options, meshPath, &gltfData.data);
    if (result != cgltf_result_success || gltfData.data == nullptr)
    {
        SDL_LogError(0, "Mesh load error: Could not read file data: %s\n", meshPath);
        SDL_LogError(0, "GLTF load error code: %i\n", result);
        return std::nullopt;
    }

    result = cgltf_load_buffers(&options, gltfData.data, meshPath);
    if (result != cgltf_result_success)
    {
        SDL_LogError(0, "Mesh load error: Could not read buffer data: %s\n", meshPath);
        SDL_LogError(0, "GLTF load error code: %i\n", result);
        return std::nullopt;
    }

    if (gltfData->meshes_count < 1)
    {
        SDL_LogError(0, "Mesh load error: Read file contains no meshes: %s\n", meshPath);
        return std::nullopt;
    }

    cgltf_mesh* gltfMesh = &gltfData->meshes[0];

    StaticMesh newMesh;
    newMesh.surfaces.reserve(gltfMesh->primitives_count);

    // sratch buffer data
    std::vector<uint16_t> index16Data;
    std::vector<uint32_t> index32Data;

    std::vector<float> positionData;
    std::vector<float> colorData;

    std::vector<Vertex> vertexData;

    for (cgltf_size i = 0; i < gltfMesh->primitives_count; i++)
    {
        MeshSurface newSurface;
        cgltf_primitive* surface = &gltfMesh->primitives[i];

        // get primative topology
        // TODO: Implement rendering all topology types
        switch (surface->type) 
        {
        //case cgltf_primitive_type_points:
        //    newMesh.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        //    break;
        //case cgltf_primitive_type_lines:
        //    newMesh.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        //    break;
        //case cgltf_primitive_type_line_strip:
        //    newMesh.topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        //    break;
        case cgltf_primitive_type_triangles:
            newSurface.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            break;
        //case cgltf_primitive_type_triangle_strip:
        //    newMesh.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        //    break;
        //case cgltf_primitive_type_triangle_fan:
        //    newMesh.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
        //    break;
        default:
            SDL_LogError(
                0, "Mesh load error: Mesh primitive contains invalid topology: %s\n", meshPath);
            SDL_LogError(0, "GLTF topology code: %i\n", cgltf_primitive_type_points);
            return std::nullopt;
        }

        // load index buffers
        if (surface->indices == nullptr)
        {
            SDL_LogError(0, "Mesh load error: Surface missing indices: %s\n", meshPath);
            return std::nullopt;
        }

        void* indexData;
        cgltf_size indexWidth;
        cgltf_size indexCount = cgltf_accessor_unpack_indices(surface->indices, nullptr, 0, 0);
        switch (surface->indices->component_type)
        {
        case cgltf_component_type_r_16u:
            newSurface.indexType = VK_INDEX_TYPE_UINT16;
            index16Data.resize(indexCount);
            indexData = static_cast<void*>(index16Data.data());
            indexWidth = 2;
            break;
        case cgltf_component_type_r_32u:
            newSurface.indexType = VK_INDEX_TYPE_UINT32;
            index32Data.resize(indexCount);
            indexData = static_cast<void*>(index32Data.data());
            indexWidth = 4;
            break;
        default:
            SDL_LogError(
                0, "Mesh load error: Mesh primitive invalid index format: %s\n", meshPath);
            return std::nullopt;
        }

        cgltf_accessor_unpack_indices(surface->indices, indexData, indexWidth, indexCount);
        newSurface.indexCount = indexCount;

        createBuffer(
            allocator, 
            indexData, static_cast<VkDeviceSize>(indexCount * indexWidth * 8), VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            newSurface.indexBuffer, newSurface.indexAlloc
        );

        // load vertex attributes
        // just load one float3 position and one float3 color (TODO: Load other attributes)

        // position
        {
            const cgltf_accessor* positionAcc =
                cgltf_find_accessor(surface, cgltf_attribute_type_position, 0);

            if (positionAcc == nullptr || positionAcc->component_type != cgltf_component_type_r_32f
                || positionAcc->type != cgltf_type_vec3)
            {
                SDL_LogError(0, "Mesh load error: Surface position attrib invalid: %s\n", meshPath);
                return std::nullopt;
            }

            cgltf_size positionCount = cgltf_accessor_unpack_floats(positionAcc, nullptr, 0);

            positionData.resize(positionCount);
            cgltf_accessor_unpack_floats(positionAcc, positionData.data(), positionCount);
        }

        // color
        {
            const cgltf_accessor* colorAcc =
                cgltf_find_accessor(surface, cgltf_attribute_type_color, 0);

            if (colorAcc == nullptr || colorAcc->component_type != cgltf_component_type_r_32f
                || colorAcc->type != cgltf_type_vec3)
            {
                SDL_LogInfo(0, "Mesh load info: Surface color0 attrib invalid: %s\n", meshPath);
            }
            else
            {
                cgltf_size colorCount = cgltf_accessor_unpack_floats(colorAcc, nullptr, 0);

                colorData.resize(colorCount);
                cgltf_accessor_unpack_floats(colorAcc, colorData.data(), colorCount);
            }
        }

        // interleave
        size_t vertexCount = positionData.size() / 3;
        vertexData.resize(vertexCount);
        for (size_t i = 0; i < vertexCount; i++)
        {
            vertexData[i].position = glm::make_vec3(&positionData[3 * i]);

            // if there's no color, use default
            if (colorData.size() <= 3 * i)
                vertexData[i].color = glm::vec3{ 0.5f, 0.5f, 0.5f };
            else
                vertexData[i].color = glm::make_vec3(&colorData[3 * i]);
        }

        newSurface.vertexCount = vertexCount;
        createBuffer(
            allocator,
            vertexData.data(), static_cast<VkDeviceSize>(sizeof(vertexData[0]) * vertexCount), 
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            newSurface.vertexBuffer, newSurface.vertexAlloc
        );

        // add new surface
        newMesh.surfaces.push_back(newSurface);
    }

    return newMesh;
}

VkVertexInputBindingDescription Vertex::getInputBindingDescription()
{
    VkVertexInputBindingDescription bindingDesc{};
    bindingDesc.binding = 0;
    bindingDesc.stride = sizeof(Vertex);
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return bindingDesc;
}

std::array<VkVertexInputAttributeDescription, 2> Vertex::getInputAttributeDescription()
{
    std::array<VkVertexInputAttributeDescription, 2> attribDesc;

    // position attrib
    attribDesc[0].location = 0;
    attribDesc[0].binding = 0;
    attribDesc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribDesc[0].offset = offsetof(Vertex, position);

    // color attrib
    attribDesc[1].location = 1;
    attribDesc[1].binding = 0;
    attribDesc[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribDesc[1].offset = offsetof(Vertex, color);

    return attribDesc;
}