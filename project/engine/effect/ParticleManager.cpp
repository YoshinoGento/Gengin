#include "effect/ParticleManager.h"
#include "2d/TextureManager.h"
#include "base/DirectXCommon.h"
#include "effect/ParticleUploadMapping.h"
#include "base/FrameClock.h"
#include "base/Logger.h"
#include "base/SrvManager.h"
#include "3d/Camera.h"
#include "3d/Model.h"
#include <d3dcompiler.h>
#include <cassert>
#include <algorithm>
#include <cstring>
#include <random>
#include <cmath>
#include <sstream>

#pragma comment(lib, "d3dcompiler.lib")

using namespace Microsoft::WRL;


// Model::VertexData と同じフィールド順にする（position → normal → texcoord）
// ※ Input Layout のメモリ配置と一致させる必要がある
struct VertexData {
    Vector4 position;
    Vector3 normal;
    Vector2 texcoord;
};

static void TransitionResource(
    ID3D12GraphicsCommandList* commandList,
    ID3D12Resource* resource,
    D3D12_RESOURCE_STATES before,
    D3D12_RESOURCE_STATES after) {
    assert(commandList);
    assert(resource);
    if (before == after) {
        return;
    }

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);
}

static void InsertUAVBarrier(ID3D12GraphicsCommandList* commandList, ID3D12Resource* resource) {
    assert(commandList);

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.UAV.pResource = resource;
    commandList->ResourceBarrier(1, &barrier);
}

static GPUParticleInteractionSettings SanitizeInteractionSettings(
    const GPUParticleInteractionSettings& source,
    uint32_t maxParticleCount) {
    GPUParticleInteractionSettings settings = source;
    settings.gridCountX = (std::max)(1u, settings.gridCountX);
    settings.gridCountY = (std::max)(1u, settings.gridCountY);
    settings.gridCountZ = (std::max)(1u, settings.gridCountZ);

    auto product = [&settings]() -> uint64_t {
        return static_cast<uint64_t>(settings.gridCountX) *
            static_cast<uint64_t>(settings.gridCountY) *
            static_cast<uint64_t>(settings.gridCountZ);
    };

    while (product() > maxParticleCount) {
        uint32_t* largest = &settings.gridCountX;
        if (settings.gridCountY > *largest) {
            largest = &settings.gridCountY;
        }
        if (settings.gridCountZ > *largest) {
            largest = &settings.gridCountZ;
        }
        if (*largest <= 1) {
            break;
        }
        --(*largest);
    }

    uint64_t limitedGridParticleCount = product();
    if (limitedGridParticleCount > maxParticleCount) {
        limitedGridParticleCount = maxParticleCount;
    }
    uint32_t gridParticleCount = static_cast<uint32_t>(limitedGridParticleCount);
    settings.particleCount = settings.particleCount == 0
        ? gridParticleCount
        : (std::min)(settings.particleCount, gridParticleCount);
    settings.particleSize = std::clamp(settings.particleSize, 0.02f, 0.05f);
    settings.brushRadius = (std::max)(settings.brushRadius, 0.001f);
    settings.brushStrength = (std::max)(settings.brushStrength, 0.0f);
    settings.isPressed = settings.isPressed != 0 ? 1u : 0u;
    if (settings.operation > static_cast<uint32_t>(InteractionBrushOperation::Pull)) {
        settings.operation = static_cast<uint32_t>(InteractionBrushOperation::None);
    }
    if (settings.operation == static_cast<uint32_t>(InteractionBrushOperation::None)) {
        settings.isPressed = 0;
    }
    if (!std::isfinite(settings.deltaTime) || settings.deltaTime < 0.0f) {
		settings.deltaTime = 0.0f;
	}
    settings.deltaTime = std::clamp(settings.deltaTime, 0.0f, 1.0f / 15.0f);
    if (!std::isfinite(settings.damping)) {
        settings.damping = 0.95f;
    }
    settings.damping = std::clamp(settings.damping, 0.0f, 1.0f);
    return settings;
}

static float GetCropBurstPhaseDuration(CropBurstPhase phase) {
    switch (phase) {
    case CropBurstPhase::Absorb:
        return 0.35f;
    case CropBurstPhase::Impact:
        return 0.10f;
    case CropBurstPhase::Burst:
        return 0.30f;
    case CropBurstPhase::Fade:
        return 0.45f;
    case CropBurstPhase::None:
    default:
        return 0.0f;
    }
}

static uint32_t GetCropBurstEmitCount(CropBurstLevel level) {
    switch (level) {
    case CropBurstLevel::Strong:
        return 320u;
    case CropBurstLevel::Rare:
        return 450u;
    case CropBurstLevel::Normal:
    default:
        return 224u;
    }
}

static float GetCropBurstStrength(CropBurstLevel level) {
    switch (level) {
    case CropBurstLevel::Strong:
        return 8.0f;
    case CropBurstLevel::Rare:
        return 11.0f;
    case CropBurstLevel::Normal:
    default:
        return 6.2f;
    }
}

static CropBurstPhase GetNextCropBurstPhase(CropBurstPhase phase) {
    switch (phase) {
    case CropBurstPhase::Absorb:
        return CropBurstPhase::Impact;
    case CropBurstPhase::Impact:
        return CropBurstPhase::Burst;
    case CropBurstPhase::Burst:
        return CropBurstPhase::Fade;
    case CropBurstPhase::Fade:
    case CropBurstPhase::None:
    default:
        return CropBurstPhase::None;
    }
}

void ParticleManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
    constexpr uint32_t requiredViews=8; // Two particle sets with one SRV and three UAVs each.
    if(!dxCommon || !srvManager || !srvManager->GetSrvDescriptorHeap() ||
        srvManager->GetAllocatedCount()>SrvManager::kMaxSRVCount-requiredViews)
        throw graphics::InitializationError("ParticleManager requires a device and eight free descriptors.");
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;

    CreateRootSignature();
    CreateGraphicsPipelineState();
    CreateModel();

    instancingResource_ = dxCommon_->CreateBufferResource(sizeof(InstancingData) * kMaxInstanceCount);
    particle::MapUpload(instancingResource_.Get(),instancingData_,"instancing");

    instancingBufferView_.BufferLocation = instancingResource_->GetGPUVirtualAddress();
    instancingBufferView_.SizeInBytes = sizeof(InstancingData) * kMaxInstanceCount;
    instancingBufferView_.StrideInBytes = sizeof(InstancingData);

    // === discardしきい値用の定数バッファ（CBuffer）を作成 ===
    // 資料スライド10: alphaReference をシェーダーに渡すためのバッファ
    // GPU側の register(b0) にバインドされる
    materialResource_ = dxCommon_->CreateBufferResource(sizeof(MaterialData));
    particle::MapUpload(materialResource_.Get(),materialData_,"material");
    CreateGPUParticleResources();
    CreateGPUParticleComputeRootSignature();
    CreateGPUParticleComputePipelineState();
    CreateGPUParticleEmitComputeRootSignature();
    CreateGPUParticleEmitComputePipelineState();
    CreateGPUParticleUpdateComputeRootSignature();
    CreateGPUParticleUpdateComputePipelineState();
    CreateGPUParticleInteractionComputeRootSignature();
    CreateGPUParticleInteractionComputePipelineStates();
    CreateGPUParticleGraphicsRootSignature();
    CreateGPUParticleGraphicsPipelineState();
    CreateAccentFXResources();
    CreateAccentFXComputePipelineStates();
    materialData_->alphaReference = 0.0f; // 初期値: 0（従来と同じ動作）
}

