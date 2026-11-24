#include "Object3d.hlsli"
#include "OutdoorLighting.hlsli"

// 定数バッファのバインド (RootSignature の index と register を一致させる)
cbuffer cbMaterial : register(b0) // Index 0
{
    Material gMaterial;
};

cbuffer cbDirectionalLight : register(b1) // Index 2 (C++側で2番に設定)
{
    DirectionalLight gDirectionalLight;
};

cbuffer cbCamera : register(b2) // Index 3
{
    Camera gCamera;
};

// ★追加：スポットライト (Index 4)
struct SpotLight
{
    float4 color;
    float3 position;
    float intensity;
    float3 direction;
    float distance;
    float decay;
    float cosAngle;
    float cosFalloffStart;
    float padding;
};
cbuffer cbSpotLight : register(b3)
{
    SpotLight gSpotLight;
};

struct ShadowScene
{
    float4x4 lightViewProjection;
    float2 texelSize;
    float depthBias;
    float normalBias;
    float strength;
    float3 padding;
};
cbuffer cbShadowScene : register(b4)
{
    ShadowScene gShadowScene;
};

Texture2D<float4> gTexture : register(t0); // Index 5
TextureCube<float4> gEnvironmentTexture : register(t1); // ★追加
Texture2D<float> gDirectionalShadowMap : register(t3);
SamplerState gSampler : register(s0);
SamplerComparisonState gShadowSampler : register(s1);

static const int kSpecularTypePhong = 0;
static const int kSpecularTypeNone = 2;

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
    float4 normal : SV_TARGET1;
};

float3 SafeNormalize(float3 value, float3 fallbackValue)
{
    const float lengthSquared = dot(value, value);
    return lengthSquared > 1.0e-10f ? value * rsqrt(lengthSquared) : fallbackValue;
}

float EvaluateBlinnPhongSpecular(float3 normal, float3 lightDirection, float3 toEye, float shininess)
{
    const float NdotL = dot(normal, lightDirection);
    const float3 halfVectorSum = lightDirection + toEye;
    const float halfVectorLengthSquared = dot(halfVectorSum, halfVectorSum);
    if (NdotL <= 0.0f || halfVectorLengthSquared <= 1.0e-10f)
    {
        return 0.0f;
    }

    const float3 halfVector = halfVectorSum * rsqrt(halfVectorLengthSquared);
    return pow(saturate(dot(normal, halfVector)), max(shininess, 1.0f));
}

float EvaluatePhongSpecular(float3 normal, float3 lightDirection, float3 toEye, float shininess)
{
    const float NdotL = dot(normal, lightDirection);
    if (NdotL <= 0.0f)
    {
        return 0.0f;
    }

    const float3 reflectedLight = reflect(-lightDirection, normal);
    return pow(saturate(dot(reflectedLight, toEye)), max(shininess, 1.0f));
}

float EvaluateSpecular(float3 normal, float3 lightDirection, float3 toEye, float shininess)
{
    if (gMaterial.specularType == kSpecularTypeNone)
    {
        return 0.0f;
    }
    if (gMaterial.specularType == kSpecularTypePhong)
    {
        return EvaluatePhongSpecular(normal, lightDirection, toEye, shininess);
    }
    return EvaluateBlinnPhongSpecular(normal, lightDirection, toEye, shininess);
}

float EvaluateDirectionalShadow(float3 worldPosition, float3 normal, float3 lightDirection)
{
    float4 lightClipPosition = mul(float4(worldPosition, 1.0f), gShadowScene.lightViewProjection);
    if (lightClipPosition.w <= 0.0f)
    {
        return 1.0f;
    }

    float3 lightNdc = lightClipPosition.xyz / lightClipPosition.w;
    if (lightNdc.x < -1.0f || lightNdc.x > 1.0f ||
        lightNdc.y < -1.0f || lightNdc.y > 1.0f ||
        lightNdc.z <= 0.0f || lightNdc.z >= 1.0f)
    {
        return 1.0f;
    }

    float2 shadowUv = float2(lightNdc.x * 0.5f + 0.5f, -lightNdc.y * 0.5f + 0.5f);
    float slopeBias = gShadowScene.normalBias * (1.0f - saturate(dot(normal, lightDirection)));
    float receiverDepth = lightNdc.z - max(gShadowScene.depthBias, slopeBias);
    float visibility = 0.0f;

    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            float2 offset = float2((float)x, (float)y) * gShadowScene.texelSize;
            visibility += gDirectionalShadowMap.SampleCmpLevelZero(
                gShadowSampler, shadowUv + offset, receiverDepth);
        }
    }

    visibility /= 9.0f;
    return lerp(1.0f, visibility, saturate(gShadowScene.strength));
}

float3 CellVariation(float3 cell)
{
    float3 seed = frac(cell * float3(0.1031f, 0.1030f, 0.0973f));
    seed += dot(seed, seed.yxz + 33.33f);
    return frac((seed.xxy + seed.yxx) * seed.zyx) * 2.0f - 1.0f;
}

