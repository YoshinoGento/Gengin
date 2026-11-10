#pragma once
#include "farm/render/FarmMeshLayout.h"

namespace farm {
struct FarmPestMeshParts {
    static constexpr std::size_t kActiveLimit = 3;
    static constexpr std::size_t kPerActive = 10;
    static constexpr std::size_t kProtectionParts = 5 * 8;
    static constexpr std::size_t kCureParts = kActiveLimit * 6;
    static constexpr std::size_t kCapacity = kActiveLimit * kPerActive + kProtectionParts + kCureParts;
    std::array<FarmMeshPart, kCapacity> parts{};
    std::size_t count = 0;

    void Add(Vector3 position, Vector3 scale, Vector4 color,
        FarmMeshSurface surface = FarmMeshSurface::PestCue) noexcept {
        if (count >= parts.size() || !std::isfinite(position.x) || !std::isfinite(position.y) ||
            !std::isfinite(position.z) || !std::isfinite(scale.x) || !std::isfinite(scale.y) ||
            !std::isfinite(scale.z) || scale.x < .0001f || scale.y < .0001f || scale.z < .0001f) return;
        parts[count++] = {position, scale, color, false, {}, FarmMeshShape::Box, surface};
    }
};

inline Vector3 FarmPestCropTop(const FarmGrid& grid, int index, const FarmVisualSystem& visual) noexcept {
    auto top = visual.GetTileVisualData(grid, index).center;
    const auto crop = BuildFarmCropMeshParts(grid, index, visual);
    // Crop OBJ coordinates are base-anchored, y in [0,1].
    for (std::size_t i = 0; i < crop.count; ++i)
        top.y = (std::max)(top.y, crop.parts[i].position.y + crop.parts[i].scale.y);
    return top;
}

inline void AppendFarmPestMeshes(FarmPestMeshParts& output, const FarmGrid& grid, int index,
    const FarmVisualSystem& visual, float phase) noexcept {
    const auto* tile = grid.GetTile(index);
    const auto data = visual.GetTileVisualData(grid, index);
    if (!tile || !data.valid || tile->pests.warningDay <= 0 || !IsPlantableCrop(tile->crop) ||
        !std::isfinite(phase)) return;
    const auto top = FarmPestCropTop(grid, index, visual);
    const float unit = data.halfExtent;
    const float bob = unit * .035f * std::sin(phase * 2.0f);
    const bool damaged = tile->pests.damageDays > 0;
    const Vector4 color = damaged ? Vector4{1,.42f,.14f,1} : Vector4{1,.80f,.22f,1};
    output.Add({top.x,top.y + unit * .43f + bob,top.z}, {unit*.045f,unit*.13f,unit*.045f}, color);
    output.Add({top.x,top.y + unit * .20f + bob,top.z}, {unit*.045f,unit*.045f,unit*.045f}, color);
    if (!damaged) return;
    constexpr float kPi = 3.14159265f;
    for (int insect = 0; insect < 2; ++insect) {
        const float angle = phase + static_cast<float>(insect)*kPi + static_cast<float>(index)*.47f;
        const float x = top.x + std::cos(angle)*unit*.55f;
        const float z = top.z + std::sin(angle)*unit*.55f;
        const float y = top.y + unit*(.12f + .07f*std::sin(angle*2));
        const float flap = unit*(.045f + .035f*std::sin(phase*12 + static_cast<float>(insect)));
        output.Add({x,y,z}, {unit*.06f,unit*.045f,unit*.06f}, {.22f,.26f,.08f,1}, FarmMeshSurface::Crop);
        output.Add({x-std::sin(angle)*unit*.08f,y,z+std::cos(angle)*unit*.08f},
            {unit*.035f,unit*.035f,unit*.035f}, {.10f,.12f,.05f,1}, FarmMeshSurface::Crop);
        for (int side : {-1,1})
            output.Add({x+static_cast<float>(side)*unit*.09f,y+flap,z},
                {unit*.07f,unit*.012f,unit*.045f}, {.75f,.82f,.64f,1}, FarmMeshSurface::Crop);
    }
}

inline void AppendFarmProtectionMeshes(FarmPestMeshParts& output, const FarmGrid& grid, int index,
    const FarmVisualSystem& visual) noexcept {
    const auto* tile = grid.GetTile(index);
    const auto data = visual.GetTileVisualData(grid, index);
    if (!tile || !data.valid || tile->feature != FarmTileFeature::None) return;
    // Inset within the flat plateau, separately from yellow selection corners.
    const float edge = data.halfExtent*.58f, arm = data.halfExtent*.17f, width = data.halfExtent*.018f;
    const float y = data.center.y + .055f;
    constexpr Vector4 color{.30f,.78f,.64f,1};
    for (int x : {-1,1}) for (int z : {-1,1}) {
        output.Add({data.center.x+x*(edge-arm*.5f),y,data.center.z+z*edge}, {arm*.5f,.006f,width}, color);
        output.Add({data.center.x+x*edge,y,data.center.z+z*(edge-arm*.5f)}, {width,.006f,arm*.5f}, color);
    }
}

inline void AppendFarmCureMeshes(FarmPestMeshParts& output, Vector3 top, float unit, float progress) noexcept {
    if (!std::isfinite(progress) || progress < 0 || progress >= 1) return;
    constexpr float kTau = 6.28318531f;
    const float size = unit*.055f*(1-progress);
    for (int i = 0; i < 6; ++i) {
        const float angle = static_cast<float>(i)*kTau/6;
        const float radius = unit*(.18f + .65f*progress);
        output.Add({top.x+std::cos(angle)*radius,top.y+unit*(.16f+.45f*progress),top.z+std::sin(angle)*radius},
            {size,size,size}, {.72f,.95f,.78f,1});
    }
}
} // namespace farm
