#define PI 3.1415926538

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
ConstantBuffer<GlobalSceneData> globalSceneData : register(b0);

struct Material
{
    float4 baseColorFactor;
    float baseMetalnessFactor;
    float baseRoughnessFactor;
};

struct PushConstants
{
    float4x4 model;
    Material material;
};

[[vk::push_constant]]
ConstantBuffer<PushConstants> pushConstants : register(b1);

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
    float3 normal : NORMAL0;
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
    
    // assume model matrix is orthogonal
    output.normal = mul(pushConstants.model, float4(input.normal, 0.f)).xyz;
    output.color = input.color;
    return output;
}

float heaviside(float x)
{
    return x > 0.f ? 1.f : 0.f;
}

// Schlick's approximation to the Fresnel term
float3 schlicks(float3 f0, float VdotH)
{
    float vdoth2 = VdotH * VdotH;
    return f0 + (1 - f0) * abs(vdoth2 * vdoth2 * VdotH);
}

// microfacet brdf based on the Trowbridge-Reitz distribution
// alpha = roughness ^ 2
float specularBRDF(float alpha, float3 viewDir, float3 lightDir, float3 normal, float3 halfway)
{
    float alpha2 = alpha * alpha;
    float oneMinusAlpha2 = 1.f - alpha2;
    
    // Trowbridge-Reitz microfacet distribution term
    float NdotH = dot(normal, halfway);
    float x = (NdotH * NdotH) * (-oneMinusAlpha2) + 1.f;
    float D = (alpha2 * heaviside(NdotH)) / (PI * x * x);
    
    // light visibility term
    float NdotL = dot(normal, lightDir);
    float HdotL = dot(halfway, lightDir);
    float vL = heaviside(HdotL) / (abs(NdotL) + sqrt(alpha2 + oneMinusAlpha2 * (NdotL * NdotL)));
    
    // view visibility term
    float NdotV = dot(normal, viewDir);
    float HdotV = dot(halfway, viewDir);
    float vV = heaviside(HdotV) / (abs(NdotV) + sqrt(alpha2 + oneMinusAlpha2 * (NdotV * NdotV)));
    
    return vV * vL * D;
}

// lambertian brdf
float3 diffuseBRDF(float3 color)
{
    return (1.f / PI) * color;
}

#define F0 0.04
PixelOutput simplePS(VertexOutput input)
{
    PixelOutput result;
    float4 baseColor = pushConstants.material.baseColorFactor * input.color;
    float rough2 = pushConstants.material.baseRoughnessFactor * pushConstants.material.baseRoughnessFactor;
    
    float3 viewDirection = normalize(globalSceneData.viewPosition - input.positionWorld);
    float3 lightDirection = normalize(globalSceneData.lightDirection);
    float3 normal = normalize(input.normal);
    float3 halfway = normalize(viewDirection + lightDirection);
    
    float3 fresnel = schlicks(F0, dot(viewDirection, halfway));
    
    float3 diffuse = diffuseBRDF(baseColor.rgb);
    float3 specular = specularBRDF(rough2, viewDirection, lightDirection, normal, halfway) * globalSceneData.lightColor;
    
    result.color = float4(lerp(diffuse, specular, fresnel), 1.f);
    return result;
}