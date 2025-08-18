#define PI 3.1415926538
#define EPSILON 1e-5

struct GlobalSceneData
{
    float4x4 viewproj;
    
    float3 viewPosition;
    float padding1;
    
    float3 lightDirection;
    float padding2;
    
    float3 lightColor;
    float padding3;
};

[[vk::binding(0, 0)]]
ConstantBuffer<GlobalSceneData> globalSceneData;

[[vk::binding(0, 1)]]
Texture2D baseColorTex;

[[vk::binding(0, 1)]]
SamplerState baseColorSampler;

[[vk::binding(1, 1)]]
Texture2D metalRoughTex;

[[vk::binding(1, 1)]]
SamplerState metalRoughSampler;

[[vk::binding(2, 1)]]
Texture2D normalTex;

[[vk::binding(2, 1)]]
SamplerState normalSampler;

struct MaterialConstants
{
    float4 baseColorFactor;
    float metalnessFactor;
    float roughnessFactor;
};

struct PushConstants
{
    float4x4 model;
    MaterialConstants material;
};

[[vk::push_constant]]
ConstantBuffer<PushConstants> pushConstants;

struct VertexInput
{
    [[vk::location(0)]] float3 position : POSITION0;
    [[vk::location(1)]] float3 normal : NORMAL0;
    [[vk::location(2)]] float4 tangent : TANGENT0;
    [[vk::location(3)]] float2 uv : TEXCOORD0;
    [[vk::location(4)]] float4 color : COLOR0;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float3 positionWorld : TEXCOORD0;
    float3 tangent : TEXCOORD1;
    float3 normal : TEXCOORD2;
    float3 binormal : TEXCOORD3;
    float2 uv : TEXCOORD4;
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
    output.positionWorld = mul(pushConstants.model, float4(input.position, 1.f)).xyz;
    
    float3 N = normalize(mul(pushConstants.model, float4(input.normal, 0.f)).xyz);
    float3 T = normalize(mul(pushConstants.model, float4(input.tangent.xyz, 0.f)).xyz);
    float3 B = normalize(cross(N, T)) * input.tangent.w;
    
    output.tangent = T;
    output.normal = N;
    output.binormal = B;
    output.uv = input.uv;
    output.color = input.color;
    return output;
}

// ------------------------
// Specular BRDF (https://google.github.io/filament/Filament.html#materialsystem/specularbrdf)
// ------------------------

// microfacet normal distribution function based on the Trowbridge-Reitz / GGX distribution
// Walter et al. 2007, "Microfacet Models for Refraction through Rough Surfaces"
float D_GGX(float NdotH, float alpha)
{
    float alpha2 = alpha * alpha;
    float f = (NdotH * alpha2 - NdotH) * NdotH + 1.f;
    return alpha2 / (PI * f * f);
}

// visibility function based on height-corrected Smith Geometric Shadowing
// Heitz 2014, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs"
float V_SmithGGXCorrelated(float NdotV, float NdotL, float alpha)
{
    float alpha2 = alpha * alpha;
    float GGXV = NdotL * sqrt((NdotV - alpha2 * NdotV) * NdotV + alpha2);
    float GGXL = NdotV * sqrt((NdotL - alpha2 * NdotL) * NdotL + alpha2);
    return 0.5f / (GGXV + GGXL);
}

// Fresnel term based on Schlick's approximation
// Reflectance at grazing angles (f90) is assumed to be 1
// Schlick 1994, "An Inexpensive BRDF Model for Physically-Based Rendering"
float3 F_Schlick(float u, float3 f0)
{
    return f0 + (float3(1.f, 1.f, 1.f) - f0) * pow(1.f - u, 5.f);
}