void ParticleManager::Update(Camera* camera, float deltaTime) {
    SetCamera(camera);
	if (!std::isfinite(deltaTime)) {
		deltaTime = 0.0f;
	}
	frameDeltaTime_ = std::clamp(deltaTime, 0.0f, FrameClock::kMaximumFrameDeltaSeconds);
	const float legacyFrameScale = frameDeltaTime_ / FrameClock::kDefaultFixedDeltaSeconds;

    for (auto& groupPair : particleGroups_) {
        for (auto it = groupPair.second.particles.begin(); it != groupPair.second.particles.end();) {
            Particle& p = *it;
            p.currentTime += frameDeltaTime_;

            if (p.currentTime >= p.lifeTime) {
                it = groupPair.second.particles.erase(it);
                continue;
            }

            p.velocity.x += p.acceleration.x * legacyFrameScale;
            p.velocity.y += p.acceleration.y * legacyFrameScale;
            p.velocity.z += p.acceleration.z * legacyFrameScale;
            p.transform.translate.x += p.velocity.x * legacyFrameScale;
            p.transform.translate.y += p.velocity.y * legacyFrameScale;
            p.transform.translate.z += p.velocity.z * legacyFrameScale;

			// UVスクロールのアニメーション
			p.uvOffset.x += p.uvVelocity.x * frameDeltaTime_;
			p.uvOffset.y += p.uvVelocity.y * frameDeltaTime_;

            float t = p.currentTime / p.lifeTime;
            // 寿命でアルファ値を減衰させる (PS側で色の強さに反映される)
            p.color.w = 1.0f - t;

            // 寿命に応じてスケールを補間する（Ringの拡張アニメーションに必須）
            float currentSize = p.startSize + (p.endSize - p.startSize) * t;
			p.transform.scale = { currentSize,currentSize,currentSize };


            ++it;
        }
    }

    if (gpuParticleEmitter_) {
        gpuParticleEmitter_->emit = 0;
        if (gpuParticleEmitRequested_) {
            gpuParticleEmitter_->emit = 1;
            gpuParticleEmitRequested_ = false;
        }
    }

    if (cropBurstActive_) {
        cropBurstPhaseTimer_ += deltaTime;
        const float phaseDuration = GetCropBurstPhaseDuration(cropBurstPhase_);
        if (phaseDuration > 0.0f && cropBurstPhaseTimer_ >= phaseDuration) {
            cropBurstPhase_ = GetNextCropBurstPhase(cropBurstPhase_);
            cropBurstPhaseTimer_ = 0.0f;
            if (cropBurstPhase_ == CropBurstPhase::None) {
                cropBurstActive_ = false;
            }
        }
    }
}

Particle& ParticleManager::AddParticle(const std::string& name, const Vector3& position) {
    auto it = particleGroups_.find(name);
    assert(it != particleGroups_.end());
    it->second.particles.emplace_back();
    Particle& p = it->second.particles.back();
    p.transform.translate = position;
    p.transform.rotate = { 0.0f, 0.0f, 0.0f };
    p.transform.scale = { 1.0f, 1.0f, 1.0f };
    p.velocity = { 0.0f, 0.0f, 0.0f };
    p.acceleration = { 0.0f, 0.0f, 0.0f };
    p.color = { 1.0f, 1.0f, 1.0f, 1.0f };
    p.lifeTime = 1.0f;
    p.currentTime = 0.0f;
    return p;
}

void ParticleManager::Emit(const std::string& name, const Vector3& position, uint32_t count) {
    std::random_device seed_gen;
    std::mt19937 engine(seed_gen());
    std::uniform_real_distribution<float> distPos(-0.3f, 0.3f);
    std::uniform_real_distribution<float> distVel(-0.1f, 0.1f);
    std::uniform_real_distribution<float> distScale(0.1f, 0.4f);

    for (uint32_t i = 0; i < count; ++i) {
        Particle& p = AddParticle(name, position);
        // ランダムなオフセットと速度を付与
        p.transform.translate.x += distPos(engine);
        p.transform.translate.y += distPos(engine);
        p.transform.translate.z += distPos(engine);

        p.velocity = { distVel(engine), distVel(engine) + 0.1f, distVel(engine) };
        p.transform.scale = { distScale(engine), distScale(engine), 1.0f };
        p.lifeTime = 0.5f;
        p.color = { 1.0f, 0.8f, 0.2f, 1.0f }; // 火の粉のような色
    }
}

void ParticleManager::EmitHarvestBurst(const std::string& name, const Vector3& position, uint32_t count) {
    auto groupIt = particleGroups_.find(name);
    if (groupIt == particleGroups_.end() || count == 0) {
        return;
    }

    std::random_device seedGenerator;
    std::mt19937 engine(seedGenerator());
    std::uniform_real_distribution<float> angleDist(0.0f, 6.2831853f);
    std::uniform_real_distribution<float> radiusDist(0.0f, 1.0f);
    std::uniform_real_distribution<float> heightDist(0.0f, 0.35f);
    std::uniform_real_distribution<float> speedDist(0.03f, 0.18f);
    std::uniform_real_distribution<float> scaleDist(0.08f, 0.34f);
    std::uniform_real_distribution<float> rotateDist(-3.141592f, 3.141592f);

    const uint32_t safeCount = (std::min)(count, 1024u);
    for (uint32_t i = 0; i < safeCount; ++i) {
        Particle& particle = AddParticle(name, position);
        const float angle = angleDist(engine);
        const float radius = radiusDist(engine);
        const float outwardX = std::cos(angle);
        const float outwardZ = std::sin(angle);
        const float speed = speedDist(engine);

        particle.transform.translate.x += outwardX * radius * 0.55f;
        particle.transform.translate.y += heightDist(engine);
        particle.transform.translate.z += outwardZ * radius * 0.55f;
        particle.velocity = {
            outwardX * speed,
            0.10f + speed * 0.85f,
            outwardZ * speed,
        };
        particle.acceleration = { 0.0f, -0.0025f, 0.0f };
        particle.transform.rotate = { 0.0f, 0.0f, rotateDist(engine) };
        const float scale = scaleDist(engine);
        particle.transform.scale = { scale * 0.45f, scale, 1.0f };
        particle.startSize = scale;
        particle.endSize = scale * 0.15f;
        particle.lifeTime = 0.75f + radiusDist(engine) * 0.35f;

        switch (i % 3) {
        case 0:
            particle.color = { 0.35f, 0.95f, 0.18f, 1.0f };
            break;
        case 1:
            particle.color = { 1.0f, 0.92f, 0.30f, 1.0f };
            break;
        default:
            particle.color = { 0.58f, 0.38f, 0.18f, 0.82f };
            break;
        }
    }
}

void ParticleManager::ClearAll() {
    for (auto& pair : particleGroups_) {
        pair.second.particles.clear();
    }
}

void ParticleManager::RequestGPUParticleEmit(const Vector3& position, uint32_t count) {
    GPUParticleEmitSettings settings{};
    settings.translate = position;
    settings.radius = 2.0f;
    settings.color = { 1.0f, 0.0f, 1.0f, 1.0f };
    settings.scale = { 0.75f, 0.75f, 0.75f };
    settings.lifeTime = 5.0f;
    settings.baseVelocity = { 0.0f, 0.15f, 0.0f };
    settings.speed = 0.35f;
    settings.count = count;
    settings.emit = 1;
    settings.preset = 0;
    RequestGPUParticleEmit(settings);
}

