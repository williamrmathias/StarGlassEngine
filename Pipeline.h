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

    // rendering attachments
    void setColorAttachmentFormats(std::span<VkFormat> colorFormats);
    void setDepthAttachmentFormat(VkFormat depthFromat);

    // rasterization info
    void setShaderStages(VkShaderModule vertexShader, VkShaderModule fragmentShader);

    void setVertexInputState(
        std::span<VkVertexInputBindingDescription> bindingDesc,
        std::span<VkVertexInputAttributeDescription> attribDesc
    );

    void setPrimitiveTopology(VkPrimitiveTopology topology);

    void setPolygonMode(VkPolygonMode polygonMode);

    void setSampleCount(VkSampleCountFlagBits sampleCount);

    void setDepthMode(VkBool32 depthTestEnable, VkBool32 depthWriteEnable);

    void disableBlendMode();
    void setBlendMode(std::span<VkBool32> blendEnable);

    // Shader input data layouts
    void setPushConstantSize(uint32_t size);
    void setDescriptorSetLayouts(std::span<VkDescriptorSetLayout> layouts);

private:
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    std::vector<VkVertexInputBindingDescription> vertexBindingDesc;
    std::vector<VkVertexInputAttributeDescription> vertexAttribDesc;
    std::vector<VkFormat> colorAttachmentFormats;
    std::vector<VkPipelineColorBlendAttachmentState> blendAttachmentStates;

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