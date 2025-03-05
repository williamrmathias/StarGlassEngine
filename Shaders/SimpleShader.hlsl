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

VertexOutput simpleVS(VertexInput input)
{
    VertexOutput output;
    output.position = float4(input.position, 0.f, 1.f);
    output.color = input.color;
	return output;
}

float4 simplePS(VertexOutput input)
{
    return float4(input.color, 1.f);
}