void ParticleManager::RequestGPUParticleEmit(const GPUParticleEmitSettings& settings) {
    if (!gpuParticleEmitter_) {
        return;
    }

    *gpuParticleEmitter_ = settings;
    gpuParticleEmitter_->count = (std::min)(settings.count, kMaxGPUParticleCount);
    gpuParticleEmitter_->emit = 0;
    gpuParticleEmitRequested_ = settings.emit != 0 && gpuParticleEmitter_->count > 0;
}

void ParticleManager::ResetGPUParticles() {
    gpuParticleEmitRequested_ = false;
    gpuParticlesInitialized_ = false;
    gpuParticleInteractionInitialized_ = false;
    if (gpuParticleEmitter_) {
        gpuParticleEmitter_->count = 0;
        gpuParticleEmitter_->emit = 0;
    }
    accentEmitRequested_ = false;
    accentParticlesInitialized_ = false;
    cropBurstActive_ = false;
    cropBurstPhase_ = CropBurstPhase::None;
    cropBurstPhaseTimer_ = 0.0f;
    cropBurstEmitCount_ = 0;
    if (accentEmitter_) {
        accentEmitter_->count = 0;
        accentEmitter_->emit = 0;
    }
}

void ParticleManager::PlayCropBurst(const Vector3& center, CropBurstLevel level) {
    if (!accentEmitter_ || !cropBurstInfo_) {
        Logger::Log("[CropBurst] PlayCropBurst skipped: AccentFX resources are not initialized.");
        return;
    }

    cropBurstLevel_ = level;
    cropBurstCenter_ = center + Vector3{ 0.0f, 0.75f, 0.0f };
    cropBurstPhase_ = CropBurstPhase::Absorb;
    cropBurstPhaseTimer_ = 0.0f;
    cropBurstActive_ = true;
    cropBurstEmitCount_ = (std::min)(GetCropBurstEmitCount(level), kMaxAccentParticleCount);
    accentEmitRequested_ = cropBurstEmitCount_ > 0;
    accentParticlesInitialized_ = false;

    accentEmitter_->translate = cropBurstCenter_;
    accentEmitter_->radius = level == CropBurstLevel::Rare ? 2.8f : (level == CropBurstLevel::Strong ? 2.15f : 1.65f);
    accentEmitter_->color = level == CropBurstLevel::Rare
        ? Vector4{ 0.65f, 0.95f, 1.0f, 1.0f }
        : Vector4{ 1.0f, 0.85f, 0.35f, 1.0f };
    const float scale = level == CropBurstLevel::Rare ? 0.36f : (level == CropBurstLevel::Strong ? 0.29f : 0.23f);
    accentEmitter_->scale = { scale, scale, scale };
    accentEmitter_->lifeTime = level == CropBurstLevel::Rare ? 1.65f : 1.45f;
    accentEmitter_->baseVelocity = { 0.0f, 0.18f, 0.0f };
    accentEmitter_->speed = level == CropBurstLevel::Rare ? 1.45f : (level == CropBurstLevel::Strong ? 1.10f : 0.85f);
    accentEmitter_->count = cropBurstEmitCount_;
    accentEmitter_->emit = 0;
    accentEmitter_->preset = 1;
    accentEmitter_->padding = 0;

    std::ostringstream log;
    log << "[CropBurst] PlayCropBurst called manager=" << static_cast<const void*>(this)
        << " level=" << static_cast<int>(level)
        << " center=(" << cropBurstCenter_.x << ", " << cropBurstCenter_.y << ", " << cropBurstCenter_.z << ")"
        << " count=" << cropBurstEmitCount_;
    Logger::Log(log.str());
}

void ParticleManager::Draw(bool drawDefaultGPUParticles) {
    if (!camera_) return;
    // CPU側の設定値をGPUバッファに反映
    materialData_->alphaReference = alphaReference_;

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->IASetVertexBuffers(1, 1, &instancingBufferView_);

    // CBufferをシェーダーのregister(b0)にバインド
    // Root Parameter Index 1 に対応（0番はテクスチャSRV）
    commandList->SetGraphicsRootConstantBufferView(1, materialResource_->GetGPUVirtualAddress());

    Matrix4x4 viewMatrix = camera_->GetViewMatrix();
    Matrix4x4 viewProjectionMatrix = camera_->GetViewProjectionMatrix();

    Matrix4x4 billboardMatrix = MatrixMath::MakeIdentity4x4();
    billboardMatrix.m[0][0] = viewMatrix.m[0][0]; billboardMatrix.m[0][1] = viewMatrix.m[1][0]; billboardMatrix.m[0][2] = viewMatrix.m[2][0];
    billboardMatrix.m[1][0] = viewMatrix.m[0][1]; billboardMatrix.m[1][1] = viewMatrix.m[1][1]; billboardMatrix.m[1][2] = viewMatrix.m[2][1];
    billboardMatrix.m[2][0] = viewMatrix.m[0][2]; billboardMatrix.m[2][1] = viewMatrix.m[1][2]; billboardMatrix.m[2][2] = viewMatrix.m[2][2];

    for (auto& groupPair : particleGroups_) {
        ParticleGroup& group = groupPair.second;
        uint32_t instanceIndex = 0;
        for (const auto& p : group.particles) {
            if (instanceIndex >= kMaxInstanceCount) break;

            // === ワールド行列の構築 ===
            // MakeAffineMatrix は Scale * RotateXYZ * Translate を一括計算する
            // これにより X/Y/Z 全軸の回転が正しく反映される
            // （旧コードは rotateZ しか使っておらず、X/Y軸回転が無視されていた）
            Matrix4x4 worldMatrix;
            if (group.model) {
                // モデル指定時（Ring, Cylinder等）: XYZ回転をそのまま使う
                // ビルボードは不要（3Dモデルは常にワールド空間で回転させる）
                worldMatrix = MatrixMath::MakeAffineMatrix(
                    p.transform.scale, p.transform.rotate, p.transform.translate);
            }
            else {
                // 四角形スプライト時: ビルボード + Z軸回転
                // カメラの方を向く行列を使って常にカメラ正面に表示する
                Matrix4x4 scaleMatrix = MatrixMath::MakeScaleMatrix(p.transform.scale);
                Matrix4x4 rotateZMatrix = MatrixMath::MakeRotateZMatrix(p.transform.rotate.z);
                Matrix4x4 translateMatrix = MatrixMath::MakeTranslateMatrix(p.transform.translate);
                worldMatrix = MatrixMath::Multiply(scaleMatrix,
                    MatrixMath::Multiply(rotateZMatrix,
                        MatrixMath::Multiply(billboardMatrix, translateMatrix)));
            }

            instancingData_[instanceIndex].World = worldMatrix;
            instancingData_[instanceIndex].WVP = MatrixMath::Multiply(worldMatrix, viewProjectionMatrix);
            instancingData_[instanceIndex].color = p.color;
			//UVスケールとオフセットをInstanceingDataに設定（xy: Scale、zw:Offset）
			instancingData_[instanceIndex].uvTransform = { p.uvScale.x, p.uvScale.y, p.uvOffset.x, p.uvOffset.y };
            instanceIndex++;
        }
        if (instanceIndex == 0) continue;
        commandList->SetGraphicsRootDescriptorTable(0, TextureManager::GetInstance()->GetGpuHandle(group.textureHandle));

        if (group.model) {
            group.model->Draw(commandList, instanceIndex);
        }
        else {
            commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
            commandList->DrawInstanced(6, instanceIndex, 0, 0);
        }
    }

    if (drawDefaultGPUParticles) {
        DrawGPUParticles();
    }
}

