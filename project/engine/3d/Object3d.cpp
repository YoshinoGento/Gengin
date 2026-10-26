#include "3d/Object3d.h"
#include "3d/Object3dCommon.h"
#include "3d/LightingSystem.h"
#include "base/SrvManager.h"
#include "base/DirectXCommon.h"
#include "2d/TextureManager.h"
#include "base/Logger.h"
#include "base/FrameClock.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <new>
#include <stdexcept>
#include <utility>

namespace {
	constexpr float kMinimumScaleMagnitude = 1.0e-4f;

	float Determinant3x3(const Matrix4x4& matrix) {
		return
			matrix.m[0][0] * (matrix.m[1][1] * matrix.m[2][2] - matrix.m[1][2] * matrix.m[2][1]) -
			matrix.m[0][1] * (matrix.m[1][0] * matrix.m[2][2] - matrix.m[1][2] * matrix.m[2][0]) +
			matrix.m[0][2] * (matrix.m[1][0] * matrix.m[2][1] - matrix.m[1][1] * matrix.m[2][0]);
	}

	int ResolveCullMode(int requestedCullMode, bool isMirrored) {
		if (!isMirrored || requestedCullMode == 0) {
			return requestedCullMode;
		}
		return requestedCullMode == 1 ? 2 : 1;
	}
}

void Object3d::Initialize(Object3dCommon* object3dCommon) {
	if (!object3dCommon || !object3dCommon->GetDxCommon() || constantBuffer_) {
        throw std::logic_error("Object3d::Initialize requires valid dependencies and no live buffer");
    }
	DirectXCommon* dxCommon = object3dCommon->GetDxCommon();
    auto buffer = dxCommon->CreateBufferResource(sizeof(graphics::ObjectBufferData));
    if (!buffer) throw std::runtime_error("Object3d constant buffer allocation failed");
    void* mapped = nullptr;
    const D3D12_RANGE noRead{0, 0};
    if (FAILED(buffer->Map(0, &noRead, &mapped)) || !mapped)
        throw std::runtime_error("Object3d constant buffer Map failed");
    auto* data = ::new(mapped) graphics::ObjectBufferData{};
    constantBuffer_ = std::move(buffer);
    materialData_ = &data->material;
    transformationMatrixData_ = &data->transform;
    shadowTransformationMatrixData_ = &data->shadow;
	object3dCommon_ = object3dCommon;
	textureHandle_ = TextureManager::GetInstance()->GetFallback2D();
	environmentMapHandle_ = TextureManager::GetInstance()->GetFallbackCube();
	materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	materialData_->enableLighting = 1;
	materialData_->shininess = 40.0f;
	materialData_->environmentCoefficient = 0.0f;
	materialData_->specularType = static_cast<int32_t>(Object3d::SpecularType::BlinnPhong);
	materialData_->uvTransform = MatrixMath::MakeIdentity4x4();
	materialData_->surfaceDetail = {1.0f, 0.0f, 0.0f, 0.0f};
	materialData_->outdoorAmbient = {};

	const Matrix4x4 identity = MatrixMath::MakeIdentity4x4();
	*transformationMatrixData_ = { identity, identity, identity };
	*shadowTransformationMatrixData_ = { identity, identity, identity };

}

bool Object3d::SetOutdoorAmbient(const Vector4& skyTint) {
	if (!constantBuffer_ || !materialData_ || !std::isfinite(skyTint.x) || !std::isfinite(skyTint.y) ||
		!std::isfinite(skyTint.z) || !std::isfinite(skyTint.w)) return false;
	materialData_->outdoorAmbient = {std::clamp(skyTint.x,0.0f,1.0f),
		std::clamp(skyTint.y,0.0f,1.0f),std::clamp(skyTint.z,0.0f,1.0f),std::clamp(skyTint.w,0.0f,1.0f)};
	return true;
}

bool Object3d::SetSurfaceDetail(const Vector4& detail) {
	if (!constantBuffer_ || !materialData_ || !std::isfinite(detail.x) || !std::isfinite(detail.y) ||
		!std::isfinite(detail.z) || !std::isfinite(detail.w)) { return false; }
	materialData_->surfaceDetail = {
		std::clamp(detail.x, 0.0f, 1.0f), std::clamp(detail.y, 0.0f, 0.5f),
		std::clamp(detail.z, 0.0f, 64.0f), std::clamp(detail.w, 0.0f, 0.3f)};
	return true;
}

