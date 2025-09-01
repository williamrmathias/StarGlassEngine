// sge
#include "RenderEngine.h"
#include "Commands.h"
#include "Log.h"

// sdl
#include <SDL2/SDL_log.h>

// vma
#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>

// glm
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_access.hpp>

// stl
#include <algorithm>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>

namespace gfx
{

void RenderEngine::init(SDL_Window* window)
{
    device = std::make_unique<gfx::Device>(window);

    initDepthTarget();

    initDescriptorPool();

    initImmediateStructures();

    initFrameData();

    initGraphicsPipelines();
    initScreenSpacePipelines();

    initImGui(window);

    initScene();
}

void RenderEngine::render()
{
    FrameData& frame = getCurrentFrameData();

    // wait for previous frame to finish
    VK_Check(vkWaitForFences(
        device->device, 1, &frame.renderFence, VK_TRUE, 1'000'000'000));

    // acquire image to draw to
    uint32_t swapchainIdx;
    VK_Check(vkAcquireNextImageKHR(
        device->device, device->swapchain.swapchain, 1'000'000'000, frame.renderSemaphore,
        VK_NULL_HANDLE, &swapchainIdx));

    VK_Check(vkResetFences(device->device, 1, &frame.renderFence));

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

    // transition color image to color attachment layout
    transitionImageLayoutCoarse(
        cmd, frame.hdrColorImage.image,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_ASPECT_COLOR_BIT
    );

    // transition depth image to depth attachment layout
    transitionImageLayoutCoarse(
        cmd, depthImage.image,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        VK_IMAGE_ASPECT_DEPTH_BIT
    );

    // copy to uniform buffer
    gfx::writeToAllocatedBuffer(
        device.get(), &globalSceneData,
        static_cast<VkDeviceSize>(sizeof(globalSceneData)), frame.uniformBuffer
    );

    vkCmdBindDescriptorSets(
        cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, activePipeline.layout, 0, 1, &frame.globalDescriptorSet, 
        0, nullptr
    );

    // begin rendering
    VkRenderingAttachmentInfo colorAttachInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = nullptr,
        .imageView = frame.hdrColorView,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .resolveMode = VK_RESOLVE_MODE_NONE,
        .resolveImageView = VK_NULL_HANDLE,
        .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = VkClearValue{.color = VkClearColorValue{0.f, 0.f, 0.f, 1.f} }
    };

    VkRenderingAttachmentInfo depthAttachInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = nullptr,
        .imageView = depthView,
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        .resolveMode = VK_RESOLVE_MODE_NONE,
        .resolveImageView = VK_NULL_HANDLE,
        .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = VkClearValue{.depthStencil = VkClearDepthStencilValue{.depth = 1.f, .stencil = 1}}
    };

    VkRenderingInfo renderInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .pNext = nullptr,
        .flags = 0,
        .renderArea = VkRect2D{ VkOffset2D{0, 0}, device->swapchain.swapchainExtent},
        .layerCount = 1,
        .viewMask = 0,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachInfo,
        .pDepthAttachment = &depthAttachInfo,
        .pStencilAttachment = nullptr
    };

    vkCmdBeginRendering(cmd, &renderInfo);

    // set dynamic viewport and scissor state
    VkViewport viewport{
        .x = 0.f,
        .y = 0.f,
        .width = static_cast<float>(device->swapchain.swapchainExtent.width),
        .height = static_cast<float>(device->swapchain.swapchainExtent.height),
        .minDepth = 0.f,
        .maxDepth = 1.f
    };
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{
        .offset = VkOffset2D{ 0, 0 },
        .extent = device->swapchain.swapchainExtent
    };
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // bind pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, activePipeline.pipeline);

    drawScene(cmd);

    // end rendering
    vkCmdEndRendering(cmd);

    VkImage swapchainImage = device->swapchain.swapchainImages[swapchainIdx];
    VkImageView swapchainImageView = device->swapchain.swapchainImageViews[swapchainIdx];

    VkExtent3D colorImageExtent{
        frame.hdrColorImage.extents.width,
        frame.hdrColorImage.extents.height,
        1
    };
    VkExtent3D swapchainExtent{
        device->swapchain.swapchainExtent.width,
        device->swapchain.swapchainExtent.height,
        1
    };

    // transition hdr color buffer to readable layout for postFX pass
    transitionImageLayoutCoarse(
        cmd, frame.hdrColorImage.image,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_IMAGE_ASPECT_COLOR_BIT
    );

