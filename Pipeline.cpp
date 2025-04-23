// sge
#include "Pipeline.h"
#include "Log.h"

// stl
#include <array>
#include <cassert>

namespace gfx 
{

void Pipeline::cleanup(Device* device)
{
    vkDestroyPipelineLayout(device->device, layout, nullptr);
    vkDestroyPipeline(device->device, pipeline, nullptr);
}

GraphicsPipelineBuilder::GraphicsPipelineBuilder()
{
    init();
}

Pipeline GraphicsPipelineBuilder::build(Device* device)
{
    Pipeline pipeline;

    // create layout
    VK_Check(vkCreatePipelineLayout(device->device, &layoutInfo, nullptr, &pipeline.layout));

    // set dynamic state
    // viewport and scissor
    std::array<VkDynamicState, 2> dynamicStates{
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data()
    };

    // create graphics pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &renderInfo,
        .flags = 0,
        .stageCount = static_cast<uint32_t>(shaderStages.size()),
        .pStages = shaderStages.data(),
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssemblyInfo,
        .pTessellationState = nullptr,
        .pViewportState = &viewportInfo,
        .pRasterizationState = &rasterInfo,
        .pMultisampleState = &multisampleInfo,
        .pDepthStencilState = &depthStencilInfo,
        .pColorBlendState = &blendInfo,
        .pDynamicState = &dynamicInfo,
        .layout = pipeline.layout,
        .renderPass = VK_NULL_HANDLE, // use dynamic rendering
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = 0
    };

    VK_Check(vkCreateGraphicsPipelines(
        device->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline.pipeline
    ));

    return pipeline;
}

void GraphicsPipelineBuilder::clear()
{
    shaderStages.clear();
    vertexBindingDesc.clear();
    vertexAttribDesc.clear();
    colorAttachmentFormats.clear();
    blendAttachmentStates.clear();

    pushConstants = {};
    descriptorSetLayouts.clear();

    init();
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

void GraphicsPipelineBuilder::setShaderStages(
    VkShaderModule vertexShader, std::string_view vertexEntryName,
    VkShaderModule fragmentShader, std::string_view fragmentEntryName
)
{
    VkPipelineShaderStageCreateInfo stageInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pSpecializationInfo = nullptr
    };

    {
        // vertex
        stageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        stageInfo.module = vertexShader;
        stageInfo.pName = vertexEntryName.data();
        shaderStages.push_back(stageInfo);
    }

    {
        // fragment
        stageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stageInfo.module = fragmentShader;
        stageInfo.pName = fragmentEntryName.data();
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
    assert(size <= 128);
    pushConstants.size = size;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstants;
}

void GraphicsPipelineBuilder::setDescriptorSetLayouts(std::span<VkDescriptorSetLayout> layouts)
{
    descriptorSetLayouts.assign(layouts.begin(), layouts.end());
    layoutInfo.setLayoutCount = descriptorSetLayouts.size();
    layoutInfo.pSetLayouts = descriptorSetLayouts.data();
}

void GraphicsPipelineBuilder::init()
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

    // init push constants
    // default size 0
    pushConstants = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = 0
    };
}

} // namespace gfx