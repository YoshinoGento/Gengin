#pragma once
#include "3d/Object3dCommon.h"
#include "3d/Model.h"
#include "3d/ObjectBufferLayout.h"
#include "math/Matrix.h"
#include "3d/Camera.h"
#include "3d/Skeleton.h"
#include "2d/TextureManager.h"
#include <cstdint>
#include <cstddef>
#include <optional>
#include <wrl.h>
#include <d3d12.h>


/**
 * Object3dクラス
 * スポットライトの管理機能を追加
 */
class Object3d {
public:
    Object3d() = default;
    Object3d(const Object3d&) = delete;
    Object3d& operator=(const Object3d&) = delete;
    Object3d(Object3d&&) noexcept = default;
    Object3d& operator=(Object3d&&) noexcept = default;
	enum class SpecularType : int32_t {
		Phong = 0,
		BlinnPhong = 1,
		None = 2,
	};

	// シェーダーと一致させる構造体 (16バイト境界に注意)
	void Initialize(Object3dCommon* object3dCommon);
	void Update(Camera* camera, float deltaTime);
	void Update(Camera* camera);
	void Draw();
	void DrawShadow();
	void SetModel(Model* model);
	void InitializeSkeleton();

	const std::optional<Skeleton>& GetSkeleton() const noexcept { return skeleton_; }
    bool SetJointTranslationScale(std::size_t index, const Vector3& translation, const Vector3& scale);
	Model* GetModel() const { return model_; }
	const Matrix4x4& GetWorldMatrix() const { return worldMatrix_; }
	// Excludes the model root transform, so skeleton gizmos use the same object space.
	const Matrix4x4& GetObjectWorldMatrix() const { return objectWorldMatrix_; }
	Vector3 GetPosition() const { return transform_.translate; }
	Vector3 GetRotation() const { return transform_.rotate; }
	Vector3 GetScale() const { return transform_.scale; }

	void SetPosition(const Vector3& position) { transform_.translate = position; }
	void SetRotation(const Vector3& rotation) { transform_.rotate = rotation; }
	bool SetScale(const Vector3& scale);
	bool SetShearY(const Vector2& slope);
	void SetTexture(Texture2DHandle textureHandle) { textureHandle_ = textureHandle; }
	void SetTexture(uint32_t textureHandle);
	void SetColor(const Vector4& color);
	void SetEnableLighting(bool enabled) { if (constantBuffer_ && materialData_) materialData_->enableLighting = enabled ? 1 : 0; }
	// 環境マップ用のテクスチャハンドルをセット
	void SetEnvironmentMap(TextureCubeHandle handle) { environmentMapHandle_ = handle; }
	// 反射強度のセット
	void SetEnvironmentCoefficient(float coef);
	void SetCullMode(int cullMode);
	void SetShininess(float shininess);
	void SetSpecularType(SpecularType type) { if (constantBuffer_ && materialData_) materialData_->specularType = static_cast<int32_t>(type); }
	// x: specular gain, y: grain contrast, z: cells/world-unit, w: normal variation.
	bool SetSurfaceDetail(const Vector4& detail);
	// RGB: sky-fill tint, W: opt-in blend. Zero preserves legacy lighting.
	bool SetOutdoorAmbient(const Vector4& skyTint);


	// 平行光源の設定



	// アニメーション用アクセサ
	void SetAnimation(const Animation& animation) { animation_ = animation; }
	float GetAnimationTime() const noexcept { return animationTime_; }
    bool SetAnimationTime(float seconds) noexcept;
	bool GetIsAnimationPlaying() const noexcept { return isAnimationPlaying_; }
	float GetAnimationSpeed() const noexcept { return animationSpeed_; }
	const Animation& GetAnimation() const { return animation_; }
	bool HasAnimation() const { return animation_.duration > 0.0f; }
	bool HasSkeleton() const { return skeleton_.has_value(); }
	bool HasSkinning() const { return model_ && model_->HasSkinCluster(); }
	bool IsAnimationPlaying() const { return isAnimationPlaying_; }
	void SetAnimationPlaying(bool isPlaying) { isAnimationPlaying_ = isPlaying; }
	void ResetAnimationTime() { animationTime_ = 0.0f; }
	float GetAnimationDuration() const { return animation_.duration; }
	float GetAnimationTimeValue() const { return animationTime_; }
	float GetPlaybackSpeed() const { return animationSpeed_; }
	bool SetPlaybackSpeed(float speed) noexcept;
    void SetBoneManualControl(bool enabled) noexcept { isBoneManualControl_ = enabled; }


private:
	Object3dCommon* object3dCommon_ = nullptr;
	Model* model_ = nullptr;
	Texture2DHandle textureHandle_{};
	TextureCubeHandle environmentMapHandle_{};
	int cullMode_ = 2;
	Animation animation_;
	float animationTime_ = 0.0f;
	bool isAnimationPlaying_ = true;
	float animationSpeed_ = 1.0f;
	std::optional<Skeleton> skeleton_;
	Matrix4x4 worldMatrix_ = MatrixMath::MakeIdentity4x4();
	// ：ルートノードのローカル行列を含まない、オブジェクト自身のワールド変換行列
	Matrix4x4 objectWorldMatrix_ = MatrixMath::MakeIdentity4x4();
	bool computeSkinningPrepared_ = false;
	bool isMirrored_ = false;
	Vector2 shearY_{}; // Per-instance only; zero preserves the standard SRT transform.

	using Material = graphics::ObjectMaterial;

	// GPUに送るための頂点スキンデータ
	struct VertexShaderSkinning
	{
		Vector4 weights;     //頂点シェイダー側でfloat4となる重み
		int32_t boneIndices[4]; //頂点シェイダー側でint4となる骨のインデックス
	};

	using TransformationMatrix = graphics::ObjectTransform;

	Transform transform_ = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };

    // Persistently mapped owner; destruction/replacement requires the caller's completed-frame Fence.
	Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
	Material* materialData_ = nullptr;
	TransformationMatrix* transformationMatrixData_ = nullptr;
	TransformationMatrix* shadowTransformationMatrixData_ = nullptr;

	// スポットライト用

    bool isBoneManualControl_ = false;
};