    // transition swapchain image to drawable layout to render ui & screen space FX
    transitionImageLayoutCoarse(
        cmd, swapchainImage,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_ASPECT_COLOR_BIT
    );

    renderPostFX(cmd, frame, swapchainImageView, device->swapchain.swapchainExtent);

    renderImGui(cmd, swapchainImageView, device->swapchain.swapchainExtent);

    // transition swapchain image to presentable layout
    transitionImageLayoutCoarse(
        cmd, swapchainImage,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_IMAGE_ASPECT_COLOR_BIT
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

    VK_Check(vkQueueSubmit2(device->graphicsQueue, 1, &submitInfo, frame.renderFence));

    // present to swapchain
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &frame.swapchainSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &device->swapchain.swapchain;
    presentInfo.pImageIndices = &swapchainIdx;
    presentInfo.pResults = nullptr;

    VK_Check(vkQueuePresentKHR(device->graphicsQueue, &presentInfo));

    // update frame
    incrementFrameData();
}

void RenderEngine::FrameData::cleanup(gfx::Device* device)
{
    vkDestroySemaphore(device->device, swapchainSemaphore, nullptr);
    vkDestroySemaphore(device->device, renderSemaphore, nullptr);

    vkDestroyFence(device->device, renderFence, nullptr);

    vkDestroyCommandPool(device->device, commandPool, nullptr);

    gfx::destroyAllocatedBuffer(device, uniformBuffer);

    gfx::destroyAllocatedImage(device, hdrColorImage);
    vkDestroyImageView(device->device, hdrColorView, nullptr);
}

void RenderEngine::cleanup()
{
    vkDeviceWaitIdle(device->device);

    loadedGltf->cleanup();

    {
        // cleanup ImGui
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
    }

    vkDestroyCommandPool(device->device, immediateCommandPool, nullptr);
    vkDestroyFence(device->device, immediateFence, nullptr);

    for (FrameData& frame : frames)
        frame.cleanup(device.get());

    vkDestroyDescriptorPool(device->device, globalDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device->device, globalSceneDataLayout, nullptr);
    vkDestroyDescriptorSetLayout(device->device, materialLayout, nullptr);
    vkDestroyDescriptorSetLayout(device->device, screenSpaceLayout, nullptr);

    {
        // pipelines
        graphicsPipeline.cleanup(device.get());

        baseColorPipeline.cleanup(device.get());
        metalPipeline.cleanup(device.get());
        roughPipeline.cleanup(device.get());
        normalPipeline.cleanup(device.get());
        vertNormalPipeline.cleanup(device.get());
        uvPipeline.cleanup(device.get());

        toneMapPipeline.cleanup(device.get());
        passThroughPipeline.cleanup(device.get());
    }

    destroyAllocatedImage(device.get(), depthImage);
    vkDestroyImageView(device->device, depthView, nullptr);

    vkDestroySampler(device->device, screenSpaceSampler, nullptr);

    device.reset();
}

VkCommandBuffer RenderEngine::startImmediateCommands()
{
    VK_Check(vkResetFences(device->device, 1, &immediateFence));
    VK_Check(vkResetCommandBuffer(immediateCommandBuffer, 0));

    VkCommandBuffer cmd = immediateCommandBuffer;

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };

    VK_Check(vkBeginCommandBuffer(cmd, &beginInfo));
    return immediateCommandBuffer;
}

