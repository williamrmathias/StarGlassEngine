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

VertexOutput irradianceVS(VertexInput input)
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

#define PI 3.14159265359f

// floor 2 pi / sampleDelta
#define PHI_SAMPLES 251

// floor 0.5 pi / sampleDelta
#define THETA_SAMPLES 62
PixelOutput irradiancePS(VertexOutput input)
{
    PixelOutput result;
    
     // the sample direction equals the hemisphere's orientation 
    float3 normal = GetRayDirection(input.uv, pushConstants.faceIndex);
    float3 up = float3(0.f, 1.f, 0.f);
    float3 right = normalize(cross(up, normal));
    up = normalize(cross(normal, right));
    
    float3 irradiance = 0.f;
    const float sampleDelta = 0.025f;
    
    for (int i = 0; i < PHI_SAMPLES; i++)
    {
        const float phi = sampleDelta * i;
        
        for (int j = 0; j < THETA_SAMPLES; j++)
        {
            const float theta = sampleDelta * j;
            const float sinTheta = sin(theta);
            const float cosTheta = cos(theta);
            
            // spherical -> cartesian (in tangent space)
            float3 tangentSample = float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
            
            // tangent space -> world space
            float3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal;
            
            irradiance += skybox.Sample(skyboxSampler, sampleVec).rgb * cosTheta * sinTheta;
        }
    }
    
    irradiance = PI * irradiance * (1.f / float(PHI_SAMPLES * THETA_SAMPLES));

    result.color = float4(irradiance, 1.f);
    return result;
}