void ApplySurfaceDetail(float3 worldPosition, inout float3 normal, inout float3 albedo)
{
    // Static world cells keep soil seams continuous. Moving crops opt out of grain.
    if (gMaterial.surfaceDetail.z <= 0.0f ||
        (gMaterial.surfaceDetail.y <= 0.0f && gMaterial.surfaceDetail.w <= 0.0f)) return;
    float3 p = worldPosition * gMaterial.surfaceDetail.z;
    float footprint = max(length(ddx(p)), length(ddy(p)));
    float visibility = 1.0f - smoothstep(0.35f, 0.85f, footprint);
    float3 variation = CellVariation(floor(p));
    albedo *= 1.0f + variation.x * gMaterial.surfaceDetail.y * visibility;
    float3 tangentVariation = variation - normal * dot(variation, normal);
    normal = SafeNormalize(normal + tangentVariation * gMaterial.surfaceDetail.w * visibility, normal);
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float3 N = length(input.normal) > 0.00001f ? normalize(input.normal) : float3(0.0f, 1.0f, 0.0f);
    output.normal = float4(N * 0.5f + 0.5f, 1.0f);
    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    
    // ライティング無効ならテクスチャ色をそのまま出す
    if (gMaterial.enableLighting == 0)
    {
        output.color = gMaterial.color * textureColor;
        // Opt-in unlit surface variation in world space keeps water patches continuous.
        if (gMaterial.surfaceDetail.y > 0 && gMaterial.surfaceDetail.z > 0)
        {
            float2 p = input.worldPosition.xz * gMaterial.surfaceDetail.z;
            float band = sin(p.y * 2.6f + 0.8f * sin(p.x + p.y * 0.4f)) + 0.55f * sin(p.x * 1.1f - p.y);
            float aa = 1.0f - smoothstep(0.35f, 1.0f, fwidth(band));
            output.color.rgb *= 1.0f + gMaterial.surfaceDetail.y * aa * (smoothstep(0.4f, 1.45f, band) - 0.24f);
            if (gMaterial.surfaceFront.w > 0.0f)
            {
                // Only the wet side is shaded. The meniscus cannot separate from its water mesh.
                float4 front = gMaterial.surfaceFront;
                float2 xz = input.worldPosition.xz;
                float grain = sin(xz.x * 12.0f + xz.y * 9.0f) * sin(xz.y * 7.0f - xz.x * 5.0f);
                float inside = front.z - dot(xz, front.xy) - (0.14f + 0.12f * grain) * front.w;
                clip(inside);
                float edge = 1.0f - smoothstep(0.0f, front.w, inside);
                output.color.rgb = lerp(output.color.rgb, float3(0.47f, 0.76f, 0.76f), edge * (0.40f + 0.17f * grain));
            }
        }
        return output;
    }

    float3 geometricNormal = N;
    float3 baseColor = gMaterial.color.rgb * textureColor.rgb;
    ApplySurfaceDetail(input.worldPosition, N, baseColor);
    output.normal = float4(N * 0.5f + 0.5f, 1.0f);
    float specularGain = saturate(gMaterial.surfaceDetail.x);

    // --- 準備 ---
    float3 V = SafeNormalize(gCamera.worldPosition - input.worldPosition, N);

    // --- 1. 平行光源 ---
    float3 L_dir = SafeNormalize(-gDirectionalLight.direction, float3(0.0f, 1.0f, 0.0f));
    float NdotL_dir = saturate(dot(N, L_dir));
    float directionalShadow = EvaluateDirectionalShadow(input.worldPosition, geometricNormal, L_dir);
    float3 ambient_dir = baseColor * EvaluateAmbientLight(geometricNormal,
        gDirectionalLight.color.rgb, gDirectionalLight.intensity, gMaterial.outdoorAmbient);
    float3 diffuse_dir = baseColor * gDirectionalLight.color.rgb * (NdotL_dir * 0.82f) * gDirectionalLight.intensity * directionalShadow;
    float spec_dir = EvaluateSpecular(N, L_dir, V, gMaterial.shininess);
    float3 specular_dir = gDirectionalLight.color.rgb * gDirectionalLight.intensity * spec_dir * directionalShadow * specularGain;

    // --- 2. スポットライト ---
    float3 lightVec = input.worldPosition - gSpotLight.position;
    float dist = length(lightVec);
    float3 L_spot = SafeNormalize(-lightVec, N);
    
    // 距離減衰
    float distAtten = pow(saturate(1.0f - dist / max(gSpotLight.distance, 1.0e-4f)), max(gSpotLight.decay, 0.0f));
    // 角度減衰
    float cosTheta = dot(SafeNormalize(lightVec, -N), SafeNormalize(gSpotLight.direction, float3(0.0f, -1.0f, 0.0f)));
    float angleRange = max(gSpotLight.cosFalloffStart - gSpotLight.cosAngle, 1.0e-4f);
    float angleAtten = saturate((cosTheta - gSpotLight.cosAngle) / angleRange);
    
    float spotFactor = gSpotLight.intensity * distAtten * angleAtten;
    float NdotL_spot = saturate(dot(N, L_spot));
    float3 diffuse_spot = baseColor * gSpotLight.color.rgb * NdotL_spot * spotFactor;
    float3 specular_spot = gSpotLight.color.rgb * spotFactor * EvaluateSpecular(N, L_spot, V, gMaterial.shininess) * specularGain;
    
     // --- 3. 環境マップ (★追加) ---
    // カメラから点への入射ベクトルを法線で反射させる
    float3 cameraToPosition = SafeNormalize(input.worldPosition - gCamera.worldPosition, -N);
    float3 reflectedVector = reflect(cameraToPosition, N);
    // 反射した方向の景色をサンプリング
    float3 environmentColor = gEnvironmentTexture.Sample(gSampler, reflectedVector).rgb;


    // 全てを合成
    output.color.rgb = ambient_dir + diffuse_dir + specular_dir + diffuse_spot + specular_spot;
    // 環境マップの色を係数に従って加算
    output.color.rgb += environmentColor * gMaterial.environmentCoefficient;
    
    output.color.a = gMaterial.color.a * textureColor.a;

    return output;
}