void Object3d::Update(Camera* camera) {
	Update(camera, 0.0f);
}

void Object3d::Update(Camera* camera, float deltaTime) {
	if (!constantBuffer_ || !camera) return;
	computeSkinningPrepared_ = false;
	if (!std::isfinite(deltaTime)) {
		deltaTime = 0.0f;
	}
	deltaTime = std::clamp(deltaTime, 0.0f, FrameClock::kMaximumFrameDeltaSeconds);

	if (isAnimationPlaying_ && animation_.duration > 0.0f) {
		animationTime_ += deltaTime * animationSpeed_;
		animationTime_ = std::fmod(animationTime_, animation_.duration);
		if (animationTime_ < 0.0f) {
			animationTime_ += animation_.duration;
		}

		// --- フェーズ1：アニメーションの適用 ---
		// 手動コントロールフラグがオフのときのみアニメーションを骨に適用する
		if (skeleton_ && !isBoneManualControl_) {
			SkeletonSystem::ApplyAnimation(*skeleton_, animation_, animationTime_);
		}
	}

	// --- フェーズ2：スケルトンの更新 ---
	// 手動で書き換えた骨のTransformに基づいて、行列階層を正しく再計算する
	if (skeleton_) {
		SkeletonSystem::Update(*skeleton_);
		if (model_ && model_->HasSkinCluster()) {
			model_->UpdateSkinCluster(*skeleton_);
		}
	}

	// --- 1. オブジェクト自身の変形行列を作る ---
	Matrix4x4 scaleMatrix = MatrixMath::MakeScaleShearYMatrix(transform_.scale, shearY_);
	Matrix4x4 rotateMatrix = MatrixMath::Multiply(MatrixMath::MakeRotateXMatrix(transform_.rotate.x),
		MatrixMath::Multiply(MatrixMath::MakeRotateYMatrix(transform_.rotate.y),
			MatrixMath::MakeRotateZMatrix(transform_.rotate.z)));
	Matrix4x4 translateMatrix = MatrixMath::MakeTranslateMatrix(transform_.translate);
	// ★修正：ルートノードのローカル行列を含まない、オブジェクト自身の変換行列を保持する
	objectWorldMatrix_ = MatrixMath::Multiply(scaleMatrix, MatrixMath::Multiply(rotateMatrix, translateMatrix));
	worldMatrix_ = objectWorldMatrix_;

	// --- 2. モデル内の階層構造（ノード）の行列を合成する ---
	if (model_ && skeleton_) {
		const Model::Node &rootNode = model_->GetRootNode();
		worldMatrix_ = MatrixMath::Multiply(rootNode.localMatrix, worldMatrix_);
	}

	// --- 3. 定数バッファへの書き込み ---
	const Matrix4x4& matrixForRendering = (model_ && model_->HasSkinCluster()) ? objectWorldMatrix_ : worldMatrix_;
	transformationMatrixData_->WVP = MatrixMath::Multiply(matrixForRendering, camera->GetViewProjectionMatrix());
	transformationMatrixData_->World = matrixForRendering;
	const float worldDeterminant = Determinant3x3(matrixForRendering);
	transformationMatrixData_->WorldInverseTranspose = std::abs(worldDeterminant) >= 1.0e-8f
		? MatrixMath::Transpose(MatrixMath::Inverse(matrixForRendering))
		: MatrixMath::MakeIdentity4x4();
	isMirrored_ = worldDeterminant < 0.0f;

	// カメラ座標の更新

	// --- 4. スポットライトの更新 (正規化の追加) ---
	// ImGuiなどで方向が変更された場合、計算に使う前に必ず長さを 1 にします
	// これを行わないと、シェーダー側で角度による減衰計算が破綻します
}

void Object3d::SetTexture(uint32_t textureHandle) {
	textureHandle_ = TextureManager::GetInstance()->GetTexture2DHandle(textureHandle);
}