void RenderEngine::endAndSubmitImmediateCommands()
{
    VkCommandBuffer cmd = immediateCommandBuffer;

    VK_Check(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdSubmitInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .pNext = nullptr,
        .commandBuffer = cmd,
        .deviceMask = 0
    };

    VkSubmitInfo2 submit{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .pNext = nullptr,
        .flags = 0,
        .waitSemaphoreInfoCount = 0,
        .pWaitSemaphoreInfos = nullptr,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &cmdSubmitInfo,
        .signalSemaphoreInfoCount = 0,
        .pSignalSemaphoreInfos = nullptr
    };

    // submit command buffer to the queue and execute it
    // will now block until the graphics command finish execution
    VK_Check(vkQueueSubmit2(device->graphicsQueue, 1, &submit, immediateFence));

    VK_Check(vkWaitForFences(device->device, 1, &immediateFence, true, 9'999'999'999));
}

void RenderEngine::setSunDirection(float azimuth, float altitude)
{
    float radAzimuth = glm::radians(azimuth);
    float radAltitude = glm::radians(altitude);

    float cosAltitude = glm::cos(radAltitude);

    globalSceneData.lightDirection = glm::vec3{ 
        cosAltitude * glm::sin(radAzimuth), 
        glm::sin(radAltitude),
        cosAltitude * glm::cos(radAzimuth) 
    };
}

void RenderEngine::setSunLuminance(float luminance)
{
    globalSceneData.lightColor = luminance * glm::vec3(1.f, 1.f, 1.f);
}

void RenderEngine::setViewMatrix(const glm::mat4 view)
{
    glm::mat4 projection = glm::perspective(glm::radians(45.f), 1280.f / 720.f, 0.1f, 100.f);
    projection[1][1] *= -1; // correct gl -> vk

    globalSceneData.viewproj = projection * view;
}

void RenderEngine::setViewPosition(const glm::vec3 viewPosition)
{
    globalSceneData.viewPosition = viewPosition;
}

void RenderEngine::setActiveMainPassPipeline(PipelineType pipeline)
{
    switch (pipeline)
    {
    case gfx::RenderEngine::PipelineType::MainGraphics:
        activePipeline = graphicsPipeline;
        break;
    case gfx::RenderEngine::PipelineType::BaseColorDebug:
        activePipeline = baseColorPipeline;
        break;
    case gfx::RenderEngine::PipelineType::MetalDebug:
        activePipeline = metalPipeline;
        break;
    case gfx::RenderEngine::PipelineType::RoughDebug:
        activePipeline = roughPipeline;
        break;
    case gfx::RenderEngine::PipelineType::NormalDebug:
        activePipeline = normalPipeline;
        break;
    case gfx::RenderEngine::PipelineType::VertexNormalDebug:
        activePipeline = vertNormalPipeline;
        break;
    case gfx::RenderEngine::PipelineType::UvDebug:
        activePipeline = uvPipeline;
        break;
    default:
        break;
    }
}

void RenderEngine::setActiveScreenSpacePipeline(PipelineType pipeline)
{
    switch (pipeline)
    {
    case gfx::RenderEngine::PipelineType::ToneMap:
        activeSSPipeline = toneMapPipeline;
        break;
    case gfx::RenderEngine::PipelineType::PassThrough:
        activeSSPipeline = passThroughPipeline;
        break;
    default:
        break;
    }
}

RenderEngine::RenderTarget RenderEngine::createHDRColorTarget() const
{
    RenderTarget target;

    VkFormat colorFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

    // get color format
    VkFormatProperties formatProps;
    vkGetPhysicalDeviceFormatProperties(device->physicalDevice, colorFormat, &formatProps);
    bool supportsFormat = formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
    if (!supportsFormat)
    {
        SDL_LogError(0, "Color image error: format not supported by device\n");
        std::abort();
    }

    target.image = gfx::createAllocatedImage(
        device.get(),
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        colorFormat, device->swapchain.swapchainExtent, /*useMips*/false
    );

    target.view = createImageView(device.get(), target.image.image, target.image.format, VK_IMAGE_ASPECT_COLOR_BIT);
    return target;
}

void RenderEngine::initDepthTarget()
{
    VkFormat depthFormat = VK_FORMAT_D16_UNORM;

    // get depth format
    VkFormatProperties formatProps;
    vkGetPhysicalDeviceFormatProperties(device->physicalDevice, depthFormat, &formatProps);
    bool supportsFormat = formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if (!supportsFormat)
    {
        SDL_LogError(0, "Depth image error: format not supported by device\n");
        std::abort();
    }

    depthImage = gfx::createAllocatedImage(
        device.get(), 
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 
        depthFormat, device->swapchain.swapchainExtent, /*useMips*/false
    );

    depthView = createImageView(device.get(), depthImage.image, depthImage.format, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void RenderEngine::initDescriptorPool()
{
    // make global scene descriptor layout
    VkDescriptorSetLayoutBinding globalSceneBinding{
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = nullptr
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = 1,
        .pBindings = &globalSceneBinding
    };

    VK_Check(vkCreateDescriptorSetLayout(device->device, &layoutInfo, nullptr, &globalSceneDataLayout));

    // make material layout
    std::array<VkDescriptorSetLayoutBinding, 3> materialBindings;
    
    // base color
    materialBindings[0] = VkDescriptorSetLayoutBinding{
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = nullptr
    };

    // metallic + roughness
    materialBindings[1] = VkDescriptorSetLayoutBinding{
        .binding = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = nullptr
    };

    // normal
    materialBindings[2] = VkDescriptorSetLayoutBinding{
        .binding = 2,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = nullptr
    };

    layoutInfo = VkDescriptorSetLayoutCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = static_cast<uint32_t>(materialBindings.size()),
        .pBindings = materialBindings.data()
    };

    VK_Check(vkCreateDescriptorSetLayout(device->device, &layoutInfo, nullptr, &materialLayout));

    // make screen space pass descriptor layout
    VkDescriptorSetLayoutBinding screenSpaceBinding{
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = nullptr
    };

    layoutInfo = VkDescriptorSetLayoutCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = 1,
        .pBindings = &screenSpaceBinding
    };

    VK_Check(vkCreateDescriptorSetLayout(device->device, &layoutInfo, nullptr, &screenSpaceLayout));

    // make descriptor pool
    std::array<VkDescriptorPoolSize, 3> poolSizes;

    poolSizes[0] = VkDescriptorPoolSize{
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = static_cast<uint32_t>(NUM_FRAMES)
    };

    poolSizes[1] = VkDescriptorPoolSize{
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = NUM_MATERIALS_MAX
    };

    poolSizes[2] = VkDescriptorPoolSize{
    .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    .descriptorCount = static_cast<uint32_t>(NUM_FRAMES)
    };

    VkDescriptorPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .maxSets = NUM_MATERIALS_MAX + NUM_FRAMES + NUM_FRAMES,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data()
    };

    VK_Check(vkCreateDescriptorPool(device->device, &poolInfo, nullptr, &globalDescriptorPool));
}

void RenderEngine::initImmediateStructures()
{
    // create command pools
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.pNext = nullptr;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = device->queueFamilyIndices.graphicsFamily;
    VK_Check(vkCreateCommandPool(device->device, &poolInfo, nullptr, &immediateCommandPool)); 

    // create command buffers
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.pNext = nullptr;
    allocInfo.commandPool = immediateCommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    VK_Check(vkAllocateCommandBuffers(device->device, &allocInfo, &immediateCommandBuffer));

    // create fence
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.pNext = nullptr;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // fence starts signaled
    VK_Check(vkCreateFence(device->device, &fenceInfo, nullptr, &immediateFence));
}

void RenderEngine::initFrameData()
{
    // create command pools
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.pNext = nullptr;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = device->queueFamilyIndices.graphicsFamily;
    for (size_t i = 0; i < NUM_FRAMES; i++)
        VK_Check(vkCreateCommandPool(device->device, &poolInfo, nullptr, &frames[i].commandPool));

    // create command buffers
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.pNext = nullptr;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    for (size_t i = 0; i < NUM_FRAMES; i++)
    {
        allocInfo.commandPool = frames[i].commandPool;
        VK_Check(vkAllocateCommandBuffers(device->device, &allocInfo, &frames[i].commandBuffer));
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
        VK_Check(vkCreateSemaphore(device->device, &semInfo, nullptr, &frames[i].swapchainSemaphore));
        VK_Check(vkCreateSemaphore(device->device, &semInfo, nullptr, &frames[i].renderSemaphore));
        VK_Check(vkCreateFence(device->device, &fenceInfo, nullptr, &frames[i].renderFence));
    }

    // create global uniform buffers + descriptor sets
    for (size_t i = 0; i < NUM_FRAMES; i++)
    {
        frames[i].uniformBuffer = gfx::createAllocatedBuffer(
            device.get(), sizeof(globalSceneData),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
        );
    }

    std::array<VkDescriptorSet, NUM_FRAMES> descriptorSets;
    std::array<VkDescriptorSetLayout, NUM_FRAMES> layouts;
    layouts.fill(globalSceneDataLayout);

    VkDescriptorSetAllocateInfo descriptorInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = globalDescriptorPool,
        .descriptorSetCount = static_cast<uint32_t>(NUM_FRAMES),
        .pSetLayouts = layouts.data()
    };

    VK_Check(vkAllocateDescriptorSets(device->device, &descriptorInfo, descriptorSets.data()));
    for (size_t i = 0; i < NUM_FRAMES; i++)
        frames[i].globalDescriptorSet = descriptorSets[i];

    // update descriptor sets
    for (size_t i = 0; i < NUM_FRAMES; i++)
    {
        VkDescriptorBufferInfo bufferInfo{
            .buffer = frames[i].uniformBuffer.buffer,
            .offset = 0,
            .range = VK_WHOLE_SIZE
        };

        VkWriteDescriptorSet write{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = frames[i].globalDescriptorSet,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pImageInfo = nullptr,
            .pBufferInfo = &bufferInfo,
            .pTexelBufferView = nullptr
        };

        vkUpdateDescriptorSets(device->device, 1, &write, 0, nullptr);
    }

    // create hdr color buffers + descriptor sets
    for (size_t i = 0; i < NUM_FRAMES; i++)
    {
        RenderTarget target = createHDRColorTarget();
        frames[i].hdrColorImage = target.image;
        frames[i].hdrColorView = target.view;
    }

    layouts.fill(screenSpaceLayout);

    descriptorInfo = VkDescriptorSetAllocateInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = globalDescriptorPool,
        .descriptorSetCount = static_cast<uint32_t>(NUM_FRAMES),
        .pSetLayouts = layouts.data()
    };

    VK_Check(vkAllocateDescriptorSets(device->device, &descriptorInfo, descriptorSets.data()));
    for (size_t i = 0; i < NUM_FRAMES; i++)
        frames[i].screenSpaceDescriptorSet = descriptorSets[i];

    // update screen space descriptor sets
    screenSpaceSampler = gfx::createSampler(device.get(), gfx::SamplerDesc::initDefault());

    for (size_t i = 0; i < NUM_FRAMES; i++)
    {
        VkDescriptorImageInfo imageInfo{
            .sampler = screenSpaceSampler,
            .imageView = frames[i].hdrColorView,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };

        VkWriteDescriptorSet write{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = frames[i].screenSpaceDescriptorSet,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &imageInfo,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        };

        vkUpdateDescriptorSets(device->device, 1, &write, 0, nullptr);
    }
}

