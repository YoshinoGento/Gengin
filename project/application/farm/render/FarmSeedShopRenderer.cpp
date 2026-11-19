#include "farm/render/FarmSeedShopRenderer.h"
#include "farm/ui/FarmSeedShopView.h"
#include "3d/ModelManager.h"
#include <filesystem>

bool FarmSeedShopRenderer::Initialize(Object3dCommon* common, ModelManager* models, Texture2DHandle white) {
    if (ready_) return true;
    if (!common || !models || !white.IsValid()) return false;
    constexpr std::array<const char*, 1> paths{"farm/unit_box.obj"};
    std::array<Model*, 1> meshes{};
    for (std::size_t i = 0; i < paths.size(); ++i) {
        std::error_code error;
        if (!std::filesystem::is_regular_file(std::filesystem::path("Resources") / paths[i], error)) return false;
        models->LoadModel(paths[i]);
        meshes[i] = models->GetModel(paths[i]);
        if (!meshes[i] || meshes[i]->HasSkinCluster()) return false;
    }
    camera_.SetTranslate({0, 0, -farmui::SeedShopLayout::cameraDistance});
    camera_.SetRotate({0, 0, 0});
    camera_.SetFovY(farmui::SeedShopLayout::fovY);
    camera_.SetAspectRatio(1280.0f / 720.0f);
    camera_.Update();
    for (auto& object : objects_) {
        object.Initialize(common); object.SetModel(meshes[0]); object.SetTexture(white);
        object.SetEnableLighting(false);
    }
    ready_ = true;
    return true;
}

void FarmSeedShopRenderer::Draw(int selected, bool showFungicide) {
    if (!ready_) return;
    std::size_t count = 0;
    const auto part = [&](Vector3 position, Vector3 scale, Vector4 color, float rotation = 0.0f) {
        if (count >= objects_.size()) return;
        auto& object = objects_[count++];
        object.SetPosition(position); object.SetRotation({0, rotation, 0});
        if (!object.SetScale(scale)) return;
        object.SetColor(color); object.Update(&camera_, 0.0f); object.Draw();
    };
    // Counter and rear wall are prototype scenery. All parts use existing immutable meshes.
    part({0, 0, 5}, {14, 9, 0.15f}, {0.20f, 0.31f, 0.34f, 1});
    part({-2, -2.75f, 0}, {7, 1, 2.5f}, {0.23f, 0.15f, 0.10f, 1});
    part({-2, -1.68f, 0}, {7.1f, 0.13f, 2.6f}, {0.57f, 0.40f, 0.25f, 1});
    part({-2, -2.10f, -2.53f}, {6.9f, 0.05f, 0.05f}, {0.73f, 0.55f, 0.32f, 1});
    part({-2, 2.45f, 3.7f}, {7.1f, 0.09f, 0.4f}, {0.35f, 0.24f, 0.16f, 1});
    part({-8.7f, 0.4f, 3.7f}, {0.12f, 3.2f, 0.25f}, {0.35f, 0.24f, 0.16f, 1});
    part({4.7f, 0.4f, 3.7f}, {0.12f, 3.2f, 0.25f}, {0.35f, 0.24f, 0.16f, 1});
    for (int i = 0; i < 2; ++i) {
        const auto& rect = farmui::SeedShopLayout::products[i];
        const auto center = farmui::SeedShopLayout::AtScreen(rect.x + rect.width * 0.5f, farmui::SeedShopLayout::packetCenterY);
        const float x = center.x;
        part({x, -1.40f, 0}, {1.50f, 0.15f, 0.80f}, selected == i ? Vector4{1, 0.81f, 0.24f, 1} : Vector4{0.28f, 0.34f, 0.33f, 1});
        if (showFungicide && i == FarmSeedShopSystem::kFungicideProduct % 2) {
            // Bottle rests on the display block; its ivory label faces the shop camera.
            part({x,-.43f,0},{.65f,.82f,.32f},{.23f,.53f,.45f,1});
            part({x,.49f,0},{.44f,.10f,.26f},{.23f,.53f,.45f,1});
            part({x,.76f,0},{.26f,.17f,.21f},{.88f,.81f,.54f,1});
            part({x,-.46f,-.33f},{.54f,.42f,.015f},{.94f,.91f,.75f,1});
            part({x,-.46f,-.36f},{.22f,.09f,.012f},{.23f,.53f,.45f,1});
        }
        // Front faces use the same projection contract as labels and click targets.
        const auto plaque = [&](farmui::Rect screen, float frontZ, Vector4 color) {
            constexpr float depth = 0.025f;
            const auto topLeft = farmui::SeedShopLayout::AtScreen(screen.x, screen.y, frontZ);
            const auto bottomRight = farmui::SeedShopLayout::AtScreen(screen.x+screen.width, screen.y+screen.height, frontZ);
            part({(topLeft.x+bottomRight.x)*0.5f, (topLeft.y+bottomRight.y)*0.5f, frontZ+depth},
                {(bottomRight.x-topLeft.x)*0.5f, (topLeft.y-bottomRight.y)*0.5f, depth}, color);
        };
        const auto sign = farmui::SeedShopLayout::SignBoard(i);
        constexpr float signFrontZ = -2.70f; // In front of the counter lip (-2.6), not embedded in the table.
        plaque(sign, signFrontZ, selected == i ? Vector4{0.92f,0.67f,0.16f,1} : Vector4{0.25f,0.18f,0.13f,1});
        plaque({sign.x+4,sign.y+4,sign.width-8,sign.height-8}, signFrontZ-0.06f, {0.84f,0.87f,0.76f,1});
        for (float pinX : {sign.x+6, sign.x+sign.width-9})
            plaque({pinX,sign.y+8,3,3}, signFrontZ-0.12f, {0.23f,0.28f,0.24f,1});
    }
}
