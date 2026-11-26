#pragma once
#include "farm/system/FarmActionParticles.h"
#include <optional>

namespace farm {
struct FarmBrushContact {
    int tile=-1;
    Vector3 position{};
    bool onCrop=false;
};

// Owns only transient responses to a stroke. No tile or model pointers are kept.
class FarmBrushPlaySystem final {
public:
    static constexpr size_t kCapacity=24;
    void Update(const FarmGrid& grid,float seconds,bool active);
    std::optional<Vector2> MovePointer(std::optional<Vector2> screen);
    bool Stroke(const FarmGrid& grid,FarmBrushContact contact,Vector2 groundDirection);
    Vector2 Lean(int tile,CropType crop) const;
    std::span<const FieldParticle> Read() const { return {particles_.data(),count_}; }
private:
    struct Touch { int tile=-1; CropType crop{}; Vector2 direction{}; float age{}; };
    float Random();
    std::array<FieldParticle,kCapacity> particles_{};
    std::array<Touch,6> touches_{};
    size_t count_=0;
    std::optional<Vector2> screen_;
    const FarmGrid* board_{};
    uint64_t generation_{};
    uint32_t rng_=701;
    float soilDelay_{},leafDelay_{};
    bool active_{};
};
}
