#pragma once
#include "farm/render/FarmRainGeometry.h"
#include <array>
#include <optional>

namespace farm {
class FarmGrid;
class FarmVisualSystem;
class FarmIrrigationSystem;

struct WaterPointer {
    int tile;
    Vector2 screen;
    Vector3 origin, direction;
};

// Cosmetic state only. No references to tile or irrigation measurement storage survive Update.
class FarmWaterPlaySystem final {
public:
    static constexpr int kRingCount = 6, kSides = 12;
    static constexpr std::size_t kMaxVertices = kRingCount*kSides*6 + 18;
    void Update(const FarmGrid& grid, const FarmVisualSystem& visual, const FarmIrrigationSystem& irrigation,
        const std::optional<WaterPointer>& pointer, float dt, bool active);
    std::span<const rain::Vertex> Vertices() const noexcept { return {vertices_.data(),count_}; }
    std::optional<Vector3> LeafPosition() const noexcept { return leaf_.tile<0 ? std::nullopt : std::optional{leaf_.position}; }
private:
    struct Ring { Vector3 center{}; float angle{}, life{}; bool wake{}; };
    struct Leaf {
        int tile=-1, previous=-1, next=-1;
        float t{}, age{}, heading{};
        Vector3 position{};
    };
    std::array<Ring,kRingCount> rings_{};
    std::array<rain::Vertex,kMaxVertices> vertices_{};
    Leaf leaf_{};
    std::optional<Vector3> anchor_;
    Vector2 screen_{};
    float cooldown_{}, travel_{}, leafWait_{}, stale_{};
    int slot_{};
    std::size_t count_{};
    const FarmGrid* binding_{};
    uint64_t generation_{}, measurement_{};
};
}