void ParticleManager::CreateGPUParticleResources() {
    gpuParticleResource_ = dxCommon_->CreateUAVBufferResource(
        sizeof(ParticleCS) * kMaxGPUParticleCount,
        D3D12_RESOURCE_STATE_COMMON);
    gpuParticleResourceState_ = D3D12_RESOURCE_STATE_COMMON;

    if(!gpuParticleResource_) throw graphics::InitializationError("Particle storage allocation failed");

    gpuParticleSrvHandle_ = srvManager_->Allocate();
    srvManager_->CreateSRVforStructuredBuffer(
        gpuParticleSrvHandle_,
        gpuParticleResource_.Get(),
        kMaxGPUParticleCount,
        sizeof(ParticleCS));

    gpuParticleUavHandle_ = srvManager_->Allocate();
    srvManager_->CreateUAVforStructuredBuffer(
        gpuParticleUavHandle_,
        gpuParticleResource_.Get(),
        kMaxGPUParticleCount,
        sizeof(ParticleCS));

    static_assert(sizeof(int32_t) == 4, "GPU free-list index must be 32-bit signed int.");
    gpuParticleFreeListIndexResource_ = dxCommon_->CreateUAVBufferResource(
        sizeof(int32_t),
        D3D12_RESOURCE_STATE_COMMON);
    if(!gpuParticleFreeListIndexResource_) throw graphics::InitializationError("Particle free list index allocation failed");
    gpuParticleFreeListIndexUavHandle_ = srvManager_->Allocate();
    srvManager_->CreateUAVforStructuredBuffer(
        gpuParticleFreeListIndexUavHandle_,
        gpuParticleFreeListIndexResource_.Get(),
        1,
        sizeof(int32_t));

    gpuParticleFreeListResource_ = dxCommon_->CreateUAVBufferResource(
        sizeof(uint32_t) * kMaxGPUParticleCount,
        D3D12_RESOURCE_STATE_COMMON);
    if(!gpuParticleFreeListResource_) throw graphics::InitializationError("Particle free list allocation failed");
    gpuParticleFreeListUavHandle_ = srvManager_->Allocate();
    srvManager_->CreateUAVforStructuredBuffer(
        gpuParticleFreeListUavHandle_,
        gpuParticleFreeListResource_.Get(),
        kMaxGPUParticleCount,
        sizeof(uint32_t));

    gpuParticleViewResource_ = dxCommon_->CreateBufferResource(sizeof(GPUParticleViewData));
    particle::MapUpload(gpuParticleViewResource_.Get(),gpuParticleViewData_,"gpuParticleView");
    gpuParticleViewData_->viewProjection = MatrixMath::MakeIdentity4x4();
    gpuParticleViewData_->billboardMatrix = MatrixMath::MakeIdentity4x4();

    gpuParticleUpdateInfoResource_ = dxCommon_->CreateBufferResource(sizeof(UpdateParticleInfo));
    particle::MapUpload(gpuParticleUpdateInfoResource_.Get(),gpuParticleUpdateInfo_,"gpuParticleUpdateInfo");
    gpuParticleUpdateInfo_->deltaTime = 1.0f / 60.0f;
    gpuParticleUpdateInfo_->particleCount = kMaxGPUParticleCount;
    gpuParticleUpdateInfo_->timeScale = 1.0f;
    gpuParticleUpdateInfo_->padding = 0;

    gpuParticleEmitterResource_ = dxCommon_->CreateBufferResource(sizeof(GPUParticleEmitSettings));
    particle::MapUpload(gpuParticleEmitterResource_.Get(),gpuParticleEmitter_,"gpuParticleEmitter");
    gpuParticleEmitter_->translate = { 0.0f, 0.0f, 0.0f };
    gpuParticleEmitter_->radius = 0.5f;
    gpuParticleEmitter_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    gpuParticleEmitter_->scale = { 0.5f, 0.5f, 0.5f };
    gpuParticleEmitter_->lifeTime = 1.0f;
    gpuParticleEmitter_->baseVelocity = { 0.0f, 0.0f, 0.0f };
    gpuParticleEmitter_->speed = 0.0f;
    gpuParticleEmitter_->count = 0;
    gpuParticleEmitter_->emit = 0;
    gpuParticleEmitter_->preset = 0;
    gpuParticleEmitter_->padding = 0;

    gpuParticleInteractionResource_ = dxCommon_->CreateBufferResource(sizeof(GPUParticleInteractionSettings));
    particle::MapUpload(gpuParticleInteractionResource_.Get(),gpuParticleInteraction_,"gpuParticleInteraction");
    gpuParticleInteraction_->gridCenter = { 0.0f, 0.0f, 8.0f };
    gpuParticleInteraction_->particleSize = 0.03f;
    gpuParticleInteraction_->gridCountX = 8;
    gpuParticleInteraction_->gridCountY = 8;
    gpuParticleInteraction_->gridCountZ = 8;
    gpuParticleInteraction_->particleCount = 512;
    gpuParticleInteraction_->brushPosition = { 0.0f, 0.0f, 8.0f };
    gpuParticleInteraction_->brushRadius = 1.0f;
    gpuParticleInteraction_->brushStrength = 1.0f;
    gpuParticleInteraction_->isPressed = 0;
    gpuParticleInteraction_->operation = static_cast<uint32_t>(InteractionBrushOperation::None);
    gpuParticleInteraction_->deltaTime = 1.0f / 60.0f;
    gpuParticleInteraction_->damping = 0.95f;
    gpuParticleInteraction_->padding0 = 0.0f;
    gpuParticleInteraction_->padding1 = 0.0f;
    gpuParticleInteraction_->padding2 = 0.0f;
}

void ParticleManager::CreateAccentFXResources() {
    accentParticleResource_ = dxCommon_->CreateUAVBufferResource(
        sizeof(ParticleCS) * kMaxAccentParticleCount,
        D3D12_RESOURCE_STATE_COMMON);
    accentParticleResourceState_ = D3D12_RESOURCE_STATE_COMMON;

    if(!accentParticleResource_) throw graphics::InitializationError("Accent particle storage allocation failed");
    accentParticleSrvHandle_ = srvManager_->Allocate();
    srvManager_->CreateSRVforStructuredBuffer(
        accentParticleSrvHandle_,
        accentParticleResource_.Get(),
        kMaxAccentParticleCount,
        sizeof(ParticleCS));

    accentParticleUavHandle_ = srvManager_->Allocate();
    srvManager_->CreateUAVforStructuredBuffer(
        accentParticleUavHandle_,
        accentParticleResource_.Get(),
        kMaxAccentParticleCount,
        sizeof(ParticleCS));

    accentFreeListIndexResource_ = dxCommon_->CreateUAVBufferResource(
        sizeof(int32_t),
        D3D12_RESOURCE_STATE_COMMON);
    accentFreeListIndexResourceState_ = D3D12_RESOURCE_STATE_COMMON;
    if(!accentFreeListIndexResource_) throw graphics::InitializationError("Accent free list index allocation failed");
    accentFreeListIndexUavHandle_ = srvManager_->Allocate();
    srvManager_->CreateUAVforStructuredBuffer(
        accentFreeListIndexUavHandle_,
        accentFreeListIndexResource_.Get(),
        1,
        sizeof(int32_t));

    accentFreeListResource_ = dxCommon_->CreateUAVBufferResource(
        sizeof(uint32_t) * kMaxAccentParticleCount,
        D3D12_RESOURCE_STATE_COMMON);
    accentFreeListResourceState_ = D3D12_RESOURCE_STATE_COMMON;
    if(!accentFreeListResource_) throw graphics::InitializationError("Accent free list allocation failed");
    accentFreeListUavHandle_ = srvManager_->Allocate();
    srvManager_->CreateUAVforStructuredBuffer(
        accentFreeListUavHandle_,
        accentFreeListResource_.Get(),
        kMaxAccentParticleCount,
        sizeof(uint32_t));

    accentEmitterResource_ = dxCommon_->CreateBufferResource(sizeof(GPUParticleEmitSettings));
    particle::MapUpload(accentEmitterResource_.Get(),accentEmitter_,"accentEmitter");
    *accentEmitter_ = {};

    cropBurstInfoResource_ = dxCommon_->CreateBufferResource(sizeof(CropBurstInfo));
    particle::MapUpload(cropBurstInfoResource_.Get(),cropBurstInfo_,"cropBurstInfo");
    *cropBurstInfo_ = {};
    cropBurstInfo_->particleCount = kMaxAccentParticleCount;
    cropBurstInfo_->deltaTime = 1.0f / 60.0f;
    cropBurstInfo_->timeScale = 1.0f;
}

