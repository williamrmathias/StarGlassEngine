[[vk::binding(0, 0)]]
Texture2D hdrColorBuffer;

[[vk::binding(0, 0)]]
SamplerState hdrColorSampler;

struct VertexInput
{
    [[vk::location(0)]] uint vertexID : SV_VertexID;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

struct PixelOutput
{
    float4 color : SV_Target;
};

VertexOutput screenSpaceVS(VertexInput input)
{
    const float2 fullScreenTriangle[3] = {
        float2(-1.f, -1.f),
        float2(-1.f, 3.f),
        float2(3.f, -1.f)
    };
    
    VertexOutput output;
    output.position = float4(fullScreenTriangle[input.vertexID], 0.f, 1.f);
    
    // Map NDC [-1, 1] to UV space [0, 1]
    output.uv = output.position.xy * 0.5f + 0.5f;
    
	return output;
}

PixelOutput toneMapPS(VertexOutput input)
{
    PixelOutput result;
    result.color = hdrColorBuffer.Sample(hdrColorSampler, input.uv);
    
    return result;
}