void RenderEngine::initGraphicsPipelines()
{
    GraphicsPipelineBuilder pipelineBuilder;

    std::filesystem::path vertexShaderPath = std::filesystem::current_path() / std::filesystem::path("Shaders/SimpleShader_simpleVS.spirv");
    VkShaderModule vertShader = loadShaderModule(vertexShaderPath.string().c_str());

    {
        // graphics pipeline
        std::filesystem::path fragmentShaderPath = std::filesystem::current_path() / std::filesystem::path("Shaders/SimpleShader_simplePS.spirv");
        VkShaderModule fragShader = loadShaderModule(fragmentShaderPath.string().c_str());

        // render attachments
        pipelineBuilder.setColorAttachmentFormats(std::span{ &frames[0].hdrColorImage.format, 1});
        pipelineBuilder.setDepthAttachmentFormat(depthImage.format);

        // shader info
        pipelineBuilder.setShaderStages(vertShader, "simpleVS", fragShader, "simplePS");
        pipelineBuilder.setPushConstantSize(static_cast<uint32_t>(sizeof(PushConstants)));

        std::array<VkDescriptorSetLayout, 2> descriptorSetLayouts = {
            globalSceneDataLayout, materialLayout
        };
        pipelineBuilder.setDescriptorSetLayouts(descriptorSetLayouts);

        // IA info
        VkVertexInputBindingDescription vertexBindingDesc = Vertex::getInputBindingDescription();
        std::array<VkVertexInputAttributeDescription, 4> vertexAttribDesc = Vertex::getInputAttributeDescription();
        pipelineBuilder.setVertexInputState(std::span{ &vertexBindingDesc, 1 }, vertexAttribDesc);
        pipelineBuilder.setPrimitiveTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

        // rasterization info
        pipelineBuilder.setPolygonMode(VK_POLYGON_MODE_FILL);
        pipelineBuilder.setSampleCount(VK_SAMPLE_COUNT_1_BIT);
        pipelineBuilder.setDepthMode(VK_TRUE, VK_TRUE);
        pipelineBuilder.disableBlendMode();

        // build pipeline
        graphicsPipeline = pipelineBuilder.build(device.get());

        // Shader modules can be destroyed after the pipeline is created
        vkDestroyShaderModule(device->device, fragShader, nullptr);
    }

    {
        // pbr debug pipelines

        // base color
        std::filesystem::path baseColorFragShaderPath = std::filesystem::current_path() / std::filesystem::path("Shaders/baseColorDebugPS.spirv");
        VkShaderModule baseColorFragShader = loadShaderModule(baseColorFragShaderPath.string().c_str());
        pipelineBuilder.setShaderStages(vertShader, "simpleVS", baseColorFragShader, "baseColorDebugPS");
        baseColorPipeline = pipelineBuilder.build(device.get());

        // metalness
        std::filesystem::path metalFragShaderPath = std::filesystem::current_path() / std::filesystem::path("Shaders/metalDebugPS.spirv");
        VkShaderModule metalFragShader = loadShaderModule(metalFragShaderPath.string().c_str());
        pipelineBuilder.setShaderStages(vertShader, "simpleVS", metalFragShader, "metalDebugPS");
        metalPipeline = pipelineBuilder.build(device.get());

        // roughness
        std::filesystem::path roughFragShaderPath = std::filesystem::current_path() / std::filesystem::path("Shaders/roughDebugPS.spirv");
        VkShaderModule roughFragShader = loadShaderModule(roughFragShaderPath.string().c_str());
        pipelineBuilder.setShaderStages(vertShader, "simpleVS", roughFragShader, "roughDebugPS");
        roughPipeline = pipelineBuilder.build(device.get());

        // normal
        std::filesystem::path normalFragShaderPath = std::filesystem::current_path() / std::filesystem::path("Shaders/normalDebugPS.spirv");
        VkShaderModule normalFragShader = loadShaderModule(normalFragShaderPath.string().c_str());
        pipelineBuilder.setShaderStages(vertShader, "simpleVS", normalFragShader, "normalDebugPS");
        normalPipeline = pipelineBuilder.build(device.get());

        // vertex normal
        std::filesystem::path vertNormalFragShaderPath = std::filesystem::current_path() / std::filesystem::path("Shaders/vertNormalDebugPS.spirv");
        VkShaderModule vertNormalFragShader = loadShaderModule(vertNormalFragShaderPath.string().c_str());
        pipelineBuilder.setShaderStages(vertShader, "simpleVS", vertNormalFragShader, "vertNormalDebugPS");
        vertNormalPipeline = pipelineBuilder.build(device.get());

        // uv
        std::filesystem::path uvFragShaderPath = std::filesystem::current_path() / std::filesystem::path("Shaders/uvDebugPS.spirv");
        VkShaderModule uvFragShader = loadShaderModule(uvFragShaderPath.string().c_str());
        pipelineBuilder.setShaderStages(vertShader, "simpleVS", uvFragShader, "uvDebugPS");
        uvPipeline = pipelineBuilder.build(device.get());

        vkDestroyShaderModule(device->device, baseColorFragShader, nullptr);
        vkDestroyShaderModule(device->device, metalFragShader, nullptr);
        vkDestroyShaderModule(device->device, roughFragShader, nullptr);
        vkDestroyShaderModule(device->device, normalFragShader, nullptr);
        vkDestroyShaderModule(device->device, vertNormalFragShader, nullptr);
        vkDestroyShaderModule(device->device, uvFragShader, nullptr);
    }

    activePipeline = graphicsPipeline;
    vkDestroyShaderModule(device->device, vertShader, nullptr);
}

