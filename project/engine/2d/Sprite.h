#pragma once
#include "2d/SpriteCommon.h"
#include "2d/TextureManager.h"
#include "2d/SpriteBufferLayout.h"
#include "math/Matrix.h"
#include <string>

class Sprite {
public:
    Sprite() = default;
    Sprite(const Sprite&) = delete;
    Sprite& operator=(const Sprite&) = delete;
    Sprite(Sprite&&) noexcept = default;
    Sprite& operator=(Sprite&&) noexcept = default;
    // The path-based entry point guarantees that every Sprite resolves assets through TextureManager.
    bool Initialize(SpriteCommon* spriteCommon, const std::string& texturePath);
    bool Initialize(SpriteCommon* spriteCommon, uint32_t textureHandle);

    // 更新
    void Update();

    // 描画
    void Draw();

    // --- ゲッター・セッター ---

    // 座標
    const Vector2& GetPosition() const { return position_; }
    void SetPosition(const Vector2& position) { position_ = position; }

    // 回転 (これが無いとエラーになります！)
    float GetRotation() const { return rotation_; }
    void SetRotation(float rotation) { rotation_ = rotation; }

    // サイズ（通常はテクスチャサイズが自動設定されますが、手動変更も可能）
    const Vector2& GetSize() const { return size_; }
    void SetSize(const Vector2& size) { size_ = size; }

    // 色
    const Vector4& GetColor() const { return color_; }
    void SetColor(const Vector4& color) { color_ = color; if (bufferResource_ && materialData_) materialData_->color = color; }

    // テクスチャ変更
    void SetTexture(Texture2DHandle textureHandle);
    void SetTexture(uint32_t textureHandle);

    // テクスチャ切り出し範囲（左上座標、サイズ）
    void SetTextureRect(const Vector2& leftTop, const Vector2& size);

    // アンカーポイント（0.5, 0.5で中心、0,0で左上）
    void SetAnchorPoint(const Vector2& anchorPoint) { anchorPoint_ = anchorPoint; }

    // 左右・上下反転
    void SetFlip(bool isFlipX, bool isFlipY) { isFlipX_ = isFlipX; isFlipY_ = isFlipY; }

private:
    // 頂点データの更新（サイズや切り取り範囲が変わったら呼ぶ）
    bool InitializeWithHandle(SpriteCommon* spriteCommon, Texture2DHandle textureHandle);
    void AdjustTextureRect();
    void UpdateTextureCoordinates();

private:
    SpriteCommon* spriteCommon_ = nullptr;

    using VertexData = graphics::SpriteVertex;
    using Material = graphics::SpriteMaterial;
    using TransformationMatrix = graphics::SpriteTransform;

    // バッファリソース
    // Mapped views are usable only while bufferResource_ is owned; a moved-from Sprite is inactive.
    // Callers must wait for the existing frame fence before destruction or replacement.
    Microsoft::WRL::ComPtr<ID3D12Resource> bufferResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    VertexData* vertexData_ = nullptr;

    D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
    uint32_t* indexData_ = nullptr;

    Material* materialData_ = nullptr;

    TransformationMatrix* transformationMatrixData_ = nullptr;
    Vector4 color_{1, 1, 1, 1};

    Transform uvTransform_{ {1,1,1}, {0,0,0}, {0,0,0} };

    // スプライトパラメータ
    Vector2 position_ = { 0.0f, 0.0f };
    float rotation_ = 0.0f;
    Vector2 size_ = { 100.0f, 100.0f };
    Vector2 anchorPoint_ = { 0.0f, 0.0f }; // デフォルトは左上
    bool isFlipX_ = false;
    bool isFlipY_ = false;

    // テクスチャ関連
    Texture2DHandle textureHandle_{};
    Vector2 textureLeftTop_ = { 0.0f, 0.0f }; // テクスチャの左上オフセット
    Vector2 textureSize_ = { 100.0f, 100.0f }; // テクスチャの切り出しサイズ
};
