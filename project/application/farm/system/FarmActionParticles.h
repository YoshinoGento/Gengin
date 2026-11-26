#pragma once
#include "farm/system/FarmVisualSystem.h"
#include "farm/system/FarmFieldCue.h"
#include "farm/system/FarmWateringPose.h"
#include "farm/system/FarmHoeStroke.h"
#include "farm/system/FarmCanalArrival.h"
#include "farm/system/FarmBasketLanding.h"
#include <array>
#include <span>

namespace farm {
enum class FieldGrain { Clod, Dust, Leaf, Drop, Foam, Ripple, WetSoil, Seed, CoverSoil, PourDrop, Soak, Seep, LandingPuff };
struct FieldParticle {
    FieldGrain kind{};
    Vector3 position{}, velocity{};
    float floor{}, age{}, life{}, radius{}, angle{}, spin{}, tone{};
    bool bounced{};
    Vector3 foamAcceleration{}; // Constant second derivative of the water streamline.
    float flutter{}; // Fold rotation amplitude; zero preserves the unrolled grain.
    int seedbed=-1;
    CropType plantedCrop=CropType::None;
    int wateredTile=-1;
    int flowTile=-1;
};

// World-space feedback only. Board identity is compared, never dereferenced or retained by a particle.
class FarmActionParticles final {
public:
    static constexpr std::size_t kCapacity = 144;
    void Clear() noexcept;
    FieldCues TakeSounds() noexcept { const auto result=sounds_; sounds_.reset(); return result; }
    void OnTool(const FarmToolActionResult& committed, const FarmGrid& grid, const FarmVisualSystem& visual);
    void OnBasketLanding(const FarmGrid& grid,const harvest::BasketLanding& contact) noexcept;
    void Update(const FarmGrid& grid, const FarmIrrigationSystem& water, const FarmVisualSystem& visual,
        float dt, float rain, bool visible);
    std::span<const FieldParticle> Read() const noexcept { return {particles_.data(),count_}; }
    float SproutScale(const FarmGrid& grid,int tile,CropType crop) const noexcept;
    std::span<const watering::Pour> Pours(const FarmGrid& grid) const noexcept;
    float PendingWater(const FarmGrid& grid,int index) const noexcept;
    arrival::Reveal WaterFront(const FarmGrid& grid,int index) const noexcept {
        return board_==&grid && generation_==grid.GetGeneration()?arrivals_.Surface(index):arrival::Reveal{};
    }
    const hoeing::Strokes* HoeStrokes(const FarmGrid& grid) const noexcept {
        return board_==&grid && generation_==grid.GetGeneration()?&hoe_:nullptr;
    }
private:
    void ScatterHoeImpact(const hoeing::Stroke& stroke);
    void StartPour(int index,const FarmTileVisualData& bed,float added);
    void Bind(const FarmGrid& grid) noexcept;
    void Step(float dt,const FarmGrid& grid,const FarmIrrigationSystem& water) noexcept;
    void Sow(int index,CropType crop,const FarmTileVisualData& bed);
    void SampleWater(const FarmGrid& grid, const FarmIrrigationSystem& water, const FarmVisualSystem& visual,
        std::span<const FarmIrrigationTileFlow> flows);
    void EmitArrival(arrival::Beat beat,int tile,int parent,const FarmGrid& grid,const FarmIrrigationSystem& water,const FarmVisualSystem& visual);
    arrival::Timeline arrivals_;
    FieldCues sounds_;
    float Random() noexcept;
    FieldParticle* Emit(FieldGrain kind, Vector3 p, Vector3 v, float floor, float radius, float life);
    void Scatter(FieldGrain kind, Vector3 origin, float floor, unsigned count, float spread, float lift,
        float radius, float contactRadius=.075f);
    std::array<FieldParticle,kCapacity> particles_{};
    std::array<watering::Pour,4> pours_{};
    hoeing::Strokes hoe_;
    std::size_t count_ = 0, nextWaterTile_ = 0;
    const FarmGrid* board_ = nullptr;
    std::uint64_t generation_ = 0, lastMeasurement_ = 0;
    std::uint32_t seed_ = 1;
    float waterCooldown_ = 0, wetLeafSeconds_ = 0;
    bool sowing_=false;
};
}
