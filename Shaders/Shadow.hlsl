struct GlobalSceneData
{
    float4x4 view;
    float4x4 viewproj;
    
    float4x4 shadowMatrix;
    
    float3 viewPosition;
    float padding1;
    
    float3 lightDirection;
    float padding2;
    
    float3 lightColor;
    float padding3;
};

[[vk::binding(0, 0)]]
ConstantBuffer<GlobalSceneData> globalSceneData;

[[vk::binding(0, 1)]]
Texture2D baseColorTex;

[[vk::binding(0, 1)]]
SamplerState baseColorSampler;

struct PushConstants
{
    float4x4 model;
    float alphaCutoff;
};

[[vk::push_constant]]
ConstantBuffer<PushConstants> pushConstants;

struct VertexInput
{
    [[vk::location(0)]] float3 position : POSITION0;
    [[vk::location(1)]] float3 normal : NORMAL0;
    [[vk::location(2)]] float2 uv : TEXCOORD0;
    [[vk::location(3)]] float4 color : COLOR0;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0; // needed for alpha cutoff
    float4 color : COLOR0; // needed for alpha cutoff
};

// use hardware depth output
//struct PixelOutput
//{
//    float depth : SV_Depth;
//};

VertexOutput shadowVS(VertexInput input)
{
    VertexOutput output;
    output.position = mul(mul(globalSceneData.shadowMatrix, pushConstants.model), float4(input.position, 1.f));
    output.uv = input.uv;
    output.color = input.color;
    return output;
}

void shadowPS(VertexOutput input)
{
    float baseColorAlpha = baseColorTex.Sample(baseColorSampler, input.uv).a;
    
    // evaluate if alpha cutout should be split into a different pipeline
    // (due to introducing depth pixel shader)
    float alpha = input.color.a * baseColorAlpha;
    clip(alpha - pushConstants.alphaCutoff);
}