void RenderEngine::initScreenSpacePipelines()
{
    GraphicsPipelineBuilder pipelineBuilder;

    std::filesystem::path vertexShaderPath = std::filesystem::current_path() / std::filesystem::path("Shaders/ScreenSpace_screenSpaceVS.spirv");
    VkShaderModule vertShader = loadShaderModule(vertexShaderPath.string().c_str());

    {
        // tone map pipeline
        std::filesystem::path fragShaderPath = std::filesystem::current_path() / std::filesystem::path("Shaders/ScreenSpace_toneMapPS.spirv");
        VkShaderModule fragShader = loadShaderModule(fragShaderPath.string().c_str());

        // render attachments
        pipelineBuilder.setColorAttachmentFormats(std::span{ &device->swapchain.swapchainFormat, 1 });

        // shader info
        pipelineBuilder.setShaderStages(vertShader, "screenSpaceVS", fragShader, "toneMapPS");
        pipelineBuilder.setPushConstantSize(static_cast<uint32_t>(sizeof(ScreenSpacePushConstants)));
        pipelineBuilder.setDescriptorSetLayouts(std::span{ &screenSpaceLayout, 1 });

        // IA info
        // no vertex input!
        pipelineBuilder.setPrimitiveTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

        // rasterization info
        pipelineBuilder.setPolygonMode(VK_POLYGON_MODE_FILL);
        pipelineBuilder.setSampleCount(VK_SAMPLE_COUNT_1_BIT);
        pipelineBuilder.setDepthMode(VK_TRUE, VK_TRUE);
        pipelineBuilder.disableBlendMode();

        // build pipeline
        toneMapPipeline = pipelineBuilder.build(device.get());

        vkDestroyShaderModule(device->device, fragShader, nullptr);
    }

    {
        // pass through pipeline
        std::filesystem::path fragShaderPath = std::filesystem::current_path() / std::filesystem::path("Shaders/ScreenSpace_passThroughPS.spirv");
        VkShaderModule fragShader = loadShaderModule(fragShaderPath.string().c_str());
        pipelineBuilder.setShaderStages(vertShader, "screenSpaceVS", fragShader, "passThroughPS");
        passThroughPipeline = pipelineBuilder.build(device.get());

        vkDestroyShaderModule(device->device, fragShader, nullptr);
    }

    activeSSPipeline = toneMapPipeline;
    vkDestroyShaderModule(device->device, vertShader, nullptr);
}

