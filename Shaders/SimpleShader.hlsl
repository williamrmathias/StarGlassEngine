#define PI 3.1415926538
#define EPSILON 1e-5
#define SHADOW_DEPTH_BIAS 0.005f

struct GlobalSceneData
{
    float4x4 view;
    float4x4 viewproj;
    
    float4x4 shadowMatrix;
    
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

[[vk::binding(0, 2)]]
TextureCube irradianceMap;

[[vk::binding(0, 2)]]
SamplerState irradianceMapSampler;

[[vk::binding(0, 3)]]
TextureCube prefilteredEnvMap;

[[vk::binding(0, 3)]]
SamplerState prefilteredEnvMapSampler;

[[vk::binding(0, 4)]]
Texture2D brdfLut;

[[vk::binding(0, 4)]]
SamplerState brdfLutSampler;

[[vk::binding(0, 5)]]
Texture2DArray cascadedShadowMap;

[[vk::binding(0, 5)]]
SamplerState cascadedShadowMapSampler;

struct MaterialConstants
{
    float4 baseColorFactor;
    float metalnessFactor;
    float roughnessFactor;
    float alphaCutoff;
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
    float3 positionShadow : TEXCOORD1;
    float3 normal : TEXCOORD2;
    float2 uv : TEXCOORD3;
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
    output.positionShadow = mul(globalSceneData.shadowMatrix, float4(output.positionWorld, 1.f)).xyz;
    
    float3 N = mul(pushConstants.model, float4(input.normal, 0.f)).xyz;

