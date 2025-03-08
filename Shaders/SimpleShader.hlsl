struct VertexInput
{
    [[vk::location(0)]] float2 position : POSITION0;
    [[vk::location(1)]] float3 color : COLOR0;
};

struct VertexOutput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float3 color : COLOR0;
};

struct PixelOutput
{
    float4 color : SV_Target;
};

VertexOutput simpleVS(VertexInput input)
{
    VertexOutput output;
    output.position = float4(input.position, 0.f, 1.f);
    output.color = input.color;
	return output;
}

PixelOutput simplePS(VertexOutput input)
{
    PixelOutput result;
    result.color = float4(input.color, 1.f);
    return result;
}