void RenderEngine::initImGui(SDL_Window* window)
{
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForVulkan(window);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = device->instance;
    init_info.PhysicalDevice = device->physicalDevice;
    init_info.Device = device->device;
    init_info.QueueFamily = device->queueFamilyIndices.graphicsFamily;
    init_info.Queue = device->graphicsQueue;
    init_info.DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE + 1;
    init_info.MinImageCount = 2;
    init_info.ImageCount = 2;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.UseDynamicRendering = true;
    // dynamic rendering parameters for imgui to use
    init_info.PipelineRenderingCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &device->swapchain.swapchainFormat;
    ImGui_ImplVulkan_Init(&init_info);

    ImGui_ImplVulkan_CreateFontsTexture();
}

void RenderEngine::initScene()
{
    // set global scene constants
    glm::mat4 view =
        glm::lookAt(glm::vec3(0.f, 1.f, 0.f), glm::vec3(0.f, 0.f, 3.f), glm::vec3(0, 1.f, 0));

    glm::mat4 projection = glm::perspective(glm::radians(45.f), 1280.f / 720.f, 0.1f, 100.f);
    projection[1][1] *= -1; // correct gl -> vk

    globalSceneData = GlobalSceneData{
        .viewproj = projection * view,
        .viewPosition = glm::vec3{0.f, 0.f, 0.f},
        .lightDirection = glm::vec3{0.f, 1.f, 0.f},
        .lightColor = glm::vec3{1.f, 1.f, 1.f}
    };

    setSunDirection(0.f, 90.f);
    setSunLuminance(1.f);

    // load gltf
    std::filesystem::path gltfPath = std::filesystem::current_path() / std::filesystem::path("Assets/Bistro.glb");
    loadedGltf = std::make_unique<LoadedGltf>(this, gltfPath.string().c_str());
}