    output.normal = N;
    output.uv = input.uv;
    output.color = input.color;
    return output;
}

// ------------------------
// Cotangent Frame Calculation (http://www.thetenthplanet.de/archives/1180)
// ------------------------

// Use cotangent frame calculation in pixel shader instead of precomputed values passed in as vertex input.
// This reduces vertex bandwidth, and helps protect against degenerate mesh assets. Also allows for more flexibility
// (useful for, ex. procedurally animated normals, etc)

float3x3 cotangentFrame(float3 N, float3 posWS, float2 uv)
{
    // screen-space derivatives of world-space position & UV
    float3 dp1 = ddx(posWS);
    float3 dp2 = ddy(posWS);
    float2 duv1 = ddx(uv);
    float2 duv2 = ddy(uv);

    // Mikk-style cotangent frame from derivatives
    float3 dp2perp = cross(dp2, N);
    float3 dp1perp = cross(N, dp1);

    float3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    float3 B = dp2perp * duv1.y + dp1perp * duv2.y;

    // Make it scale-invariant and robust
    float invmax = rsqrt(max(max(dot(T, T), dot(B, B)), 1e-20));
    T *= invmax;
    B *= invmax;

    // In HLSL, this constructor sets ROWS; use mul(vec, mat) later.
    return float3x3(T, B, N);
}

float3 perturbNormal(float3 Nws, float3 posWS, float2 uv)
{
    Nws = normalize(Nws);

    float3 n = normalTex.Sample(normalSampler, uv).xyz * 2.0f - 1.0f;

    float3x3 TBN = cotangentFrame(Nws, posWS, uv);

    // HLSL row-vector multiply: rows are T,B,N
    return normalize(mul(n, TBN));
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
    float GGXV = NdotL * sqrt((-NdotV * alpha2 + NdotV) * NdotV + alpha2);
    float GGXL = NdotV * sqrt((-NdotL * alpha2 + NdotL) * NdotL + alpha2);
    return 0.5f / (GGXV + GGXL);
}

// Fresnel term based on Schlick's approximation
// Reflectance at grazing angles (f90) is assumed to be 1
// Schlick 1994, "An Inexpensive BRDF Model for Physically-Based Rendering"
float3 F_Schlick(float VdotH, float3 f0)
{
    return f0 + (1.f - f0) * pow(1.f - VdotH, 5.f);
}

float3 F_SchlickRough(float NdotV, float3 f0, float roughness)
{
    return f0 + (max(1.f - roughness, f0) - f0) * pow(1.f - NdotV, 5.f);
}

// lambertian brdf
float3 lambert(float3 albedo)
{
    return (1.f / PI) * albedo;
}

float3 brdf_direct(
    float3 baseColor,
    float roughness,
    float metalness,
    float3 viewDir,
    float3 lightDir,
    float3 normal
)
{
    float3 halfway = normalize(viewDir + lightDir);
    
    // clamp NdotV and NdotL above to 0.045f to prevent infinity visibility term
    float NdotV = max(dot(normal, viewDir), 0.045f);
    float NdotL = max(dot(normal, lightDir), 0.045f);
    float NdotH = saturate(dot(normal, halfway));
    float VdotH = saturate(dot(viewDir, halfway));
    
    #define REFLECTANCE 0.04 // 0.16 * 0.5^2
    const float3 f0 = lerp(REFLECTANCE, baseColor, metalness);
    float3 F = F_Schlick(VdotH, f0);
    
    // input roughness param is a perceptual roughness
    // alpha is a more physically accurate value
    // clamp to 0.045 to reduce specular aliasing
    float alpha = clamp(roughness * roughness, 0.045f, 1.f);
    
    float D = D_GGX(NdotH, alpha);
    float V = V_SmithGGXCorrelated(NdotV, NdotL, alpha);
    
    float3 specular = (D * V) * F;
    
    float3 diffuseColor = lerp(baseColor, 0.f, metalness);
    float3 diffuse = lambert(diffuseColor) * (1.f - F);
    
    float3 radiance = (diffuse + specular) * NdotL * globalSceneData.lightColor;
    
    return radiance;
}

float3 brdf_IBL(
    float3 baseColor,
    float roughness,
    float metalness,
    float3 viewDir,
    float3 lightDir,
    float3 normal
)
{
    const float NdotV = saturate(dot(normal, viewDir));
    
    // input roughness param is a perceptual roughness
    // alpha is a more physically accurate value
    // clamp to 0.045 to reduce specular aliasing
    const float alpha = clamp(roughness * roughness, 0.045f, 1.f);
    
    float3 reflection = reflect(-viewDir, normal);
    
    float3 irradiance = irradianceMap.Sample(irradianceMapSampler, normal).rgb;
    
    // sync w/ LoadedGltf::generatePrefilteredEnvMap()
    #define PREFILTERED_ENV_MIPS 9
    float3 prefilteredColor = prefilteredEnvMap.SampleLevel(
        prefilteredEnvMapSampler, reflection, alpha * PREFILTERED_ENV_MIPS).rgb;
    
    float2 envBRDF = brdfLut.Sample(brdfLutSampler, float2(NdotV, roughness)).rg;
    
    #define REFLECTANCE 0.04 // 0.16 * 0.5^2
    const float3 f0 = lerp(REFLECTANCE, baseColor, metalness);
    float3 kS = F_SchlickRough(NdotV, f0, alpha);
    float3 kD = (1.f - kS) * (1.f - metalness);
    
    float3 specular = prefilteredColor * (kS * envBRDF.x + envBRDF.y);
    float3 diffuse = irradiance * baseColor.rgb;

    float3 ambient = (diffuse * kD) + specular;
    
    return ambient;
}

// A simple pseudorandom noise function based on screen position
float getRandom(float2 pos)
{
    return frac(sin(dot(pos, float2(12.9898f, 78.233f))) * 43758.5453f);
}

// Pre-computed Poisson Disk (16 samples)
static const float2 poissonDisk[16] =
{
    float2(-0.94201624, -0.39906216), float2(0.94558609, -0.76890725),
    float2(-0.09418410, -0.92938870), float2(0.34495938, 0.29387760),
    float2(-0.91588001, 0.45771432), float2(-0.81544232, -0.87912464),
    float2(-0.38277545, 0.27676845), float2(0.97484398, 0.75648379),
    float2(0.44323325, -0.97511554), float2(0.53742981, -0.47373420),
    float2(-0.26496911, -0.41893023), float2(0.79197514, 0.19090188),
    float2(-0.24188840, 0.99706507), float2(-0.81409955, 0.91437590),
    float2(0.19984126, 0.78641367), float2(0.14383161, -0.14100790)
};

float computeShadowFactor(float3 positionShadow, float3 normal, float3 lightDir, float2 screenUV)
{
    const float currDepth = positionShadow.z;
    const float2 ndc = positionShadow.xy * float2(0.5f, 0.5f) + float2(0.5f, 0.5f);
    
    uint width, height, numCascades;
    cascadedShadowMap.GetDimensions(width, height, numCascades);
    
    // if the current depth is behind the closest, we are in shadow
    // add a angle adjusted bias
    const float depthBias = lerp(0.005f, 0.0005f, dot(normal, lightDir));
    
    float shadow = 0.f;
    float2 texelSize = float2(1.f, 1.f) / float2(width, height);
    
    // randomly rotate poisson disk
    float randomAngle = getRandom(screenUV.xy) * PI * 2.f;
    float cosAngle = 0.f;
    float sinAngle = 0.f;
    sincos(randomAngle, sinAngle, cosAngle);
    
    float2x2 rotationMatrix = float2x2(cosAngle, -sinAngle, sinAngle, cosAngle);
    
    #define NUM_SAMPLES 16
    for (int i = 0; i < NUM_SAMPLES; ++i)
    {
        // rotate poisson offset and sample
        float2 offset = mul(poissonDisk[i], rotationMatrix);
        
        float pcfDepth = cascadedShadowMap.Sample(cascadedShadowMapSampler, float3(ndc + offset * texelSize * 2.f, 0.f)).r;
        shadow += (currDepth - depthBias) > pcfDepth ? 1.f : 0.f;
    }

    return shadow / float(NUM_SAMPLES);
}

PixelOutput simplePS(VertexOutput input)
{
    PixelOutput result;
    
    float4 baseColorSample = baseColorTex.Sample(baseColorSampler, input.uv);
    
    // evaluate if alpha cutout should be split into a different pipeline
    // (due to disabling early-Z test)
    const float alpha = input.color.a * baseColorSample.a;
    clip(alpha - pushConstants.material.alphaCutoff);
    
    float4 metalRoughSample = metalRoughTex.Sample(metalRoughSampler, input.uv);
    
    float4 baseColor = baseColorSample * pushConstants.material.baseColorFactor * input.color;
    
    float roughness = metalRoughSample.g * pushConstants.material.roughnessFactor;
    float metalness = metalRoughSample.b * pushConstants.material.metalnessFactor;
    
    float3 viewDirection = normalize(globalSceneData.viewPosition - input.positionWorld);
    float3 lightDirection = normalize(globalSceneData.lightDirection);
    
    float3 normal = perturbNormal(normalize(input.normal), input.positionWorld, input.uv);
    
    float3 radiance = brdf_direct(baseColor.rgb, roughness, metalness, viewDirection, lightDirection, normal);
    
    float3 ambient = brdf_IBL(baseColor.rgb, roughness, metalness, viewDirection, lightDirection, normal);
    
    const float shadowFactor = computeShadowFactor(input.positionShadow, input.normal, lightDirection, input.position.xy);
    radiance = radiance * (1.f - shadowFactor); // TODO conditional skip the direct brdf
    
    result.color = float4(radiance + ambient, alpha);
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
    float metalFactor = metalRoughSample.b* pushConstants.material.metalnessFactor;
    
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

    // Everything in world space
    float3 normalWS = perturbNormal(normalize(input.normal), input.positionWorld, input.uv);

    // visualize [-1,1] -> [0,1]
    result.color = float4(normalWS * 0.5f + 0.5f, 1.0f);
    return result;
}

// This shader displays the vertex normal of the draw
// used for debugging
PixelOutput vertNormalDebugPS(VertexOutput input)
{
    PixelOutput result;
    
    result.color = float4(input.normal.xyz, 1.f);
    return result;
}

// This shader displays the UV coordinates of the draw
// used for debugging
PixelOutput uvDebugPS(VertexOutput input)
{
    PixelOutput result;
    
    result.color = float4(input.uv.xy, 0.f, 1.f);
    return result;
}