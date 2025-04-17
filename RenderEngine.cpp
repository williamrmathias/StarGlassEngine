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

// stl
#include <algorithm>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>

// cgltf
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

// stb
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace gfx
{

void RenderEngine::init(SDL_Window* window)
{
    device = std::make_unique<gfx::Device>(window);

    initColorTarget();

    initDepthTarget();

    initDescriptorPool();

    initImmediateStructures();

    initFrameData();

    initGeometryBuffers();

    initGraphicsPipeline();
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
        cmd, colorImage.image,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_ASPECT_COLOR_BIT
    );

    // transition depth image to depth attachment layout
    transitionImageLayoutCoarse(
        cmd, depthImage.image,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        VK_IMAGE_ASPECT_DEPTH_BIT
    );

    // update constants
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

    // copy to uniform buffer
    gfx::writeToAllocatedBuffer(
        device.get(), &globalSceneData,
        static_cast<VkDeviceSize>(sizeof(globalSceneData)), frame.uniformBuffer
    );

    vkCmdBindDescriptorSets(
        cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelineLayout, 0, 1, &frame.descriptorSet, 
        0, nullptr
    );

    // begin rendering
    VkRenderingAttachmentInfo colorAttachInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = nullptr,
        .imageView = colorView,
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

    VkRenderingInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.pNext = nullptr;
    renderInfo.flags = 0;
    renderInfo.renderArea = VkRect2D{ VkOffset2D{0, 0}, device->swapchain.swapchainExtent};
    renderInfo.layerCount = 1;
    renderInfo.viewMask = 0;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = &colorAttachInfo;
    renderInfo.pDepthAttachment = &depthAttachInfo;
    renderInfo.pStencilAttachment = nullptr;

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
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

    // draw static mesh
    if (staticMesh.has_value())
    {
        for (MeshSurface& surface : staticMesh.value().surfaces)
        {
            VkDeviceSize vertexBufferOffset{ 0 };
            vkCmdBindVertexBuffers(cmd, 0, 1, &surface.vertexBuffer.buffer, &vertexBufferOffset);

            VkDeviceSize indexBufferOffset{ 0 };
            vkCmdBindIndexBuffer(cmd, surface.indexBuffer.buffer, indexBufferOffset, surface.indexType);

            // bind material
            vkCmdBindDescriptorSets(
                cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelineLayout, 1, 1, 
                &surface.material.descriptorSet, 0, nullptr
            );

            // set push constants
            static float angle = 0.f;
            glm::mat4 model = glm::identity<glm::mat4>();
            model = glm::translate(model, glm::vec3(0.f, 0.f, 3.f));
            model = glm::rotate(
                model,
                glm::radians(angle),
                glm::vec3(0.f, 1.f, 0.f)
            );

            PushConstants pushConstants{
                .model = model,
                .material = surface.material.constants
            };

            vkCmdPushConstants(
                cmd,
                graphicsPipelineLayout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                sizeof(PushConstants),
                &pushConstants
            );

            angle += 0.1f;

            vkCmdDrawIndexed(cmd, surface.indexCount, 1, 0, 0, 0);
        }
    }

    // end rendering
    vkCmdEndRendering(cmd);

    // blit main color image to swapchain image
    VkImage swapchainImage = device->swapchain.swapchainImages[swapchainIdx];

    VkExtent3D colorImageExtent{
        colorImage.extents.width,
        colorImage.extents.height,
        1
    };
    VkExtent3D swapchainExtent{
        device->swapchain.swapchainExtent.width,
        device->swapchain.swapchainExtent.height,
        1
    };

    blitImageToImage(
        cmd,
        colorImage.image, swapchainImage,
        colorImageExtent, swapchainExtent,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_ASPECT_COLOR_BIT
    );

    // transition swapchain image to presentable layout
    transitionImageLayoutCoarse(
        cmd, swapchainImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
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
}

void Texture::cleanup(gfx::Device* device)
{
    gfx::destroyAllocatedImage(device, image);
    vkDestroyImageView(device->device, view, nullptr);
    vkDestroySampler(device->device, sampler, nullptr);
}

void Material::cleanup(gfx::Device* device)
{
    baseColorTex.cleanup(device);
    metalRoughTex.cleanup(device);
}

void StaticMesh::cleanup(gfx::Device* device)
{
    for (MeshSurface& surface : surfaces)
    {
        gfx::destroyAllocatedBuffer(device, surface.indexBuffer);
        gfx::destroyAllocatedBuffer(device, surface.vertexBuffer);

        surface.material.cleanup(device);
    }
}

void RenderEngine::cleanup()
{
    vkDeviceWaitIdle(device->device);

    vkDestroyCommandPool(device->device, immediateCommandPool, nullptr);
    vkDestroyFence(device->device, immediateFence, nullptr);

    for (FrameData& frame : frames)
        frame.cleanup(device.get());

    vkDestroyDescriptorPool(device->device, globalDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device->device, globalSceneDataLayout, nullptr);
    vkDestroyDescriptorSetLayout(device->device, materialLayout, nullptr);

    vkDestroyPipelineLayout(device->device, graphicsPipelineLayout, nullptr);
    vkDestroyPipeline(device->device, graphicsPipeline, nullptr);

    destroyAllocatedImage(device.get(), colorImage);
    vkDestroyImageView(device->device, colorView, nullptr);

    destroyAllocatedImage(device.get(), depthImage);
    vkDestroyImageView(device->device, depthView, nullptr);

    if (staticMesh.has_value())
        staticMesh.value().cleanup(device.get());

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

void RenderEngine::initColorTarget()
{
    VkFormat colorFormat = VK_FORMAT_R8G8B8A8_UNORM;

    // get color format
    VkFormatProperties formatProps;
    vkGetPhysicalDeviceFormatProperties(device->physicalDevice, colorFormat, &formatProps);
    bool supportsFormat = formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
    if (!supportsFormat)
    {
        SDL_LogError(0, "Depth image error: format not supported by device\n");
        std::abort();
    }

    colorImage = gfx::createAllocatedImage(
        device.get(),
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        colorFormat, device->swapchain.swapchainExtent
    );

    colorView = createImageView(device.get(), colorImage.image, colorImage.format, VK_IMAGE_ASPECT_COLOR_BIT);
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
        depthFormat, device->swapchain.swapchainExtent
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
    std::array<VkDescriptorSetLayoutBinding, 2> materialBindings;
    
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

    layoutInfo = VkDescriptorSetLayoutCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = static_cast<uint32_t>(materialBindings.size()),
        .pBindings = materialBindings.data()
    };

    VK_Check(vkCreateDescriptorSetLayout(device->device, &layoutInfo, nullptr, &materialLayout));

    // make descriptor pool
    std::array<VkDescriptorPoolSize, 2> poolSizes;

    poolSizes[0] = VkDescriptorPoolSize{
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = static_cast<uint32_t>(NUM_FRAMES)
    };

    poolSizes[1] = VkDescriptorPoolSize{
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = NUM_MATERIALS_MAX
    };

    VkDescriptorPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .maxSets = NUM_MATERIALS_MAX + NUM_FRAMES,
        .poolSizeCount = 2,
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

    // create uniform buffers + descriptor sets
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
        frames[i].descriptorSet = descriptorSets[i];

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
            .dstSet = frames[i].descriptorSet,
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
}

void RenderEngine::initGeometryBuffers()
{
    // load cube mesh
    std::filesystem::path boxPath = std::filesystem::current_path() / std::filesystem::path("Assets/BoxTextured.glb");
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
    std::array<VkVertexInputAttributeDescription, 4> vertexAttribDesc = Vertex::getInputAttributeDescription();

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
    dsInfo.depthTestEnable = VK_TRUE; // enable depth test
    dsInfo.depthWriteEnable = VK_TRUE; // enable depth buffer writes
    dsInfo.depthCompareOp = VK_COMPARE_OP_LESS;
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

    // make push constants
    VkPushConstantRange pushConstants{
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = static_cast<uint32_t>(sizeof(PushConstants))
    };

    // make basic pipeline layout with push constants and descriptors
    std::array<VkDescriptorSetLayout, 2> descriptorSetLayouts;
    descriptorSetLayouts[0] = globalSceneDataLayout;
    descriptorSetLayouts[1] = materialLayout;

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pNext = nullptr;
    layoutInfo.flags = 0;
    layoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
    layoutInfo.pSetLayouts = descriptorSetLayouts.data();
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstants;

    VK_Check(vkCreatePipelineLayout(device->device, &layoutInfo, nullptr, &graphicsPipelineLayout));

    // rendering create info
    VkPipelineRenderingCreateInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderInfo.pNext = nullptr;
    renderInfo.viewMask = 0;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachmentFormats = &device->swapchain.swapchainFormat;
    renderInfo.depthAttachmentFormat = depthImage.format;
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
        device->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline));

    // Shader modules can be destroyed after the pipeline is created
    vkDestroyShaderModule(device->device, vertShader, nullptr);
    vkDestroyShaderModule(device->device, fragShader, nullptr);
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

