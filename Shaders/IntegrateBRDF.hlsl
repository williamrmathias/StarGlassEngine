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

VertexOutput integrateBRDF_VS(VertexInput input)
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

// geometric function based on height-corrected Smith Geometric Shadowing
// Heitz 2014, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs"
float G_SmithGGXCorrelated(float NdotV, float NdotL, float alpha)
{
    float alpha2 = alpha * alpha;
    float GGXV = NdotV * sqrt(alpha2 + (1.f - alpha2) * (NdotV * NdotV));
    float GGXL = NdotL * sqrt(alpha2 + (1.f - alpha2) * (NdotL * NdotL));
    
    return GGXV * GGXL;
}

#define NUM_SAMPLES 1024

// this function generate a LUT mapping (NdotV, roughness) -> specular BRDF
PixelOutput integrateBRDF_PS(VertexOutput input)
{
    PixelOutput result;
    
    const float NdotV = input.uv.x;
    const float roughness = input.uv.y;
    
    float3 V = float3(sqrt(1.f - NdotV * NdotV), 0.f, NdotV);
    
    float A = 0.f;
    float B = 0.f;
    
    float N = float3(0.f, 0.f, 1.f);
    
    for (uint i = 0; i < NUM_SAMPLES; ++i)
    {
        // generates a sample vector that's biased towards the
        // preferred alignment direction (importance sampling).
        float2 Xi = Hammersley(i, NUM_SAMPLES);
        float3 H = ImportanceSampleGGX(Xi, N, roughness);
        float3 L = normalize(2.f * dot(V, H) * H - V);
        
        float NdotL = saturate(L.z);
        float NdotH = saturate(H.z);
        float VdotH = saturate(dot(V, H));
        
        if (NdotL > 0.f)
        {
            float G = G_SmithGGXCorrelated(NdotV, NdotL, roughness * roughness);
            float G_Vis = (G * VdotH) / (NdotH * NdotV);
            float Fc = pow(1.f - VdotH, 5.f);
            
            A += (1.f - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }
    
    A /= float(NUM_SAMPLES);
    B /= float(NUM_SAMPLES);
    
    result.color = float4(A, B, 0.f, 0.f);
    
    return result;
}