// sge
#include "Pipeline.h"

// stl
#include <array>
#include <cassert>

namespace gfx 
{

GraphicsPipelineBuilder::GraphicsPipelineBuilder()
{
    // init rendering info
    // default no color, depth, or stencil attachments
    renderInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext = nullptr,
        .viewMask = 0,
        .colorAttachmentCount = 0,
        .pColorAttachmentFormats = nullptr,
        .depthAttachmentFormat = VK_FORMAT_UNDEFINED,
        .stencilAttachmentFormat = VK_FORMAT_UNDEFINED
    };

    // init vertex input state
    vertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0
    };

    // init input assembly
    inputAssemblyInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // default to triangle list
        .primitiveRestartEnable = VK_FALSE
    };

    // init dummy viewport state
    // Note: We're using dynamic viewport and scissor state
    viewportInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .viewportCount = 1,
        .pViewports = nullptr,
        .scissorCount = 1,
        .pScissors = nullptr
    };

    // init rasterization state
    rasterInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT, // enable backface culling
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE, // disable depth bias
        .depthBiasConstantFactor = 0.f,
        .depthBiasClamp = 0.f,
        .depthBiasSlopeFactor = 0.f,
        .lineWidth = 1.f
    };

    // init multisample state
    multisampleInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT, // default to one sample
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 0.f,
        .pSampleMask = nullptr, // disable sample mask test
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE
    };

    // init depth stencil state
    depthStencilInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .depthTestEnable = VK_FALSE, // default disable depth test
        .depthWriteEnable = VK_FALSE, // default disable depth write
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE, // disable depth bounds test
        .stencilTestEnable = VK_FALSE, // disable stencil test
        .front = VkStencilOpState{},
        .back = VkStencilOpState{},
        .minDepthBounds = 0.f,
        .maxDepthBounds = 1.f
    };

    // init blend state
    // default to zero color attachments
    blendInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 0,
        .pAttachments = nullptr,
        .blendConstants = {0.f, 0.f, 0.f, 0.f}
    };

    // init pipeline layout
    // default no descriptors or push constants
    layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = 0,
        .pSetLayouts = nullptr,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = nullptr
    };
}

void GraphicsPipelineBuilder::setColorAttachmentFormats(std::span<VkFormat> colorFormats)
{
    colorAttachmentFormats.assign(colorFormats.begin(), colorFormats.end());

    renderInfo.colorAttachmentCount = colorAttachmentFormats.size();
    renderInfo.pColorAttachmentFormats = colorAttachmentFormats.data();
}

void GraphicsPipelineBuilder::setDepthAttachmentFormat(VkFormat depthFromat)
{
    renderInfo.depthAttachmentFormat = depthFromat;
}

void GraphicsPipelineBuilder::setShaderStages(VkShaderModule vertexShader, VkShaderModule fragmentShader)
{
    VkPipelineShaderStageCreateInfo stageInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pSpecializationInfo = nullptr
    };
    stageInfo.pName = nullptr; // check valid

    {
        // vertex
        stageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        stageInfo.module = vertexShader;
        shaderStages.push_back(stageInfo);
    }

    {
        // fragment
        stageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stageInfo.module = fragmentShader;
        shaderStages.push_back(stageInfo);
    }
}

void GraphicsPipelineBuilder::setVertexInputState(
    std::span<VkVertexInputBindingDescription> bindingDesc,
    std::span<VkVertexInputAttributeDescription> attribDesc
)
{
    // have to copy spans so external pointer can't become invalid
    vertexBindingDesc.assign(bindingDesc.begin(), bindingDesc.end());
    vertexAttribDesc.assign(attribDesc.begin(), attribDesc.end());

    vertexInputInfo.vertexBindingDescriptionCount = bindingDesc.size();
    vertexInputInfo.pVertexBindingDescriptions = bindingDesc.data();

    vertexInputInfo.vertexAttributeDescriptionCount = attribDesc.size();
    vertexInputInfo.pVertexAttributeDescriptions = attribDesc.data();
}

void GraphicsPipelineBuilder::setPrimitiveTopology(VkPrimitiveTopology topology)
{
    inputAssemblyInfo.topology = topology;
}

void GraphicsPipelineBuilder::setPolygonMode(VkPolygonMode polygonMode)
{
    rasterInfo.polygonMode = polygonMode;
}

void GraphicsPipelineBuilder::setSampleCount(VkSampleCountFlagBits sampleCount)
{
    // todo: Multisampling
    if (sampleCount != VK_SAMPLE_COUNT_1_BIT)
        assert("GraphicsPipelineBuilderError: Multisampled Pipelines Not Supported!");

    multisampleInfo.rasterizationSamples = sampleCount;
}

void GraphicsPipelineBuilder::setDepthMode(VkBool32 depthTestEnable, VkBool32 depthWriteEnable)
{
    if (renderInfo.depthAttachmentFormat == VK_FORMAT_UNDEFINED &&
        (depthTestEnable == VK_TRUE || depthWriteEnable == VK_TRUE))
    {
        assert("GraphicsPipelineBuilderError: Depth Test / Write enabled, but there's no depth attachment");
    }

    depthStencilInfo.depthTestEnable = depthTestEnable;
    depthStencilInfo.depthWriteEnable = depthWriteEnable;
}

void GraphicsPipelineBuilder::disableBlendMode()
{
    std::vector<VkBool32> blendModes(renderInfo.colorAttachmentCount, VK_FALSE);
    setBlendMode(blendModes);
}

void GraphicsPipelineBuilder::setBlendMode(std::span<VkBool32> blendEnable)
{
    if (blendEnable.size() != renderInfo.colorAttachmentCount)
        assert("GraphicsPipelineBuilderError: Not every color attachment has a blend state specified");

    VkPipelineColorBlendAttachmentState blendState{
        .blendEnable = VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT
            | VK_COLOR_COMPONENT_G_BIT
            | VK_COLOR_COMPONENT_B_BIT
            | VK_COLOR_COMPONENT_A_BIT
    };

    blendAttachmentStates.reserve(renderInfo.colorAttachmentCount);
    for (auto blendMode : blendEnable)
    {
        blendState.blendEnable = blendMode;
        blendAttachmentStates.emplace_back(blendState);
    }

    blendInfo.attachmentCount = renderInfo.colorAttachmentCount;
    blendInfo.pAttachments = blendAttachmentStates.data();
}

void GraphicsPipelineBuilder::setPushConstantSize(uint32_t size)
{

}

void GraphicsPipelineBuilder::setDescriptorSetLayouts(std::span<VkDescriptorSetLayout> layouts)
{

}

} // namespace gfx