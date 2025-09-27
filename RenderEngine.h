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
static const uint32_t NUM_MATERIALS_MAX = 1000;

struct GlobalSceneData
{
    glm::mat4 view;
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

// must be 128 bytes
struct ScreenSpacePushConstants
{
    float exposure;
    float padding[31];
};

// must be 128 bytes
struct SkyboxPushConstants
{
    glm::mat4 viewproj;
    float padding[16];
};

class RenderEngine
{
public:

    std::unique_ptr<Device> device;
    std::unique_ptr<LoadedGltf> loadedGltf;

    VkCommandPool immediateCommandPool;
    VkCommandBuffer immediateCommandBuffer;
    VkFence immediateFence;

    AllocatedImage depthImage;
    VkImageView depthView;

    AllocatedImage skybox;

    VkSampler screenSpaceSampler;

    VkDescriptorPool globalDescriptorPool;
    VkDescriptorSetLayout globalSceneDataLayout;
    VkDescriptorSetLayout materialLayout;
    VkDescriptorSetLayout screenSpaceLayout;
    VkDescriptorSetLayout cubemapLayout;

    struct FrameData
    {
        VkSemaphore swapchainSemaphore;
        VkSemaphore renderSemaphore;
        VkFence renderFence;

        VkCommandPool commandPool;
        VkCommandBuffer commandBuffer;

        AllocatedBuffer uniformBuffer;
        VkDescriptorSet globalDescriptorSet;

        AllocatedImage hdrColorImage;
        VkImageView hdrColorView;
        VkDescriptorSet screenSpaceDescriptorSet;

        void cleanup(Device* device);
    };
    FrameData frames[NUM_FRAMES];
    size_t currentFrameNumber = 0;

    enum class PipelineType : uint8_t
    {
        // main pass pipelines
        MainGraphics,
        BaseColorDebug,
        MetalDebug,
        RoughDebug,
        NormalDebug,
        VertexNormalDebug,
        UvDebug,

        // screen space pipelines
        ToneMap,
        PassThrough
    };

    Pipeline activePipeline;
    Pipeline graphicsPipeline;

    // debug material pipelines
    Pipeline baseColorPipeline;
    Pipeline metalPipeline;
    Pipeline roughPipeline;
    Pipeline normalPipeline;
    Pipeline vertNormalPipeline;
    Pipeline uvPipeline;

    Pipeline activeSSPipeline;
    Pipeline toneMapPipeline;
    Pipeline passThroughPipeline;

    Pipeline skyboxPipeline;
    Pipeline irradiancePipeline;
    Pipeline skyPipeline;

    void init(SDL_Window* window);
    void render();
    void cleanup();

    void setSunDirection(float azimuth, float altitude);
    void setSunLuminance(float luminance);
    void setViewMatrix(const glm::mat4 view);
    void setViewPosition(const glm::vec3 viewPosition);
    void setActiveMainPassPipeline(PipelineType pipeline);
    void setActiveScreenSpacePipeline(PipelineType pipeline);
    void setExposure(float exposureIn) { exposure = exposureIn; }

    VkCommandBuffer startImmediateCommands();
    void endAndSubmitImmediateCommands();

    void renderSkyboxFace(VkCommandBuffer cmd, VkImageView colorAttachView, VkDescriptorSet hdrEquirecDescriptor, glm::mat4 viewproj, uint32_t renderExtent) const;
    void renderIrradianceMapFace(VkCommandBuffer cmd, VkImageView colorAttachView, VkDescriptorSet skyboxDescriptor, glm::mat4 viewproj, uint32_t renderExtent) const;

private:
    GlobalSceneData globalSceneData;
    float exposure = 1.f;

    struct RenderTarget
    {
        gfx::AllocatedImage image;
        VkImageView view;
    };

    RenderTarget createHDRColorTarget() const;
    void initDepthTarget();
    void initDescriptorPool();
    void initImmediateStructures();
    void initFrameData();
    void initGraphicsPipelines();
    void initScreenSpacePipelines();
    void initSkyboxPipeline();
    void initIrradianceConvolutionPipeline();
    void initSkyPipeline();
    void initImGui(SDL_Window* window);
    void initScene();

    void drawScene(VkCommandBuffer cmd);

    void renderSky(VkCommandBuffer cmd, VkImageView colorAttachView, VkImageView depthAttachView, VkExtent2D renderExtent);
    void renderPostFX(VkCommandBuffer cmd, FrameData& frame, VkImageView colorAttachView, VkExtent2D renderExtent);
    void renderImGui(VkCommandBuffer cmd, VkImageView colorAttachView, VkExtent2D renderExtent);

    FrameData& getCurrentFrameData();
    void incrementFrameData();
    VkShaderModule loadShaderModule(const char* shaderPath);
};

} // namespace gfx