struct ScopedGLTFData
{
    cgltf_data* data;

    cgltf_data* operator->() const { return data; }

    ~ScopedGLTFData()
    {
        cgltf_free(data);
    }
};

void RenderEngine::initMaterialDescriptor(Material& material)
{
    // allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = globalDescriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &materialLayout
    };

    VK_Check(vkAllocateDescriptorSets(device->device, &allocInfo, &material.descriptorSet));

    // write to descriptor set
    VkDescriptorImageInfo baseColorInfo{
        .sampler = material.baseColorTex.sampler,
        .imageView = material.baseColorTex.view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkDescriptorImageInfo metalRoughInfo{
        .sampler = material.metalRoughTex.sampler,
        .imageView = material.metalRoughTex.view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    std::array<VkWriteDescriptorSet, 2> writeSets;

    writeSets[0] = VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = material.descriptorSet,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &baseColorInfo,
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr
    };

    writeSets[1] = VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = material.descriptorSet,
        .dstBinding = 1,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &metalRoughInfo,
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr
    };

    vkUpdateDescriptorSets(device->device, static_cast<uint32_t>(writeSets.size()), writeSets.data(), 0, nullptr);
}

// returns a blank white 1x1 texture
Texture RenderEngine::loadWhiteTexture()
{
    Texture whiteTex;

    uint32_t whiteData = glm::packUnorm4x8(glm::vec4(1.f, 1.f, 1.f, 1.f));
    VkDeviceSize whiteDataSize = static_cast<VkDeviceSize>(1 * 1 * STBI_rgb_alpha);

    gfx::AllocatedBuffer stagingBuffer = gfx::createAllocatedBuffer(
        device.get(), whiteDataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
    );

    gfx::writeToAllocatedBuffer(device.get(), &whiteData, whiteDataSize, stagingBuffer);

    whiteTex.image = gfx::createAllocatedImage(
        device.get(), VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VK_FORMAT_R8G8B8A8_UNORM, VkExtent2D{ 1, 1 }
    );

    VkImageSubresourceLayers subresource{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = 1
    };

    VkCommandBuffer cmd = startImmediateCommands();
    copyBufferToImage(
        cmd,
        stagingBuffer, whiteTex.image,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_ASPECT_COLOR_BIT
    );
    transitionImageLayoutCoarse(
        cmd, whiteTex.image.image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_IMAGE_ASPECT_COLOR_BIT
    );
    endAndSubmitImmediateCommands();

    gfx::destroyAllocatedBuffer(device.get(), stagingBuffer);

    whiteTex.view = createImageView(
        device.get(), whiteTex.image.image,
        VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT
    );

    whiteTex.sampler = createSampler(
        device.get(), VK_FILTER_NEAREST, VK_FILTER_NEAREST,
        VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT
    );

    return whiteTex;
}