// Cook-Torrance specular microfacet model
float3 specularBRDF(
    float3 baseColor,
    float roughness,
    float metalness,
    float3 viewDir, 
    float3 lightDir,
    float3 normal,
    float3 halfway
)
{
    float NdotV = abs(dot(normal, viewDir)) + EPSILON;
    float NdotL = abs(dot(normal, lightDir)) + EPSILON;
    float NdotH = abs(dot(normal, halfway)) + EPSILON;
    float LdotH = abs(dot(lightDir, halfway)) + EPSILON;
    
    // input roughness param is a perceptual roughness
    // alpha is a more physically accurate value
    float alpha = roughness * roughness;
    
    float D = D_GGX(NdotH, alpha);
    float V = V_SmithGGXCorrelated(NdotV, NdotL, alpha);
    
    float reflectance = 0.5f;
    float3 f0 = 0.16f * reflectance * reflectance * (1.f - metalness) + baseColor * metalness;
    float3 F = F_Schlick(LdotH, f0);
    
    return (D * V) * F;
}

// lambertian brdf
float3 diffuseBRDF(float3 color)
{
    return (1.f / PI) * color;
}

PixelOutput simplePS(VertexOutput input)
{
    PixelOutput result;
    
    float4 baseColorSample = baseColorTex.Sample(baseColorSampler, input.uv);
    float4 metalRoughSample = metalRoughTex.Sample(metalRoughSampler, input.uv);
    float4 normalSample = normalTex.Sample(normalSampler, input.uv);
    
    float4 baseColor = baseColorSample * pushConstants.material.baseColorFactor * input.color;
    
    float roughness = metalRoughSample.g * pushConstants.material.roughnessFactor;
    float metalness = metalRoughSample.b * pushConstants.material.metalnessFactor;
    
    float3 viewDirection = normalize(globalSceneData.viewPosition - input.positionWorld);
    float3 lightDirection = normalize(globalSceneData.lightDirection);
    
    float3x3 TBN = float3x3(input.tangent, input.binormal, input.normal);
    float3 normal = normalSample.xyz * float3(2.f, 2.f, 2.f) - float3(1.f, 1.f, 1.f);
    
    normal = normalize(mul(TBN, normal));
    
    float3 halfway = normalize(viewDirection + lightDirection);
    
    float3 diffuseColor = lerp(baseColor.rgb, 0.f, metalness);
    float3 diffuse = diffuseBRDF(diffuseColor);
    float3 specular = specularBRDF(
        baseColor.rgb, roughness, metalness,
        viewDirection, lightDirection, normal, halfway
    );
    
    result.color = float4(diffuse + specular, 1.f);
    return result;
}

// This shader displays the base color of the draw
// used for debugging
PixelOutput baseColorDebugPS(VertexOutput input)
{
    PixelOutput result;
    
    float4 baseColorSample = baseColorTex.Sample(baseColorSampler, input.uv);
    
    result.color = baseColorSample * pushConstants.material.baseColorFactor * input.color;
    return result;
}

// This shader displays the metalness of the draw
// used for debugging
PixelOutput metalDebugPS(VertexOutput input)
{
    PixelOutput result;
    
    float4 metalRoughSample = metalRoughTex.Sample(metalRoughSampler, input.uv);
    float metalFactor = metalRoughSample.b * pushConstants.material.metalnessFactor;
    
    result.color = float4(metalFactor, metalFactor, metalFactor, 1.f);
    return result;
}

// This shader displays the roughness of the draw
// used for debugging
PixelOutput roughDebugPS(VertexOutput input)
{
    PixelOutput result;
    
    // this shader uses the *perceptual* roughness value, not alpha
    float4 metalRoughSample = metalRoughTex.Sample(metalRoughSampler, input.uv);
    float roughFactor = metalRoughSample.g * pushConstants.material.roughnessFactor;
    
    result.color = float4(roughFactor, roughFactor, roughFactor, 1.f);
    return result;
}

// This shader displays the normal of the draw
// used for debugging
PixelOutput normalDebugPS(VertexOutput input)
{
    PixelOutput result;
    
    float4 normalSample = normalTex.Sample(normalSampler, input.uv);
    
    float3x3 TBN = float3x3(input.tangent, input.binormal, input.normal);
    float3 normal = normalSample.xyz * float3(2.f, 2.f, 2.f) - float3(1.f, 1.f, 1.f);
    
    normal = normalize(mul(TBN, normal));
    
    result.color = float4(normal.x, normal.y, normal.z, 1.f);
    return result;
}