struct Frustum
{
    std::array<glm::vec4, 6> planes;
};

//https://www.gamedevs.org/uploads/fast-extraction-viewing-frustum-planes-from-world-view-projection-matrix.pdf
static Frustum extractViewFrustum(const glm::mat4& viewproj)
{
    Frustum result{};

    // left plane
    result.planes[0] = glm::row(viewproj, 3) + glm::row(viewproj, 0);

    // right plane
    result.planes[1] = glm::row(viewproj, 3) - glm::row(viewproj, 0);

    // bottom plane
    result.planes[2] = glm::row(viewproj, 3) + glm::row(viewproj, 1);

    // top plane
    result.planes[3] = glm::row(viewproj, 3) - glm::row(viewproj, 1);

    // near plane
    result.planes[4] = glm::row(viewproj, 2);

    // far plane
    result.planes[5] = glm::row(viewproj, 3) - glm::row(viewproj, 2);

    return result;
}

// a primitive is not visible if all 8 of its corners are outside one of the planes 
static bool isVisible(const MeshPrimitive& primitive, const glm::mat4& modelMatrix, const Frustum& viewFrustum)
{
    Extent worldBoundingBox{
        .max = glm::mat3(modelMatrix) * primitive.boundingBox.max,
        .min = glm::mat3(modelMatrix) * primitive.boundingBox.min
    };

    std::array<glm::vec3, 8> corners = worldBoundingBox.getCorners();

    for (glm::vec4 plane : viewFrustum.planes)
    {
        if (glm::dot(corners[0], glm::vec3(plane)) + plane.w > 0.f) continue;
        if (glm::dot(corners[1], glm::vec3(plane)) + plane.w > 0.f) continue;
        if (glm::dot(corners[2], glm::vec3(plane)) + plane.w > 0.f) continue;
        if (glm::dot(corners[3], glm::vec3(plane)) + plane.w > 0.f) continue;
        if (glm::dot(corners[4], glm::vec3(plane)) + plane.w > 0.f) continue;
        if (glm::dot(corners[5], glm::vec3(plane)) + plane.w > 0.f) continue;
        if (glm::dot(corners[6], glm::vec3(plane)) + plane.w > 0.f) continue;
        if (glm::dot(corners[7], glm::vec3(plane)) + plane.w > 0.f) continue;

        // if none of the corners are inside this plane, this object is not visible
        return false;
    }

    return true;
}