void ParticleManager::CreateGPUParticleComputeRootSignature() {
    D3D12_DESCRIPTOR_RANGE particleUavRange{};
    particleUavRange.BaseShaderRegister = 0;
    particleUavRange.NumDescriptors = 1;
    particleUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    particleUavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE freeListIndexUavRange{};
    freeListIndexUavRange.BaseShaderRegister = 1;
    freeListIndexUavRange.NumDescriptors = 1;
    freeListIndexUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    freeListIndexUavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE freeListUavRange{};
    freeListUavRange.BaseShaderRegister = 2;
    freeListUavRange.NumDescriptors = 1;
    freeListUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    freeListUavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[3] = {};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[0].DescriptorTable.pDescriptorRanges = &particleUavRange;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[1].DescriptorTable.pDescriptorRanges = &freeListIndexUavRange;
    rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;

    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[2].DescriptorTable.pDescriptorRanges = &freeListUavRange;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;

    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);

    ComPtr<ID3DBlob> signatureBlob;
    ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA(static_cast<char*>(errorBlob->GetBufferPointer()));
        }
        assert(false);
    }
    hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&gpuParticleComputeRootSignature_));
    assert(SUCCEEDED(hr));
}

void ParticleManager::CreateGPUParticleComputePipelineState() {
    ComPtr<IDxcBlob> csBlob = dxCommon_->CompileShader(L"Resources/shader/InitializeParticle.CS.hlsl", L"cs_6_0");
    if (csBlob == nullptr) {
        Logger::Log("ParticleManager::CreateGPUParticleComputePipelineState failed: compute shader compile failed.\n");
        assert(false);
        return;
    }

    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = gpuParticleComputeRootSignature_.Get();
    psoDesc.CS = { csBlob->GetBufferPointer(), csBlob->GetBufferSize() };

    HRESULT hr = dxCommon_->GetDevice()->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&gpuParticleComputePipelineState_));
    assert(SUCCEEDED(hr));
}

void ParticleManager::CreateGPUParticleEmitComputeRootSignature() {
    D3D12_DESCRIPTOR_RANGE particleUavRange{};
    particleUavRange.BaseShaderRegister = 0;
    particleUavRange.NumDescriptors = 1;
    particleUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    particleUavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE freeListIndexUavRange{};
    freeListIndexUavRange.BaseShaderRegister = 1;
    freeListIndexUavRange.NumDescriptors = 1;
    freeListIndexUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    freeListIndexUavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE freeListUavRange{};
    freeListUavRange.BaseShaderRegister = 2;
    freeListUavRange.NumDescriptors = 1;
    freeListUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    freeListUavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[4] = {};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[0].DescriptorTable.pDescriptorRanges = &particleUavRange;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[1].DescriptorTable.pDescriptorRanges = &freeListIndexUavRange;
    rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;

    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[2].DescriptorTable.pDescriptorRanges = &freeListUavRange;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;

    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[3].Descriptor.ShaderRegister = 0;

    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);

    ComPtr<ID3DBlob> signatureBlob;
    ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA(static_cast<char*>(errorBlob->GetBufferPointer()));
        }
        assert(false);
    }
    hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&gpuParticleEmitComputeRootSignature_));
    assert(SUCCEEDED(hr));
}

void ParticleManager::CreateGPUParticleEmitComputePipelineState() {
    ComPtr<IDxcBlob> csBlob = dxCommon_->CompileShader(L"Resources/shader/EmitParticle.CS.hlsl", L"cs_6_0");
    if (csBlob == nullptr) {
        Logger::Log("ParticleManager::CreateGPUParticleEmitComputePipelineState failed: compute shader compile failed.\n");
        assert(false);
        return;
    }

    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = gpuParticleEmitComputeRootSignature_.Get();
    psoDesc.CS = { csBlob->GetBufferPointer(), csBlob->GetBufferSize() };

    HRESULT hr = dxCommon_->GetDevice()->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&gpuParticleEmitComputePipelineState_));
    assert(SUCCEEDED(hr));
}

void ParticleManager::CreateGPUParticleUpdateComputeRootSignature() {
    D3D12_DESCRIPTOR_RANGE particleUavRange{};
    particleUavRange.BaseShaderRegister = 0;
    particleUavRange.NumDescriptors = 1;
    particleUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    particleUavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE freeListIndexUavRange{};
    freeListIndexUavRange.BaseShaderRegister = 1;
    freeListIndexUavRange.NumDescriptors = 1;
    freeListIndexUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    freeListIndexUavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE freeListUavRange{};
    freeListUavRange.BaseShaderRegister = 2;
    freeListUavRange.NumDescriptors = 1;
    freeListUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    freeListUavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[4] = {};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[0].DescriptorTable.pDescriptorRanges = &particleUavRange;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[1].DescriptorTable.pDescriptorRanges = &freeListIndexUavRange;
    rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;

    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[2].DescriptorTable.pDescriptorRanges = &freeListUavRange;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;

    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[3].Descriptor.ShaderRegister = 0;

    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);

    ComPtr<ID3DBlob> signatureBlob;
    ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA(static_cast<char*>(errorBlob->GetBufferPointer()));
        }
        assert(false);
    }
    hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&gpuParticleUpdateComputeRootSignature_));
    assert(SUCCEEDED(hr));
}

void ParticleManager::CreateGPUParticleUpdateComputePipelineState() {
    ComPtr<IDxcBlob> csBlob = dxCommon_->CompileShader(L"Resources/shader/UpdateParticle.CS.hlsl", L"cs_6_0");
    if (csBlob == nullptr) {
        Logger::Log("ParticleManager::CreateGPUParticleUpdateComputePipelineState failed: compute shader compile failed.\n");
        assert(false);
        return;
    }

    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = gpuParticleUpdateComputeRootSignature_.Get();
    psoDesc.CS = { csBlob->GetBufferPointer(), csBlob->GetBufferSize() };

    HRESULT hr = dxCommon_->GetDevice()->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&gpuParticleUpdateComputePipelineState_));
    assert(SUCCEEDED(hr));
}

