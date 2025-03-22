struct GlobalSceneData
{
    float4x4 viewproj;
};

[[vk::binding(0, 0)]]
ConstantBuffer<GlobalSceneData> globalSceneData : register(b0);

struct PushConstants
{
    float4x4 model;
};

[[vk::push_constant]]
ConstantBuffer<PushConstants> pushConstants : register(b1);

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
    float4 color : COLOR0;
};

struct PixelOutput
{
    float4 color : SV_Target;
};

VertexOutput simpleVS(VertexInput input)
{
    VertexOutput output;
    float4x4 mvp = mul(globalSceneData.viewproj, pushConstants.model);
    output.position = mul(mvp, float4(input.position, 1.f));
    output.color = input.color;
	return output;
}

PixelOutput simplePS(VertexOutput input)
{
    PixelOutput result;
    result.color = input.color;
    return result;
}