void Object3d::Draw() {
	if (!constantBuffer_ || !object3dCommon_ || !model_) return;
	if (!object3dCommon_->IsObjectPassActive()) {
		Logger::Log("Object3d::Draw rejected because BeginObjectPass was not called.");
		assert(false && "Object3d draw outside object pass");
		return;
	}
	ID3D12GraphicsCommandList* commandList = object3dCommon_->GetDxCommon()->GetCommandList();
	const bool isSkinned = model_->HasSkinCluster();
	const bool useComputeSkinning = model_->UseComputeSkinning();
	const bool useVertexShaderSkinning = isSkinned && !useComputeSkinning;

	if (useComputeSkinning && !computeSkinningPrepared_) {
		model_->DispatchComputeSkinning(object3dCommon_);
		computeSkinningPrepared_ = true;
	}

	// 3Dオブジェクト用のルートシグネチャを明示的にセットする
	commandList->SetGraphicsRootSignature(object3dCommon_->GetRootSignature(useVertexShaderSkinning));

	// パイプラインをセット
	commandList->SetPipelineState(object3dCommon_->GetPipelineState(
		ResolveCullMode(cullMode_, isMirrored_), useVertexShaderSkinning));

	// RootSignature の Index に合わせて定数バッファをセット
	const auto address = constantBuffer_->GetGPUVirtualAddress();
	commandList->SetGraphicsRootConstantBufferView(0, address + offsetof(graphics::ObjectBufferData, material));
	commandList->SetGraphicsRootConstantBufferView(1, address + offsetof(graphics::ObjectBufferData, transform));
	LightingSystem* lightingSystem = object3dCommon_->GetLightingSystem();
	assert(lightingSystem != nullptr);
	commandList->SetGraphicsRootConstantBufferView(2, lightingSystem->GetDirectionalLightAddress());
	commandList->SetGraphicsRootConstantBufferView(3, lightingSystem->GetCameraAddress());
	commandList->SetGraphicsRootConstantBufferView(4, lightingSystem->GetSpotLightAddress());

	SrvManager* srvManager = object3dCommon_->GetSrvManager();
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle = TextureManager::GetInstance()->GetGpuHandle(textureHandle_);
	commandList->SetGraphicsRootDescriptorTable(5, textureSrvHandle);

	D3D12_GPU_DESCRIPTOR_HANDLE environmentMapSrvHandle = TextureManager::GetInstance()->GetGpuHandle(environmentMapHandle_);
	commandList->SetGraphicsRootDescriptorTable(6, environmentMapSrvHandle);
	if (object3dCommon_->IsShadowReady()) {
		commandList->SetGraphicsRootConstantBufferView(7, object3dCommon_->GetShadowSceneBufferAddress());
		commandList->SetGraphicsRootDescriptorTable(8, object3dCommon_->GetShadowMapSrvHandle());
	}

	if (useVertexShaderSkinning) {
		const Model::SkinCluster& skinCluster = model_->GetSkinCluster();
		if (skinCluster.paletteSrvHandle == Model::SkinCluster::kInvalidSrvHandle) {
			OutputDebugStringA("Skinned mesh has invalid MatrixPalette SRV handle.\n");
			assert(false && "Skinned mesh has invalid MatrixPalette SRV handle.");
			return;
		}
		commandList->SetGraphicsRootDescriptorTable(9, srvManager->GetGPUDescriptorHandle(skinCluster.paletteSrvHandle));
	}

	// モデルの描画実行
	model_->Draw(object3dCommon_->GetDxCommon());
}