void RenderEngine::drawScene(VkCommandBuffer cmd)
{
    Frustum viewFrusum = extractViewFrustum(globalSceneData.viewproj);

    for (const MeshNode& meshNode : loadedGltf->scene.nodes)
    {
        const Mesh& mesh = loadedGltf->meshes[meshNode.mesh];

        for (const MeshPrimitive& primitive : mesh.primitives)
        {
            // cull non visible primitives
            if (!isVisible(primitive, meshNode.transform, viewFrusum))
            {
                continue;
            }

            // bind geometry buffers
            gfx::AllocatedBuffer vertexBuffer = loadedGltf->buffers[primitive.vertexBuffer];
            VkDeviceSize vertexBufferOffset{ 0 };
            vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer.buffer, &vertexBufferOffset);

            gfx::AllocatedBuffer indexBuffer = loadedGltf->buffers[primitive.indexBuffer];
            VkDeviceSize indexBufferOffset{ 0 };
            vkCmdBindIndexBuffer(cmd, indexBuffer.buffer, indexBufferOffset, primitive.indexType);

            // bind material
            const Material& material = loadedGltf->materials[primitive.material];
            vkCmdBindDescriptorSets(
                cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, activePipeline.layout, 
                1, 1, &material.descriptorSet, 0, nullptr
            );

            // bind push constants
            PushConstants pushConstants{
                .model = meshNode.transform,
                .material = material.constants
            };

            vkCmdPushConstants(
                cmd, activePipeline.layout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                sizeof(pushConstants), &pushConstants
            );

            // draw primitive
            vkCmdDrawIndexed(cmd, primitive.indexCount, 1, 0, 0, 0);
        }
    }
}

void RenderEngine::renderPostFX(
    VkCommandBuffer cmd, FrameData& frame, VkImageView colorAttachView, VkExtent2D renderExtent
)
{
    // bind pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, activeSSPipeline.pipeline);

    // bind hdr color buffer
    vkCmdBindDescriptorSets(
        cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, activeSSPipeline.layout,
        0, 1, &frame.screenSpaceDescriptorSet, 0, nullptr
    );

    // bind push constants
    ScreenSpacePushConstants pushConstants{
        .exposure = exposure
    };

    vkCmdPushConstants(
        cmd, activePipeline.layout,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
        sizeof(pushConstants), &pushConstants
    );

    // begin rendering
    VkRenderingAttachmentInfo colorAttachInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = nullptr,
        .imageView = colorAttachView,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .resolveMode = VK_RESOLVE_MODE_NONE,
        .resolveImageView = VK_NULL_HANDLE,
        .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = VkClearValue{.color = VkClearColorValue{0.f, 0.f, 0.f, 1.f} }
    };

    VkRenderingInfo renderInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .pNext = nullptr,
        .flags = 0,
        .renderArea = VkRect2D{ VkOffset2D{0, 0}, renderExtent},
        .layerCount = 1,
        .viewMask = 0,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachInfo,
        .pDepthAttachment = nullptr,
        .pStencilAttachment = nullptr
    };

    vkCmdBeginRendering(cmd, &renderInfo);

    vkCmdDraw(cmd, 3, 1, 0, 0);

    // end rendering
    vkCmdEndRendering(cmd);
}

void RenderEngine::renderImGui(
    VkCommandBuffer cmd, VkImageView colorAttachView, VkExtent2D renderExtent
)
{
    // begin rendering
    VkRenderingAttachmentInfo colorAttachInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = nullptr,
        .imageView = colorAttachView,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .resolveMode = VK_RESOLVE_MODE_NONE,
        .resolveImageView = VK_NULL_HANDLE,
        .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = VkClearValue{.color = VkClearColorValue{0.f, 0.f, 0.f, 1.f} }
    };

    VkRenderingInfo renderInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .pNext = nullptr,
        .flags = 0,
        .renderArea = VkRect2D{ VkOffset2D{0, 0}, renderExtent},
        .layerCount = 1,
        .viewMask = 0,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachInfo,
        .pDepthAttachment = nullptr,
        .pStencilAttachment = nullptr
    };

    vkCmdBeginRendering(cmd, &renderInfo);

    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    // end rendering
    vkCmdEndRendering(cmd);
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
    VK_Check(vkCreateShaderModule(device->device, &createInfo, nullptr, &resultShader));

    return resultShader;
}

} // namespace gfx