void ParticleManager::CreateGPUParticleInteractionComputeRootSignature() {
    D3D12_DESCRIPTOR_RANGE particleUavRange{};
    particleUavRange.BaseShaderRegister = 0;
    particleUavRange.NumDescriptors = 1;
    particleUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    particleUavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[2] = {};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[0].DescriptorTable.pDescriptorRanges = &particleUavRange;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[1].Descriptor.ShaderRegister = 0;

    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);

    ComPtr<ID3DBlob> signatureBlob;
    ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA(static_cast<char*>(errorBlob->GetBufferPointer()));
        }
        assert(false);
    }
    hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&gpuParticleInteractionComputeRootSignature_));
    assert(SUCCEEDED(hr));
}

void ParticleManager::CreateGPUParticleInteractionComputePipelineStates() {
    ComPtr<IDxcBlob> initializeCsBlob = dxCommon_->CompileShader(L"Resources/shader/InitializeInteractionParticle.CS.hlsl", L"cs_6_0");
    ComPtr<IDxcBlob> updateCsBlob = dxCommon_->CompileShader(L"Resources/shader/UpdateInteractionParticle.CS.hlsl", L"cs_6_0");
    if (initializeCsBlob == nullptr) {
        Logger::Log("ParticleManager::CreateGPUParticleInteractionComputePipelineStates failed: initialize compute shader compile failed.\n");
        assert(false);
        return;
    }
    if (updateCsBlob == nullptr) {
        Logger::Log("ParticleManager::CreateGPUParticleInteractionComputePipelineStates failed: update compute shader compile failed.\n");
        assert(false);
        return;
    }

    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = gpuParticleInteractionComputeRootSignature_.Get();

    psoDesc.CS = { initializeCsBlob->GetBufferPointer(), initializeCsBlob->GetBufferSize() };
    HRESULT hr = dxCommon_->GetDevice()->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&gpuParticleInteractionInitializeComputePipelineState_));
    assert(SUCCEEDED(hr));

    psoDesc.CS = { updateCsBlob->GetBufferPointer(), updateCsBlob->GetBufferSize() };
    hr = dxCommon_->GetDevice()->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&gpuParticleInteractionUpdateComputePipelineState_));
    assert(SUCCEEDED(hr));
}

void ParticleManager::CreateGPUParticleGraphicsRootSignature() {
    D3D12_DESCRIPTOR_RANGE textureRange{};
    textureRange.BaseShaderRegister = 0;
    textureRange.NumDescriptors = 1;
    textureRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    textureRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE particleSrvRange{};
    particleSrvRange.BaseShaderRegister = 0;
    particleSrvRange.NumDescriptors = 1;
    particleSrvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    particleSrvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[4] = {};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].DescriptorTable.pDescriptorRanges = &textureRange;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[1].Descriptor.ShaderRegister = 0;

    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[2].DescriptorTable.pDescriptorRanges = &particleSrvRange;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;

    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[3].Descriptor.ShaderRegister = 0;

    D3D12_STATIC_SAMPLER_DESC staticSampler{};
    staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSampler.ShaderRegister = 0;
    staticSampler.MaxLOD = D3D12_FLOAT32_MAX;
    staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);
    descriptionRootSignature.pStaticSamplers = &staticSampler;
    descriptionRootSignature.NumStaticSamplers = 1;

    ComPtr<ID3DBlob> signatureBlob;
    ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA(static_cast<char*>(errorBlob->GetBufferPointer()));
        }
        assert(false);
    }
    hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&gpuParticleGraphicsRootSignature_));
    assert(SUCCEEDED(hr));
}

void ParticleManager::CreateGPUParticleGraphicsPipelineState() {
    ComPtr<IDxcBlob> vsBlob = dxCommon_->CompileShader(L"Resources/shader/GPUParticle.VS.hlsl", L"vs_6_0");
    ComPtr<IDxcBlob> psBlob = dxCommon_->CompileShader(L"Resources/shader/Particle.PS.hlsl", L"ps_6_0");
    if (vsBlob == nullptr) {
        Logger::Log("ParticleManager::CreateGPUParticleGraphicsPipelineState failed: vertex shader compile failed.\n");
        assert(false);
        return;
    }
    if (psBlob == nullptr) {
        Logger::Log("ParticleManager::CreateGPUParticleGraphicsPipelineState failed: pixel shader compile failed.\n");
        assert(false);
        return;
    }

    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ZERO;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
    rasterizerDesc.DepthClipEnable = TRUE;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = gpuParticleGraphicsRootSignature_.Get();
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
    psoDesc.BlendState = blendDesc;
    psoDesc.RasterizerState = rasterizerDesc;
    psoDesc.DepthStencilState = { TRUE, D3D12_DEPTH_WRITE_MASK_ZERO, D3D12_COMPARISON_FUNC_LESS_EQUAL, FALSE };
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.NumRenderTargets = 1;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&gpuParticleGraphicsPipelineState_));
    assert(SUCCEEDED(hr));
}

void ParticleManager::CreateAccentFXComputePipelineStates() {
    ComPtr<IDxcBlob> initializeCsBlob = dxCommon_->CompileShader(L"Resources/shader/InitializeAccentParticle.CS.hlsl", L"cs_6_0");
    ComPtr<IDxcBlob> emitCsBlob = dxCommon_->CompileShader(L"Resources/shader/EmitAccentParticle.CS.hlsl", L"cs_6_0");
    ComPtr<IDxcBlob> updateCsBlob = dxCommon_->CompileShader(L"Resources/shader/UpdateAccentParticle.CS.hlsl", L"cs_6_0");

    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = gpuParticleComputeRootSignature_.Get();
    psoDesc.CS = { initializeCsBlob->GetBufferPointer(), initializeCsBlob->GetBufferSize() };
    HRESULT hr = dxCommon_->GetDevice()->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&accentInitializeComputePipelineState_));
    assert(SUCCEEDED(hr));

    psoDesc.pRootSignature = gpuParticleEmitComputeRootSignature_.Get();
    psoDesc.CS = { emitCsBlob->GetBufferPointer(), emitCsBlob->GetBufferSize() };
    hr = dxCommon_->GetDevice()->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&accentEmitComputePipelineState_));
    assert(SUCCEEDED(hr));

    psoDesc.pRootSignature = gpuParticleUpdateComputeRootSignature_.Get();
    psoDesc.CS = { updateCsBlob->GetBufferPointer(), updateCsBlob->GetBufferSize() };
    hr = dxCommon_->GetDevice()->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&accentUpdateComputePipelineState_));
    assert(SUCCEEDED(hr));
}

