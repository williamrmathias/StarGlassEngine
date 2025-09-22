[[vk::binding(0, 0)]]
TextureCube environmentMap;

[[vk::binding(0, 0)]]
SamplerState environmentMapSampler;

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

VertexOutput irradianceVS(VertexInput input)
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

#define PI 3.14159265359f

// floor 2 pi / sampleDelta
#define PHI_SAMPLES 251

// floor 0.5 pi / sampleDelta
#define THETA_SAMPLES 62
PixelOutput irradiancePS(VertexOutput input)
{
    PixelOutput result;
    
     // the sample direction equals the hemisphere's orientation 
    float3 normal = normalize(input.positionLocal);
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
            
            irradiance += environmentMap.Sample(environmentMapSampler, sampleVec).rgb * cosTheta * sinTheta;
        }
    }
    
    irradiance = PI * irradiance * (1.f / float(PHI_SAMPLES * THETA_SAMPLES));

    result.color = float4(irradiance, 1.f);
    return result;
}