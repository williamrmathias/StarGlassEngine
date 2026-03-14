[[vk::binding(0, 0)]]
TextureCube skybox;

[[vk::binding(0, 0)]]
SamplerState skyboxSampler;

struct PushConstants
{
    uint faceIndex;
    float roughness;
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
    float2 uv : TEXCOORD0;
};

struct PixelOutput
{
    float4 color : SV_Target;
};

VertexOutput prefilterVS(VertexInput input)
{
    const float2 fullScreenTriangle[3] =
    {
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

// generating Van Der Corput sequence values
float RadicalInverse_Vdc(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

// get sequence element i out of n total samples
float2 Hammersley(uint i, uint n)
{
    return float2(float(i) / float(n), RadicalInverse_Vdc(i));
}

// generate an importance-biased vector to sample the skybox
// https://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
#define PI 3.14159265359f
float3 ImportanceSampleGGX(float2 Xi, float3 normal, float roughness)
{
    // input roughness param is a perceptual roughness
    // alpha is a more physically accurate value
    const float alpha = roughness * roughness;
    
    const float phi = 2.f * PI * Xi.x;
    const float cosTheta = sqrt((1.f - Xi.y) / (1.f + (alpha * alpha - 1.f) * Xi.y));
    const float sinTheta = sqrt(1.f - cosTheta * cosTheta);
    
    // spherical coords -> cartesian coords
    float3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;
    
    // transform tangent space vector to world space
    float3 up = abs(normal.z) < 0.999f ? float3(0.f, 0.f, 1.f) : float3(1.f, 0.f, 0.f);
    float3 tangent = normalize(cross(up, normal));
    float3 bitangent = cross(normal, tangent);
    
    float3 sampleVec = tangent * H.x + bitangent * H.y + normal * H.z;
    return normalize(sampleVec);
}

float3 GetRayDirection(float2 uv, uint faceIdx)
{
    // Convert UV [0,1] to range [-1, 1]
    float2 p = uv * 2.0f - 1.0f;
    
    float3 dir;
    switch (faceIdx)
    {
        case 0:
            dir = float3(1.0, -p.y, -p.x);
            break; // +X
        case 1:
            dir = float3(-1.0, -p.y, p.x);
            break; // -X
        case 2:
            dir = float3(p.x, 1.0, p.y);
            break; // +Y
        case 3:
            dir = float3(p.x, -1.0, -p.y);
            break; // -Y
        case 4:
            dir = float3(p.x, -p.y, 1.0);
            break; // +Z
        case 5:
            dir = float3(-p.x, -p.y, -1.0);
            break; // -Z
    }
    
    return normalize(dir);
}

#define NUM_SAMPLES 4096
PixelOutput prefilterPS(VertexOutput input)
{
    PixelOutput result;
    
    float3 normal = GetRayDirection(input.uv, pushConstants.faceIndex);
    float3 V = normal;
    
    float totalWeight = 0.f;
    float3 prefilteredColor = float3(0.f, 0.f, 0.f);
    
    for (uint i = 0u; i < NUM_SAMPLES; i++)
    {
        float2 Xi = Hammersley(i, NUM_SAMPLES);
        float3 H = ImportanceSampleGGX(Xi, normal, pushConstants.roughness);
        float3 L = 2 * dot(V, H) * H - V;
        
        float NdotL = saturate(dot(normal, L));
        if (NdotL > 0.f)
        {
            prefilteredColor += skybox.Sample(skyboxSampler, L).rgb * NdotL;
            totalWeight += NdotL;
        }
    }
    
    prefilteredColor = prefilteredColor / totalWeight;
    result.color = float4(prefilteredColor, 1.f);
    return result;
}