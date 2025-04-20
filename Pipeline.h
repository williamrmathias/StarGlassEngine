#pragma once

// Vulkan
#include <vulkan/vulkan.h>

// stl
#include <vector>
#include <span>

namespace gfx
{

struct Pipeline
{
    VkPipeline pipeline;
    VkPipelineLayout layout;
};

class GraphicsPipelineBuilder
{
public:
    GraphicsPipelineBuilder();

    Pipeline build();

    void setShaderStages(VkShaderModule vertexShader, VkShaderModule fragmentShader);

    void setVertexInputState(
        VkVertexInputBindingDescription bindingDesc,
        std::span<VkVertexInputAttributeDescription> attribDesc
    );

    void setPrimitiveTopology(VkPrimitiveTopology topology);

    void setPolygonMode(VkPolygonMode polygonMode);

    void setSampleCount(VkSampleCountFlags sampleCount);

    void setDepthMode(VkBool32 depthTestEnable, VkBool32 depthWriteEnable);

    void setBlendMode(VkBool32 blendEnable);

    // Shader input data layouts
    void setPushConstantSize(uint32_t size);
    void setDescriptorSetLayouts(std::span<VkDescriptorSetLayout> layouts);

    // rendering attachments
    void setColorAttachmentFormats(std::span<VkFormat> colorFormats);
    void setDepthAttachmentFormat(VkFormat depthFromat);

private:
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

    VkPipelineVertexInputStateCreateInfo vertexInputInfo;
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
    VkPipelineViewportStateCreateInfo viewportInfo; // leave null - use dynamic viewport
    VkPipelineRasterizationStateCreateInfo rasterInfo;
    VkPipelineMultisampleStateCreateInfo multisampleInfo;
    VkPipelineDepthStencilStateCreateInfo depthStencilInfo;
    VkPipelineColorBlendStateCreateInfo blendInfo;
    VkPipelineLayoutCreateInfo layoutInfo;
    VkPipelineRenderingCreateInfo renderInfo;
};

} // namespace gfx