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
    [[vk::location(2)]] float2 uv : TEXCOORD0;
    [[vk::location(3)]] float4 color : COLOR0;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float3 positionWorld : TEXCOORD0;
    float3 normalWorld : NORMAL0;
    float2 uv : TEXCOORD1;
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
    
    output.normalWorld = mul(pushConstants.model, float4(input.normal, 0.f)).xyz;
    output.uv = input.uv;
    output.color = input.color;
    return output;
}

// microfacet normal distribution function based on the Trowbridge-Reitz / GGX distribution
float distribution(float NdotH, float alpha)
{
    float a = NdotH * alpha;
    float k = alpha / (1.f - NdotH * NdotH + a * a);
    return k * k * (1.f / PI);
}

// visibility function based on height-corrected Smith Geometric Shadowing
float visibility(float NdotV, float NdotL, float alpha)
{
    float alpha2 = alpha * alpha;
    float lambdaV = NdotL * sqrt(NdotV * NdotV * (1.f - alpha2) + alpha2);
    float lambdaL = NdotV * sqrt(NdotL * NdotL * (1.f - alpha2) + alpha2);
    return 0.5f / (lambdaV + lambdaL);
}

// Fresnel term based on Schlick's approximation
// Reflectance at grazing angles (f90) is assumed ti be 1
float3 fresnel(const float3 f0, float VdotH)
{
    float f = pow(1.f - VdotH, 5.f);
    return f + f0 * (1.f - f);
}

// Cook-Torrance specular microfacet model
float3 specularBRDF(float roughness, float3 viewDir, float3 lightDir, float3 normal, float3 halfway)
{
    float NdotV = abs(dot(normal, viewDir)) + EPSILON;
    float NdotL = clamp(dot(normal, lightDir), 0.f, 1.f);
    float NdotH = clamp(dot(normal, halfway), 0.f, 1.f);
    float LdotH = clamp(dot(lightDir, halfway), 0.f, 1.f);
    
    // input roughness param is a perceptual roughness
    // alpha is a more physically accurate value
    float alpha = roughness * roughness;
    
    float D = distribution(NdotH, alpha);
    float V = visibility(NdotV, NdotL, alpha);
    float3 F = fresnel(float3(0.04f, 0.04f, 0.04f), LdotH);
    
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
    
    float4 baseColor = baseColorSample * pushConstants.material.baseColorFactor * input.color;
    
    float roughness = metalRoughSample.b * pushConstants.material.roughnessFactor;
    float rough2 = roughness * roughness;
    
    float3 viewDirection = normalize(globalSceneData.viewPosition - input.positionWorld);
    float3 lightDirection = normalize(globalSceneData.lightDirection);
    float3 normal = normalize(input.normalWorld);
    float3 halfway = normalize(viewDirection + lightDirection);
    
    float3 diffuse = diffuseBRDF(baseColor.rgb);
    float3 specular = specularBRDF(rough2, viewDirection, lightDirection, normal, halfway);
    
    //result.color = float4(lerp(diffuse, specular, fresnel), 1.f);
    result.color = float4(diffuse + specular, 1.f);
    return result;
}