#include "3d/ModelManager.h"
#include "3d/Model.h"       // ★Modelを消去するために「正体」が必要
#include "base/DirectXCommon.h"
#include "base/SrvManager.h"
#include <cassert>
#include "base/LoadTiming.h"

/**
 * コンストラクタとデストラクタ
 */
ModelManager::ModelManager() = default;
ModelManager::~ModelManager() = default;

/**
 * 初期化
 */
void ModelManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
    assert(dxCommon);
    assert(srvManager);
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
}

/**
 * モデルの読み込み
 */
void ModelManager::LoadModel(const std::string& filename) {
    loading::Scope timing(loading::Stage::ModelLookup);
    if (models_.find(filename) != models_.end()) {
        return;
    }

    std::unique_ptr<Model> model = std::make_unique<Model>();
    model->Initialize(this, "Resources", filename);
    models_[filename] = std::move(model);
}

/**
 * モデルの取得
 */
Model* ModelManager::GetModel(const std::string& filename) {
    if (models_.find(filename) != models_.end()) {
        return models_[filename].get();
    }
    return nullptr;
}

/**
 * ゲッターの実装
 * ★ヘッダー側で宣言のみにしたので、こちらに本体を書きます。
 */
DirectXCommon* ModelManager::GetDxCommon() const {
    return dxCommon_;
}

SrvManager* ModelManager::GetSrvManager() const {
    return srvManager_;
}
