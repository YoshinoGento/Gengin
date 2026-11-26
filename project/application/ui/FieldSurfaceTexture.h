#pragma once
#include "ui/FieldSurface.h"
#include "2d/TextureManager.h"

namespace fieldsign {
// All views borrow the same scene-owned sheet. Do not silently sample the fallback with atlas UVs.
[[nodiscard]] inline Texture2DHandle LoadForScene() {
    auto* manager=TextureManager::GetInstance();
    if (!manager || !manager->IsInitialized()) return {};
    const auto sheet=manager->LoadTexture2D(kTexture,TextureManager::Lifetime::Scene,TextureManager::MipPolicy::PreserveFile);
    return sheet==manager->GetFallback2D() ? Texture2DHandle{} : sheet;
}
}
