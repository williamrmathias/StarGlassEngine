#pragma once

// sge
#include "Device.h"

// Vulkan
#include <vulkan/vulkan.h>

// stl
#include <vector>
#include <span>
#include <string_view>

namespace gfx
{

struct Pipeline
{
    VkPipeline pipeline;
    VkPipelineLayout layout;

    void cleanup(Device* device);
};

enum class BlendState : uint8_t
{
    Disabled,
    Over // out.rgb = (src.rgb * src.a) + (dst.rgb * (1-src.a)); out.a = src.a + dst.a * (1-src.a)
};

class GraphicsPipelineBuilder
{
public:
    GraphicsPipelineBuilder();

    Pipeline build(Device* device);

    void clear();

    // rendering attachments
    void setColorAttachmentFormats(std::span<const VkFormat> colorFormats);
    void setDepthAttachmentFormat(VkFormat depthFromat);

    // rasterization info
    void setShaderStages(
        VkShaderModule vertexShader, std::string_view vertexEntryName,
        VkShaderModule fragmentShader, std::string_view fragmentEntryName
    );

    void setVertexInputState(
        std::span<VkVertexInputBindingDescription> bindingDesc,
        std::span<VkVertexInputAttributeDescription> attribDesc
    );

    void setPrimitiveTopology(VkPrimitiveTopology topology);

    void setPolygonMode(VkPolygonMode polygonMode);

    void setSampleCount(VkSampleCountFlagBits sampleCount);

    void setDepthMode(VkBool32 depthTestEnable, VkBool32 depthWriteEnable);
    void setDepthFunc(VkCompareOp func);

    void setCullMode(VkCullModeFlags cullMode) { rasterInfo.cullMode = cullMode; }

    void setBlendMode(std::span<BlendState> blendState);

    // Shader input data layouts
    void setPushConstantSize(uint32_t size);
    void setDescriptorSetLayouts(std::span<VkDescriptorSetLayout> layouts);

private:
    void init();

    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    std::string vertexShaderName;
    std::string fragmentShaderName;

    std::vector<VkVertexInputBindingDescription> vertexBindingDesc;
    std::vector<VkVertexInputAttributeDescription> vertexAttribDesc;
    std::vector<VkFormat> colorAttachmentFormats;
    std::vector<VkPipelineColorBlendAttachmentState> blendAttachmentStates;

    VkPushConstantRange pushConstants;
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;

    VkPipelineRenderingCreateInfo renderInfo;
    VkPipelineVertexInputStateCreateInfo vertexInputInfo;
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
    VkPipelineViewportStateCreateInfo viewportInfo; // leave null - use dynamic viewport
    VkPipelineRasterizationStateCreateInfo rasterInfo;
    VkPipelineMultisampleStateCreateInfo multisampleInfo;
    VkPipelineDepthStencilStateCreateInfo depthStencilInfo;
    VkPipelineColorBlendStateCreateInfo blendInfo;
    VkPipelineLayoutCreateInfo layoutInfo;
};

} // namespace gfx