Texture RenderEngine::loadTexture(cgltf_texture* texture)
{
    Texture resultTex;

    cgltf_image* image = texture->image;
    cgltf_buffer_view* bufferView = image->buffer_view;
    cgltf_buffer* buffer = bufferView->buffer;
    const uint8_t* data = static_cast<uint8_t*>(buffer->data) + bufferView->offset;

    int width, height, nChannels;
    stbi_uc* imageData = stbi_load_from_memory(
        data, static_cast<int>(bufferView->size),
        &width, &height, &nChannels, STBI_rgb_alpha
    );

    VkDeviceSize imageDataSize = static_cast<VkDeviceSize>(width * height * STBI_rgb_alpha);

    gfx::AllocatedBuffer stagingBuffer = gfx::createAllocatedBuffer(
        device.get(), imageDataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
    );

    gfx::writeToAllocatedBuffer(device.get(), imageData, imageDataSize, stagingBuffer);

    resultTex.image = gfx::createAllocatedImage(
        device.get(), VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VK_FORMAT_R8G8B8A8_UNORM, VkExtent2D{ static_cast<uint32_t>(width), static_cast<uint32_t>(height) }
    );

    VkImageSubresourceLayers subresource{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = 1
    };

    VkCommandBuffer cmd = startImmediateCommands();
    copyBufferToImage(
        cmd,
        stagingBuffer, resultTex.image,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_ASPECT_COLOR_BIT
    );
    transitionImageLayoutCoarse(
        cmd, resultTex.image.image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_IMAGE_ASPECT_COLOR_BIT
    );
    endAndSubmitImmediateCommands();

    gfx::destroyAllocatedBuffer(device.get(), stagingBuffer);

    resultTex.view = createImageView(
        device.get(), resultTex.image.image,
        VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT
    );

    cgltf_sampler* sampler = texture->sampler;

    VkFilter magFilter;
    switch (sampler->mag_filter)
    {
    case cgltf_filter_type_nearest:
        magFilter = VK_FILTER_NEAREST;
        break;
    case cgltf_filter_type_linear:
    default:
        magFilter = VK_FILTER_LINEAR;
        break;
    }

    VkFilter minFilter;
    switch (sampler->min_filter)
    {
    case cgltf_filter_type_nearest:
    case cgltf_filter_type_nearest_mipmap_nearest:
    case cgltf_filter_type_nearest_mipmap_linear:
        minFilter = VK_FILTER_NEAREST;
        break;
    case cgltf_filter_type_linear:
    case cgltf_filter_type_linear_mipmap_nearest:
    case cgltf_filter_type_linear_mipmap_linear:
    default:
        minFilter = VK_FILTER_LINEAR;
        break;
    }

    VkSamplerAddressMode uWrap{};
    switch (sampler->wrap_s)
    {
    case cgltf_wrap_mode_clamp_to_edge:
        uWrap = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        break;
    case cgltf_wrap_mode_mirrored_repeat:
        uWrap = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        break;
    case cgltf_wrap_mode_repeat:
        uWrap = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        break;
    }

    VkSamplerAddressMode vWrap{};
    switch (sampler->wrap_t)
    {
    case cgltf_wrap_mode_clamp_to_edge:
        vWrap = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        break;
    case cgltf_wrap_mode_mirrored_repeat:
        vWrap = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        break;
    case cgltf_wrap_mode_repeat:
        vWrap = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        break;
    }

    resultTex.sampler = createSampler(
        device.get(), magFilter, minFilter, uWrap, vWrap
    );

    return resultTex;
}

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

    // load in single mesh
    cgltf_mesh* gltfMesh = &gltfData->meshes[0];

    StaticMesh newMesh;
    newMesh.surfaces.reserve(gltfMesh->primitives_count);

    // sratch buffer data
    std::vector<uint8_t> indexData;

    std::vector<float> positionData;
    std::vector<float> normalData;
    std::vector<float> uvData;
    std::vector<float> colorData;

    std::vector<Vertex> vertexData;

    for (cgltf_size i = 0; i < gltfMesh->primitives_count; i++)
    {
        MeshSurface newSurface;
        cgltf_primitive* surface = &gltfMesh->primitives[i];

        // get primative topology
        {
            // TODO: Implement rendering all topology types
            switch (surface->type)
            {
            case cgltf_primitive_type_triangles:
                newSurface.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
                break;
            default:
                SDL_LogError(
                    0, "Mesh load error: Mesh primitive contains invalid topology: %s\n", meshPath);
                SDL_LogError(0, "GLTF topology code: %i\n", surface->type);
                return std::nullopt;
            }
        }

        // load material
        {
            newSurface.material.constants = MaterialConstants{
                .baseColorFactor = glm::vec4{1.f, 1.f, 1.f, 1.f},
                .metalnessFactor = 0.f,
                .roughnessFactor = 1.f
            };
            if (surface->material && surface->material->has_pbr_metallic_roughness)
            {
                cgltf_pbr_metallic_roughness& pbrMetalRough 
                    = surface->material->pbr_metallic_roughness;

                newSurface.material.constants.baseColorFactor 
                    = glm::make_vec4(pbrMetalRough.base_color_factor);
                newSurface.material.constants.metalnessFactor = pbrMetalRough.metallic_factor;
                newSurface.material.constants.roughnessFactor = pbrMetalRough.roughness_factor;

                // load base color texture
                if (pbrMetalRough.base_color_texture.texture)
                {
                    newSurface.material.baseColorTex = loadTexture(
                        pbrMetalRough.base_color_texture.texture
                    );
                }
                else
                {
                    newSurface.material.baseColorTex = loadWhiteTexture();
                }

                // load metallic roughness texture
                if (pbrMetalRough.metallic_roughness_texture.texture)
                {
                    newSurface.material.metalRoughTex = loadTexture(
                        pbrMetalRough.metallic_roughness_texture.texture
                    );
                }
                else
                {
                    newSurface.material.metalRoughTex = loadWhiteTexture();
                }
            }

            initMaterialDescriptor(newSurface.material);
        }

        // load index buffer
        // TODO: Support non index geometry
        {
            if (surface->indices == nullptr)
            {
                SDL_LogError(0, "Mesh load error: Surface missing indices: %s\n", meshPath);
                return std::nullopt;
            }

            cgltf_size outComponentSize = cgltf_component_size(surface->indices->component_type);
            cgltf_size indexCount = cgltf_accessor_unpack_indices(surface->indices, nullptr, 0, 0);
            indexData.resize(outComponentSize * indexCount);

            // base vulkan only supports 16u and 32u indices
            switch (surface->indices->component_type)
            {
            case cgltf_component_type_r_16u:
                newSurface.indexType = VK_INDEX_TYPE_UINT16;
                break;
            case cgltf_component_type_r_32u:
                newSurface.indexType = VK_INDEX_TYPE_UINT32;
                break;
            default:
                SDL_LogError(
                    0, "Mesh load error: Mesh primitive contains invalid index format: %s\n", meshPath);
                SDL_LogError(0, "GLTF index code: %i\n", surface->indices->component_type);
                return std::nullopt;
            }

            cgltf_accessor_unpack_indices(surface->indices, indexData.data(), outComponentSize, indexCount);

            newSurface.indexCount = static_cast<uint32_t>(indexCount);

            VkDeviceSize dataSize = static_cast<VkDeviceSize>(outComponentSize * indexCount);

            // create and upload gpu buffer
            gfx::AllocatedBuffer stagingBuffer = gfx::createAllocatedBuffer(
                device.get(), dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
            );

            gfx::writeToAllocatedBuffer(device.get(), indexData.data(), dataSize, stagingBuffer);

            newSurface.indexBuffer = gfx::createAllocatedBuffer(
                device.get(), dataSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 0
            );

            // copy staging to gpu only buffer
            VkCommandBuffer cmd = startImmediateCommands();
            copyBufferToBuffer(cmd, stagingBuffer, newSurface.indexBuffer, dataSize);
            endAndSubmitImmediateCommands();

            gfx::destroyAllocatedBuffer(device.get(), stagingBuffer);
        }

        // load vertex buffer
        {
            // position
            {
                const cgltf_accessor* positionAcc =
                    cgltf_find_accessor(surface, cgltf_attribute_type_position, 0);

                if (positionAcc == nullptr)
                {
                    SDL_LogError(0, "Mesh load error: Surface position attrib invalid: %s\n", meshPath);
                    return std::nullopt;
                }

                cgltf_size positionCount = cgltf_accessor_unpack_floats(positionAcc, nullptr, 0);

                positionData.resize(positionCount);
                cgltf_accessor_unpack_floats(positionAcc, positionData.data(), positionCount);
            }

            // normal
            bool hasNormal = false;
            {
                const cgltf_accessor* normalAcc =
                    cgltf_find_accessor(surface, cgltf_attribute_type_normal, 0);

                if (normalAcc != nullptr)
                {
                    hasNormal = true;
                    cgltf_size normalCount = cgltf_accessor_unpack_floats(normalAcc, nullptr, 0);

                    normalData.resize(normalCount);
                    cgltf_accessor_unpack_floats(normalAcc, normalData.data(), normalCount);
                }
            }

            // uv
            bool hasUv = false;
            {
                const cgltf_accessor* uvAcc =
                    cgltf_find_accessor(surface, cgltf_attribute_type_texcoord, 0);

                if (uvAcc != nullptr)
                {
                    hasUv = true;
                    cgltf_size uvCount = cgltf_accessor_unpack_floats(uvAcc, nullptr, 0);

                    uvData.resize(uvCount);
                    cgltf_accessor_unpack_floats(uvAcc, uvData.data(), uvCount);
                }
            }

            // color
            cgltf_size colorChannels = 0;
            {
                const cgltf_accessor* colorAcc =
                    cgltf_find_accessor(surface, cgltf_attribute_type_color, 0);

                if (colorAcc != nullptr)
                {
                    colorChannels = cgltf_num_components(colorAcc->type);
                    cgltf_size colorCount = cgltf_accessor_unpack_floats(colorAcc, nullptr, 0);

                    colorData.resize(colorCount);
                    cgltf_accessor_unpack_floats(colorAcc, colorData.data(), colorCount);
                }
            }

            // assemble attribs
            size_t vertexCount = positionData.size() / 3;
            {
                vertexData.resize(vertexCount);
                for (size_t i = 0; i < vertexCount; i++)
                {
                    vertexData[i].position = glm::make_vec3(&positionData[3 * i]);

                    if (hasNormal)
                        vertexData[i].normal = glm::make_vec3(&normalData[3 * i]);
                    else
                        vertexData[i].normal = glm::vec3{ 0.f, 0.f, 1.f };

                    if (hasUv)
                        vertexData[i].uv = glm::make_vec2(&uvData[2 * i]);
                    else
                        vertexData[i].uv = glm::vec2{ 0.f, 0.f };

                    if (colorChannels == 4)
                        vertexData[i].color = glm::make_vec4(&colorData[4 * i]);
                    else if (colorChannels == 3)
                        vertexData[i].color = glm::vec4{ glm::make_vec3(&colorData[3 * i]), 1.f };
                    else
                        vertexData[i].color = glm::vec4{ 1.f, 1.f, 1.f, 1.f };
                }
            }

            newSurface.vertexCount = static_cast<uint32_t>(vertexCount);

            VkDeviceSize dataSize = static_cast<VkDeviceSize>(sizeof(vertexData[0]) * vertexCount);

            // create and upload gpu buffer
            gfx::AllocatedBuffer stagingBuffer = gfx::createAllocatedBuffer(
                device.get(), dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
            );

            gfx::writeToAllocatedBuffer(device.get(), vertexData.data(), dataSize, stagingBuffer);

            newSurface.vertexBuffer = gfx::createAllocatedBuffer(
                device.get(), dataSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 0
            );

            // copy staging to gpu only buffer
            VkCommandBuffer cmd = startImmediateCommands();
            copyBufferToBuffer(cmd, stagingBuffer, newSurface.vertexBuffer, dataSize);
            endAndSubmitImmediateCommands();

            gfx::destroyAllocatedBuffer(device.get(), stagingBuffer);
        }

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

std::array<VkVertexInputAttributeDescription, 4> Vertex::getInputAttributeDescription()
{
    std::array<VkVertexInputAttributeDescription, 4> attribDesc;

    // position attrib
    attribDesc[0].location = 0;
    attribDesc[0].binding = 0;
    attribDesc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribDesc[0].offset = offsetof(Vertex, position);

    // normal attrib
    attribDesc[1].location = 1;
    attribDesc[1].binding = 0;
    attribDesc[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribDesc[1].offset = offsetof(Vertex, normal);

    // uv attrib
    attribDesc[2].location = 2;
    attribDesc[2].binding = 0;
    attribDesc[2].format = VK_FORMAT_R32G32_SFLOAT;
    attribDesc[2].offset = offsetof(Vertex, uv);

    // color attrib
    attribDesc[3].location = 3;
    attribDesc[3].binding = 0;
    attribDesc[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attribDesc[3].offset = offsetof(Vertex, color);

    return attribDesc;
}

} // namespace gfx