void ParticleManager::InitializeGPUParticles() {
    if (gpuParticlesInitialized_) {
        return;
    }

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    TransitionResource(
        commandList,
        gpuParticleResource_.Get(),
        gpuParticleResourceState_,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    gpuParticleResourceState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    TransitionResource(
        commandList,
        gpuParticleFreeListIndexResource_.Get(),
        gpuParticleFreeListIndexResourceState_,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    gpuParticleFreeListIndexResourceState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    TransitionResource(
        commandList,
        gpuParticleFreeListResource_.Get(),
        gpuParticleFreeListResourceState_,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    gpuParticleFreeListResourceState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

    commandList->SetComputeRootSignature(gpuParticleComputeRootSignature_.Get());
    commandList->SetPipelineState(gpuParticleComputePipelineState_.Get());
    commandList->SetComputeRootDescriptorTable(0, srvManager_->GetGPUDescriptorHandle(gpuParticleUavHandle_));
    commandList->SetComputeRootDescriptorTable(1, srvManager_->GetGPUDescriptorHandle(gpuParticleFreeListIndexUavHandle_));
    commandList->SetComputeRootDescriptorTable(2, srvManager_->GetGPUDescriptorHandle(gpuParticleFreeListUavHandle_));
    commandList->Dispatch(1, 1, 1);

    InsertUAVBarrier(commandList, nullptr);
    gpuParticlesInitialized_ = true;
}

void ParticleManager::EmitGPUParticles() {
    if (frameDeltaTime_ <= 0.0f ||
        !gpuParticleEmitter_ ||
        gpuParticleEmitter_->emit == 0 ||
        gpuParticleEmitter_->count == 0) {
        return;
    }

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    TransitionResource(
        commandList,
        gpuParticleResource_.Get(),
        gpuParticleResourceState_,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    gpuParticleResourceState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

    commandList->SetComputeRootSignature(gpuParticleEmitComputeRootSignature_.Get());
    commandList->SetPipelineState(gpuParticleEmitComputePipelineState_.Get());
    commandList->SetComputeRootDescriptorTable(0, srvManager_->GetGPUDescriptorHandle(gpuParticleUavHandle_));
    commandList->SetComputeRootDescriptorTable(1, srvManager_->GetGPUDescriptorHandle(gpuParticleFreeListIndexUavHandle_));
    commandList->SetComputeRootDescriptorTable(2, srvManager_->GetGPUDescriptorHandle(gpuParticleFreeListUavHandle_));
    commandList->SetComputeRootConstantBufferView(3, gpuParticleEmitterResource_->GetGPUVirtualAddress());
    constexpr uint32_t kThreadCount = 256;
    const uint32_t groupCount = (gpuParticleEmitter_->count + kThreadCount - 1) / kThreadCount;
    commandList->Dispatch((std::max)(1u, groupCount), 1, 1);

    InsertUAVBarrier(commandList, nullptr);
}

void ParticleManager::UpdateGPUParticles() {
    assert(gpuParticleUpdateInfo_);
	if (frameDeltaTime_ <= 0.0f) {
		ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
		// Draw still reads the buffer as an SRV while simulation is paused.
		TransitionResource(
			commandList,
			gpuParticleResource_.Get(),
			gpuParticleResourceState_,
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		gpuParticleResourceState_ = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		return;
	}
	gpuParticleUpdateInfo_->deltaTime = frameDeltaTime_;
    gpuParticleUpdateInfo_->particleCount = kMaxGPUParticleCount;
    gpuParticleUpdateInfo_->timeScale = 1.0f;
    gpuParticleUpdateInfo_->padding = 0;

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    TransitionResource(
        commandList,
        gpuParticleResource_.Get(),
        gpuParticleResourceState_,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    gpuParticleResourceState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

    commandList->SetComputeRootSignature(gpuParticleUpdateComputeRootSignature_.Get());
    commandList->SetPipelineState(gpuParticleUpdateComputePipelineState_.Get());
    commandList->SetComputeRootDescriptorTable(0, srvManager_->GetGPUDescriptorHandle(gpuParticleUavHandle_));
    commandList->SetComputeRootDescriptorTable(1, srvManager_->GetGPUDescriptorHandle(gpuParticleFreeListIndexUavHandle_));
    commandList->SetComputeRootDescriptorTable(2, srvManager_->GetGPUDescriptorHandle(gpuParticleFreeListUavHandle_));
    commandList->SetComputeRootConstantBufferView(3, gpuParticleUpdateInfoResource_->GetGPUVirtualAddress());

    constexpr uint32_t kThreadCount = 256;
    const uint32_t dispatchCount = (kMaxGPUParticleCount + kThreadCount - 1) / kThreadCount;
    commandList->Dispatch(dispatchCount, 1, 1);

    InsertUAVBarrier(commandList, nullptr);

    TransitionResource(
        commandList,
        gpuParticleResource_.Get(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    gpuParticleResourceState_ = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
}

void ParticleManager::InitializeGPUParticleInteraction(const GPUParticleInteractionSettings& settings) {
    if (!gpuParticleInteraction_ || !gpuParticleResource_) {
        return;
    }

    GPUParticleInteractionSettings sanitizedSettings = SanitizeInteractionSettings(settings, kMaxGPUParticleCount);
    *gpuParticleInteraction_ = sanitizedSettings;

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    TransitionResource(
        commandList,
        gpuParticleResource_.Get(),
        gpuParticleResourceState_,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    gpuParticleResourceState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

    commandList->SetComputeRootSignature(gpuParticleInteractionComputeRootSignature_.Get());
    commandList->SetPipelineState(gpuParticleInteractionInitializeComputePipelineState_.Get());
    commandList->SetComputeRootDescriptorTable(0, srvManager_->GetGPUDescriptorHandle(gpuParticleUavHandle_));
    commandList->SetComputeRootConstantBufferView(1, gpuParticleInteractionResource_->GetGPUVirtualAddress());

    constexpr uint32_t kThreadCount = 256;
    const uint32_t dispatchCount = (kMaxGPUParticleCount + kThreadCount - 1) / kThreadCount;
    commandList->Dispatch(dispatchCount, 1, 1);
    InsertUAVBarrier(commandList, gpuParticleResource_.Get());

    gpuParticleInteractionInitialized_ = true;
}

void ParticleManager::UpdateGPUParticleInteraction(const GPUParticleInteractionSettings& settings) {
    if (!gpuParticleInteraction_ || !gpuParticleResource_) {
        return;
    }

    GPUParticleInteractionSettings sanitizedSettings = SanitizeInteractionSettings(settings, kMaxGPUParticleCount);
    if (!gpuParticleInteractionInitialized_) {
        InitializeGPUParticleInteraction(sanitizedSettings);
    }
    *gpuParticleInteraction_ = sanitizedSettings;

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
	if (sanitizedSettings.deltaTime <= 0.0f) {
		// Preserve the draw-ready state without dispatching a zero-delta update.
		TransitionResource(
			commandList,
			gpuParticleResource_.Get(),
			gpuParticleResourceState_,
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		gpuParticleResourceState_ = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		return;
	}
    TransitionResource(
        commandList,
        gpuParticleResource_.Get(),
        gpuParticleResourceState_,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    gpuParticleResourceState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

    commandList->SetComputeRootSignature(gpuParticleInteractionComputeRootSignature_.Get());
    commandList->SetPipelineState(gpuParticleInteractionUpdateComputePipelineState_.Get());
    commandList->SetComputeRootDescriptorTable(0, srvManager_->GetGPUDescriptorHandle(gpuParticleUavHandle_));
    commandList->SetComputeRootConstantBufferView(1, gpuParticleInteractionResource_->GetGPUVirtualAddress());

    constexpr uint32_t kThreadCount = 256;
    const uint32_t dispatchCount = (sanitizedSettings.particleCount + kThreadCount - 1) / kThreadCount;
    commandList->Dispatch((std::max)(1u, dispatchCount), 1, 1);
    InsertUAVBarrier(commandList, gpuParticleResource_.Get());

    TransitionResource(
        commandList,
        gpuParticleResource_.Get(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    gpuParticleResourceState_ = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
}

void ParticleManager::DrawGPUParticleBuffer() {
    if (!camera_) {
        return;
    }

    Texture2DHandle textureHandle{};
    if (!TryGetGPUParticleTextureHandle(textureHandle)) {
        return;
    }

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    TransitionResource(
        commandList,
        gpuParticleResource_.Get(),
        gpuParticleResourceState_,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    gpuParticleResourceState_ = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

    Matrix4x4 viewMatrix = camera_->GetViewMatrix();
    Matrix4x4 billboardMatrix = MatrixMath::MakeIdentity4x4();
    billboardMatrix.m[0][0] = viewMatrix.m[0][0]; billboardMatrix.m[0][1] = viewMatrix.m[1][0]; billboardMatrix.m[0][2] = viewMatrix.m[2][0];
    billboardMatrix.m[1][0] = viewMatrix.m[0][1]; billboardMatrix.m[1][1] = viewMatrix.m[1][1]; billboardMatrix.m[1][2] = viewMatrix.m[2][1];
    billboardMatrix.m[2][0] = viewMatrix.m[0][2]; billboardMatrix.m[2][1] = viewMatrix.m[1][2]; billboardMatrix.m[2][2] = viewMatrix.m[2][2];

    gpuParticleViewData_->viewProjection = camera_->GetViewProjectionMatrix();
    gpuParticleViewData_->billboardMatrix = billboardMatrix;

    commandList->SetGraphicsRootSignature(gpuParticleGraphicsRootSignature_.Get());
    commandList->SetPipelineState(gpuParticleGraphicsPipelineState_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->SetGraphicsRootDescriptorTable(0, TextureManager::GetInstance()->GetGpuHandle(textureHandle));
    commandList->SetGraphicsRootConstantBufferView(1, materialResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootDescriptorTable(2, srvManager_->GetGPUDescriptorHandle(gpuParticleSrvHandle_));
    commandList->SetGraphicsRootConstantBufferView(3, gpuParticleViewResource_->GetGPUVirtualAddress());
    commandList->DrawInstanced(6, kMaxGPUParticleCount, 0, 0);
}

void ParticleManager::DrawGPUParticles() {
    InitializeGPUParticles();
    EmitGPUParticles();
    UpdateGPUParticles();
    DrawGPUParticleBuffer();
}

bool ParticleManager::TryGetGPUParticleTextureHandle(Texture2DHandle& textureHandle) const {
    for (const auto& groupPair : particleGroups_) {
        if (!groupPair.second.model) {
            textureHandle = groupPair.second.textureHandle;
            return true;
        }
    }

    if (!particleGroups_.empty()) {
        textureHandle = particleGroups_.begin()->second.textureHandle;
        return true;
    }

    return false;
}

void ParticleManager::CreateParticleGroup(const std::string& name, Texture2DHandle textureHandle, Model* model) {
    ParticleGroup& group = particleGroups_[name];
    group.name = name;
    group.textureHandle = textureHandle;
	group.model = model;
}

void ParticleManager::CreateRootSignature() {
    D3D12_DESCRIPTOR_RANGE descriptorRange{};
    descriptorRange.BaseShaderRegister = 0;
    descriptorRange.NumDescriptors = 1;
    descriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // === Root Parameter の定義 ===
    // [0] テクスチャ用のSRV（既存）
    // [1] マテリアル定数バッファ用のCBV（新規追加 — alphaReferenceを渡す）
    D3D12_ROOT_PARAMETER rootParameters[2] = {};

    // Root Parameter 0: テクスチャ（ピクセルシェーダーの register(t0)）
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].DescriptorTable.pDescriptorRanges = &descriptorRange;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;

    // Root Parameter 1: マテリアルCBuffer（ピクセルシェーダーの register(b0)）
    // alphaReference をシェーダーに渡すために追加
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[1].Descriptor.ShaderRegister = 0; // register(b0)

    D3D12_STATIC_SAMPLER_DESC staticSampler{};
    staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;   // 横方向はループさせる
    staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;  // 縦方向（内側・外側）は端で止める
    staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSampler.ShaderRegister = 0;
    staticSampler.MaxLOD = D3D12_FLOAT32_MAX;
    staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);
    descriptionRootSignature.pStaticSamplers = &staticSampler;
    descriptionRootSignature.NumStaticSamplers = 1;

    ComPtr<ID3DBlob> signatureBlob, errorBlob;
    D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
}

void ParticleManager::CreateGraphicsPipelineState() {
    ComPtr<IDxcBlob> vsBlob = dxCommon_->CompileShader(L"Resources/shader/Particle.VS.hlsl", L"vs_6_0");
    ComPtr<IDxcBlob> psBlob = dxCommon_->CompileShader(L"Resources/shader/Particle.PS.hlsl", L"ps_6_0");
    if (vsBlob == nullptr) {
        Logger::Log("ParticleManager::CreateGraphicsPipelineState failed: vertex shader compile failed.\n");
        assert(false);
        return;
    }
    if (psBlob == nullptr) {
        Logger::Log("ParticleManager::CreateGraphicsPipelineState failed: pixel shader compile failed.\n");
        assert(false);
        return;
    }

    // Input Layout のメモリ配置は Model::VertexData と同じ順序にする
    // position(float4) → normal(float3) → texcoord(float2)
    // ※ 以前は TEXCOORD と NORMAL が逆順で、3Dモデル(Cylinder等)のUVが壊れていた
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "WVP",      0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "WVP",      1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "WVP",      2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "WVP",      3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "WORLD",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "WORLD",    1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "WORLD",    2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "WORLD",    3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        // UVスクロール・スケール用のパラメータ (TEXCOORDの1番として送る)
        { "TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
    };

    // 明示的なブレンド設定の代入
    D3D12_BLEND_DESC blendDesc{};
    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;
    blendDesc.RenderTarget[0].BlendEnable = TRUE;

    // カラーの加算設定 (Src * SrcAlpha + Dest * 1)
    blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;

    // 【修正箇所】アルファは描画先の値を維持させる (Src * 0 + Dest * 1)
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ZERO;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;

    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    // ★真相ポイント2：ラスタライザ設定を完全に固定
    D3D12_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE; // カリングなし
    rasterizerDesc.FrontCounterClockwise = FALSE;
    rasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    rasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    rasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    rasterizerDesc.DepthClipEnable = TRUE;
    rasterizerDesc.MultisampleEnable = FALSE;
    rasterizerDesc.AntialiasedLineEnable = FALSE;
    rasterizerDesc.ForcedSampleCount = 0;
    rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = rootSignature_.Get();
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
    psoDesc.BlendState = blendDesc;
    psoDesc.RasterizerState = rasterizerDesc;
    psoDesc.DepthStencilState = { TRUE, D3D12_DEPTH_WRITE_MASK_ZERO, D3D12_COMPARISON_FUNC_LESS_EQUAL, FALSE };
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    // RTV形式は Game.cpp の srvManager_->CreateSRVforTexture2D に渡している形式と一致させる必要があります
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.NumRenderTargets = 1;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
    assert(SUCCEEDED(hr));
}

void ParticleManager::CreateModel() {
    // 頂点データも VertexData の新しいフィールド順（position, normal, texcoord）に合わせる
    VertexData vertices6[] = {
        {{ -0.5f, -0.5f, 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f }},
        {{ -0.5f,  0.5f, 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f }},
        {{  0.5f, -0.5f, 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 1.0f }},
        {{ -0.5f,  0.5f, 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f }},
        {{  0.5f,  0.5f, 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f }},
        {{  0.5f, -0.5f, 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 1.0f }},
    };
    vertexResource_ = dxCommon_->CreateBufferResource(sizeof(VertexData) * 6);
    VertexData* data=nullptr;
    particle::MapUpload(vertexResource_.Get(),data,"quad vertices");
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = sizeof(VertexData) * 6;
    vertexBufferView_.StrideInBytes = sizeof(VertexData);
    std::memcpy(data, vertices6, sizeof(VertexData) * 6);
    vertexResource_->Unmap(0, nullptr);
}
