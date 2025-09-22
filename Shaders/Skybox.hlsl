[[vk::binding(0, 0)]]
Texture2D hdrEquirec;

[[vk::binding(0, 0)]]
SamplerState hdrEquirecSampler;

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

VertexOutput skyboxVS(VertexInput input)
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
    output.position = mul(pushConstants.viewproj, float4(vertex, 1.f));
    output.positionLocal = vertex;
    
	return output;
}

float2 SampleSphericalMap(float3 v)
{
    const float2 invAtan = float2(0.1591f, 0.3183f);
    
    float2 uv = float2(atan2(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5f;
    return uv;
}

PixelOutput skyboxPS(VertexOutput input)
{
    float2 uv = SampleSphericalMap(normalize(input.positionLocal));
    
    PixelOutput result;
    result.color = hdrEquirec.Sample(hdrEquirecSampler, uv);

    return result;
}