void Object3d::DrawShadow() {
	if (!constantBuffer_ || !model_ || !object3dCommon_ || !object3dCommon_->IsShadowReady()) {
		return;
	}

	ID3D12GraphicsCommandList* commandList = object3dCommon_->GetDxCommon()->GetCommandList();
	const bool isSkinned = model_->HasSkinCluster();
	const bool useComputeSkinning = model_->UseComputeSkinning();
	const bool useVertexShaderSkinning = isSkinned && !useComputeSkinning;
	if (useComputeSkinning && !computeSkinningPrepared_) {
		model_->DispatchComputeSkinning(object3dCommon_);
		computeSkinningPrepared_ = true;
	}

	const Matrix4x4& matrixForRendering = isSkinned ? objectWorldMatrix_ : worldMatrix_;
	shadowTransformationMatrixData_->WVP = MatrixMath::Multiply(
		matrixForRendering, object3dCommon_->GetLightViewProjectionMatrix());
	shadowTransformationMatrixData_->World = matrixForRendering;
	const float shadowWorldDeterminant = Determinant3x3(matrixForRendering);
	shadowTransformationMatrixData_->WorldInverseTranspose = std::abs(shadowWorldDeterminant) >= 1.0e-8f
		? MatrixMath::Transpose(MatrixMath::Inverse(matrixForRendering))
		: MatrixMath::MakeIdentity4x4();

	commandList->SetGraphicsRootSignature(object3dCommon_->GetShadowRootSignature());
	commandList->SetPipelineState(object3dCommon_->GetShadowPipelineState(useVertexShaderSkinning, isMirrored_));
	commandList->SetGraphicsRootConstantBufferView(
		0, constantBuffer_->GetGPUVirtualAddress() + offsetof(graphics::ObjectBufferData, shadow));

	if (useVertexShaderSkinning) {
		const Model::SkinCluster& skinCluster = model_->GetSkinCluster();
		if (skinCluster.paletteSrvHandle == Model::SkinCluster::kInvalidSrvHandle) {
			OutputDebugStringA("Shadow pass skipped: invalid MatrixPalette SRV handle.\n");
			assert(false && "Skinned shadow caster has invalid MatrixPalette SRV handle.");
			return;
		}
		commandList->SetGraphicsRootDescriptorTable(
			1, object3dCommon_->GetSrvManager()->GetGPUDescriptorHandle(skinCluster.paletteSrvHandle));
	}

	model_->DrawDepth(object3dCommon_->GetDxCommon());
}

void Object3d::SetModel(Model* model) {
	model_ = model;
	if (model_) {
		InitializeSkeleton();
	}
}

bool Object3d::SetShearY(const Vector2& slope) {
	if (!std::isfinite(slope.x) || !std::isfinite(slope.y)) { return false; }
	shearY_ = slope;
	return true;
}

bool Object3d::SetScale(const Vector3& scale) {
	if (std::abs(scale.x) < kMinimumScaleMagnitude ||
		std::abs(scale.y) < kMinimumScaleMagnitude ||
		std::abs(scale.z) < kMinimumScaleMagnitude) {
		Logger::Log("Object3d::SetScale rejected a near-zero component because its normal matrix would be singular.");
		return false;
	}
	transform_.scale = scale;
	return true;
}

void Object3d::SetCullMode(int cullMode) {
	cullMode_ = std::clamp(cullMode, 0, 2);
}

void Object3d::InitializeSkeleton() {
	if (model_) {
		skeleton_ = SkeletonSystem::CreateSkeleton(model_->GetRootNode());
	}
}

// These commands never expose the mapped constant buffer or mutable skeleton storage.
bool Object3d::SetAnimationTime(float seconds) noexcept {
    if (!std::isfinite(seconds)) return false;
    animationTime_ = std::clamp(seconds, 0.0f, (std::max)(0.0f, animation_.duration));
    return true;
}
bool Object3d::SetPlaybackSpeed(float speed) noexcept {
    if (!std::isfinite(speed)) return false;
    animationSpeed_ = speed;
    return true;
}
bool Object3d::SetJointTranslationScale(std::size_t index, const Vector3& translation, const Vector3& scale) {
    const auto finite = [](const Vector3& v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); };
    if (!skeleton_ || index >= skeleton_->joints.size() || !finite(translation) || !finite(scale) ||
        scale.x <= 0.0f || scale.y <= 0.0f || scale.z <= 0.0f) return false;
    auto& transform = skeleton_->joints[index].transform;
    transform.translate = translation;
    transform.scale = scale;
    return true;
}
void Object3d::SetColor(const Vector4& color) {
    if (constantBuffer_ && materialData_ && std::isfinite(color.x) && std::isfinite(color.y) && std::isfinite(color.z) && std::isfinite(color.w))
        materialData_->color = color;
}
void Object3d::SetEnvironmentCoefficient(float coefficient) {
    if (constantBuffer_ && materialData_ && std::isfinite(coefficient)) materialData_->environmentCoefficient = coefficient;
}
void Object3d::SetShininess(float shininess) {
    if (constantBuffer_ && materialData_ && std::isfinite(shininess) && shininess >= 0.0f) materialData_->shininess = shininess;
}
