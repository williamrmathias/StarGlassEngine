#pragma once

// sge
#include "Device.h"
#include "Resource.h"
#include "Pipeline.h"
#include "GLTFLoader.h"

// Tell SDL not to mess with main()
#define SDL_MAIN_HANDLED

// GLM
#include <glm/glm.hpp>

// SDL
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_vulkan.h>
#include <SDL2/SDL_filesystem.h>

// imgui
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>

// cgltf
#include <cgltf.h>

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

namespace gfx
{

static const size_t NUM_FRAMES = 2;
static const uint32_t NUM_MATERIALS_MAX = 10;

struct GlobalSceneData
{
    glm::mat4 viewproj;

    glm::vec3 viewPosition;
    float padding1;

    glm::vec3 lightDirection;
    float padding2;

    glm::vec3 lightColor;
    float padding3;
};

// must be 128 bytes
struct PushConstants
{
    glm::mat4 model;
    MaterialConstants material;

    float padding[10];
};

class RenderEngine
{
public:

    std::unique_ptr<Device> device;
    std::unique_ptr<LoadedGltf> LoadedGltf;

    VkCommandPool immediateCommandPool;
    VkCommandBuffer immediateCommandBuffer;
    VkFence immediateFence;

    AllocatedImage colorImage;
    VkImageView colorView;

    AllocatedImage depthImage;
    VkImageView depthView;

    VkDescriptorPool globalDescriptorPool;
    VkDescriptorSetLayout globalSceneDataLayout;
    VkDescriptorSetLayout materialLayout;

    struct FrameData
    {
        VkSemaphore swapchainSemaphore;
        VkSemaphore renderSemaphore;
        VkFence renderFence;

        VkCommandPool commandPool;
        VkCommandBuffer commandBuffer;

        AllocatedBuffer uniformBuffer;

        VkDescriptorSet descriptorSet;

        void cleanup(Device* device);
    };
    FrameData frames[NUM_FRAMES];
    size_t currentFrameNumber = 0;

    enum class PipelineType : uint8_t
    {
        MainGraphics,
        BaseColorDebug,
        MetalDebug,
        RoughDebug,
        NumPipelineTypes
    };

    Pipeline activePipeline;

    Pipeline graphicsPipeline;

    // debug pipelines
    Pipeline baseColorPipeline;
    Pipeline metalPipeline;
    Pipeline roughPipeline;

    void init(SDL_Window* window);
    void render();
    void cleanup();

    void setSunDirection(float azimuth, float altitude);
    void setActiveDrawPipeline(PipelineType pipeline);

    GlobalSceneData& getGlobalSceneData() { return globalSceneData; }

    VkCommandBuffer startImmediateCommands();
    void endAndSubmitImmediateCommands();

private:
    GlobalSceneData globalSceneData;

    void initColorTarget();
    void initDepthTarget();
    void initDescriptorPool();
    void initGlobalSceneData();
    void initImmediateStructures();
    void initFrameData();
    void initGeometryBuffers();
    void initGraphicsPipelines();
    void initImGui(SDL_Window* window);

    void renderImGui(VkCommandBuffer cmd, VkImageView colorAttachView, VkExtent2D renderExtent);

    FrameData& getCurrentFrameData();
    void incrementFrameData();
    VkShaderModule loadShaderModule(const char* shaderPath);
};

} // namespace gfx