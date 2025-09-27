[[vk::binding(0, 0)]]
TextureCube skybox;

[[vk::binding(0, 0)]]
SamplerState skyboxSampler;

struct PushConstants
{
    float4x4 viewproj;
};

[[vk::push_constant]]
ConstantBuffer<PushConstants> pushConstants;

struct VertexInput
{
    [[vk::location(0)]] uint vertexID : SV_VertexID;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float3 positionLocal : TEXCOORD0;
};

struct PixelOutput
{
    float4 color : SV_Target;
};

VertexOutput skyVS(VertexInput input)
{
    static const float3 unitCube[36] =
    {
        // -Z face
        float3(-0.5f, -0.5f, -0.5f),
        float3(0.5f, -0.5f, -0.5f),
        float3(0.5f, 0.5f, -0.5f),

        float3(0.5f, 0.5f, -0.5f),
        float3(-0.5f, 0.5f, -0.5f),
        float3(-0.5f, -0.5f, -0.5f),

        // +Z face
        float3(-0.5f, -0.5f, 0.5f),
        float3(0.5f, -0.5f, 0.5f),
        float3(0.5f, 0.5f, 0.5f),

        float3(0.5f, 0.5f, 0.5f),
        float3(-0.5f, 0.5f, 0.5f),
        float3(-0.5f, -0.5f, 0.5f),

        // -X face
        float3(-0.5f, -0.5f, -0.5f),
        float3(-0.5f, -0.5f, 0.5f),
        float3(-0.5f, 0.5f, 0.5f),

        float3(-0.5f, 0.5f, 0.5f),
        float3(-0.5f, 0.5f, -0.5f),
        float3(-0.5f, -0.5f, -0.5f),

        // +X face
        float3(0.5f, -0.5f, -0.5f),
        float3(0.5f, -0.5f, 0.5f),
        float3(0.5f, 0.5f, 0.5f),

        float3(0.5f, 0.5f, 0.5f),
        float3(0.5f, 0.5f, -0.5f),
        float3(0.5f, -0.5f, -0.5f),

        // -Y face
        float3(-0.5f, -0.5f, -0.5f),
        float3(0.5f, -0.5f, -0.5f),
        float3(0.5f, -0.5f, 0.5f),

        float3(0.5f, -0.5f, 0.5f),
        float3(-0.5f, -0.5f, 0.5f),
        float3(-0.5f, -0.5f, -0.5f),

        // +Y face
        float3(-0.5f, 0.5f, -0.5f),
        float3(0.5f, 0.5f, -0.5f),
        float3(0.5f, 0.5f, 0.5f),

        float3(0.5f, 0.5f, 0.5f),
        float3(-0.5f, 0.5f, 0.5f),
        float3(-0.5f, 0.5f, -0.5f)
    };

    float3 vertex = unitCube[input.vertexID];
    
    VertexOutput output;
    float4 clip = mul(pushConstants.viewproj, float4(vertex, 1.f));
    output.position = clip.xyzz; // infinite depth
    output.positionLocal = vertex;
    
	return output;
}

PixelOutput skyPS(VertexOutput input)
{
    PixelOutput result;
    float4 envColor = skybox.Sample(skyboxSampler, input.positionLocal);

    result.color = envColor;
    return result;
}