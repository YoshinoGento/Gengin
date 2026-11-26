#include "farm/system/FarmActionParticles.h"
#include "farm/core/FarmGrid.h"
#include "farm/system/FarmIrrigationSystem.h"
#include <algorithm>
#include <cmath>

namespace farm {
namespace {
constexpr float kTurn = 6.2831853f, kWaterPulseSeconds = .23f, kRainDripMemory = 9.f;
constexpr std::size_t kWaterBudget = FarmActionParticles::kCapacity / 2, kEdgesPerPulse = 8;
constexpr float kGravity = 5.8f;
constexpr int kSownSeeds=4;
constexpr float kFlight=.46f, kStagger=.066f, kDropSpeed=.16f;
constexpr float kCoverStart=kFlight+(kSownSeeds-1)*kStagger+.10f, kSweep=.38f, kSoilRest=.45f;
float EaseOut(float t) { t=std::clamp(t,0.f,1.f); return t*(2.f-t); }
void StepSowing(FieldParticle& p,float before) {
    if (p.age<0.f) return;
    if (p.kind==FieldGrain::Seed) {
        const float t=std::clamp(p.age,0.f,kFlight);
        const float dt=t-std::clamp(before,0.f,kFlight);
        p.position.x+=p.velocity.x*dt; p.position.z+=p.velocity.z*dt;
        p.position.y=p.floor+(kFlight-t)*(kDropSpeed+.5f*kGravity*(kFlight+t));
        p.angle+=p.spin*dt;
        // Sink the kernel only as the low soil wedges meet over its landing point.
        p.position.y-=p.radius*.8f*EaseOut(1.f-(p.life-p.age)/.09f);
    } else {
        const float t=std::clamp(p.age/kSweep,0.f,1.f);
        const float dt=kSweep*(EaseOut(t)-EaseOut(before/kSweep));
        p.position.x+=p.velocity.x*dt; p.position.z+=p.velocity.z*dt;
        p.position.y=p.floor+p.radius*.84f*4.f*t*(1.f-t);
        p.angle+=p.spin*dt;
    }
}
}
void FarmActionParticles::Clear() noexcept {
    sounds_.reset();
    arrivals_.Clear();
    pours_={};
    hoe_={};
    count_ = nextWaterTile_ = 0;
    board_ = nullptr;
    lastMeasurement_ = 0;
    waterCooldown_ = wetLeafSeconds_ = 0;
    sowing_=false;
}
void FarmActionParticles::Bind(const FarmGrid& grid) noexcept {
    if (board_ == &grid && generation_ == grid.GetGeneration()) return;
    Clear();
    board_ = &grid;
    generation_ = grid.GetGeneration();
}
float FarmActionParticles::Random() noexcept {
    seed_ = seed_ * 1664525u + 1013904223u;
    return static_cast<float>(seed_ >> 8) / 16777216.f;
}
FieldParticle* FarmActionParticles::Emit(FieldGrain kind, Vector3 p, Vector3 v, float floor, float radius, float life) {
    if (count_ == kCapacity) return nullptr;
    particles_[count_++] = {kind,p,v,floor,0,life,radius,Random()*kTurn,(Random()-.5f)*8,Random(),false};
    return &particles_[count_-1];
}
void FarmActionParticles::Scatter(FieldGrain kind, Vector3 origin, float floor, unsigned count, float spread, float lift, float radius, float contactRadius) {
    for (unsigned i = 0; i < count; ++i) {
        const float angle = (static_cast<float>(i)+Random())*kTurn/count;
        const float speed = (.35f+.65f*Random())*spread;
        const float x = std::cos(angle), z = std::sin(angle);
        const Vector3 position{origin.x+x*contactRadius,origin.y,origin.z+z*contactRadius};
        const float up = lift*(.52f+.48f*Random());
        const float size = radius*(.67f+.54f*Random());
        Emit(kind,position,{x*speed,up,z*speed},floor,size,.55f+.38f*Random());
    }
}
void FarmActionParticles::Sow(int index,CropType crop,const FarmTileVisualData& bed) {
    // Reserve both sides of the cover with each kernel, even when other actions fill the pool.
    const int n=(std::min)(kSownSeeds,static_cast<int>((kCapacity-count_)/3));
    const float half=bed.halfExtent, soil=bed.center.y+kFarmSoilRidgeHeight;
    for (int i=0;i<n;++i) {
        const float delay=i*kStagger;
        const float along=(i+.5f)/n-.5f;
        const float x=bed.center.x+((i%2?.10f:-.10f)+(Random()-.5f)*.07f)*half;
        const float z=bed.center.z+(along*.98f+(Random()-.5f)*.075f)*half;
        const float r=half*(.087f+.018f*Random());
        const Vector3 drift{(.19f+.18f*Random())*half,0,(Random()-.5f)*half*.35f};
        const float floor=soil+r*.27f;
        const Vector3 start{x-drift.x*kFlight,floor+kDropSpeed*kFlight+.5f*kGravity*kFlight*kFlight,z-drift.z*kFlight};
        auto* seed=Emit(FieldGrain::Seed,start,drift,floor,r,kCoverStart-delay+kSweep);
        seed->age=-delay; seed->seedbed=index; seed->plantedCrop=crop;
        for (int side : {-1,1}) {
            const float offset=side*half*(.47f+.07f*Random()), dz=half*(Random()-.5f)*.13f;
            const float size=r*(1.23f+.19f*Random());
            const Vector3 from{x+offset,soil+.008f,z+dz};
            const Vector3 travel{(side*.30f*r-offset)/kSweep,0,-dz*.80f/kSweep};
            auto* earth=Emit(FieldGrain::CoverSoil,from,travel,from.y,size,kSweep+kSoilRest);
            earth->age=-kCoverStart; earth->seedbed=index; earth->plantedCrop=crop;
            earth->tone=(.32f+.68f*Random())*(1.f-.50f*bed.soilWetness);
        }
        sowing_=true;
    }
}
float FarmActionParticles::SproutScale(const FarmGrid& grid,int tile,CropType crop) const noexcept {
    if (!sowing_ || board_!=&grid || generation_!=grid.GetGeneration() || !IsPlantableCrop(crop)) return 1.f;
    for (const auto& p:Read()) if (p.kind==FieldGrain::CoverSoil && p.seedbed==tile && p.plantedCrop==crop)
        return EaseOut((p.age-kSweep)/kSoilRest);
    return 1.f;
}
std::span<const watering::Pour> FarmActionParticles::Pours(const FarmGrid& grid) const noexcept {
    if (board_!=&grid || generation_!=grid.GetGeneration()) return {};
    return pours_;
}
float FarmActionParticles::PendingWater(const FarmGrid& grid,int index) const noexcept {
    for (const auto& p:Pours(grid)) if (p.tile>=0 && p.tile==index) {
        const auto* tile=grid.GetTile(index);
        return tile?watering::Pending(p,tile->moisture):0.f;
    }
    return 0.f;
}
void FarmActionParticles::StartPour(int index,const FarmTileVisualData& bed,float added) {
    using namespace watering;
    auto* slot=&pours_.front();
    for(auto& p:pours_) {
        if(p.tile==index) { slot=&p; break; }
        if(p.tile<0 || (slot->tile>=0 && p.age>slot->age)) slot=&p;
    }
    float before=std::clamp(bed.soilWetness-added,0.f,1.f);
    if(slot->tile>=0) {
        if(slot->tile==index) before-=Pending(*slot,before);
        for(size_t i=0;i<count_;) {
            if(particles_[i].wateredTile==slot->tile) particles_[i]=particles_[--count_]; else ++i;
        }
        *slot={};
    }
    // Each target needs one wet mark and at least one drop. A full pool never delays gameplay.
    const int available=static_cast<int>(kCapacity-count_);
    if(available<2) return;
    const int patches=(std::min)(kPatches,available/2), drops=(std::min)(kDrops,available-patches);
    *slot={index,bed.center,bed.halfExtent,0,before};
    std::array<Vector3,kPatches> targets{};
    for(int i=0;i<patches;++i) {
        const float x=(-.36f+.32f*i)*bed.halfExtent,z=(-.43f+.21f*(i%2))*bed.halfExtent;
        targets[i]=bed.center+Vector3{x,.006f,z};
        auto* stain=Emit(FieldGrain::Soak,targets[i],{},targets[i].y,bed.halfExtent*(.52f+.06f*Random()),kSoaked-kFirstHit);
        stain->wateredTile=index; stain->age=-(kFirstHit+i*kSpacing);
    }
    for(int i=0;i<drops;++i) {
        const float release=kTip+i*kSpacing;
        const auto nozzle=Pose(*slot,release).Rose();
        auto hit=targets[i%patches];
        hit.x+=(Random()-.5f)*.22f*bed.halfExtent; hit.z+=(Random()-.5f)*.28f*bed.halfExtent;
        const float ridge=std::abs(std::remainder((hit.x-bed.center.x)/bed.halfExtent,.52f));
        hit.y+=ridge<.12f?kFarmSoilRidgeHeight:0.f;
        const auto travel=(hit-nozzle)*(1.f/kDropFlight);
        auto* drop=Emit(FieldGrain::PourDrop,nozzle,travel,hit.y,bed.halfExtent*(.046f+.019f*Random()),kDropFlight+kBounce);
        drop->age=-release; drop->wateredTile=index;
    }
}
void FarmActionParticles::OnTool(const FarmToolActionResult& result, const FarmGrid& grid, const FarmVisualSystem& visual) {
    if (!result.Succeeded()) return;
    const auto* tile = grid.GetTile(result.tileIndex);
    if (!tile || tile->feature != FarmTileFeature::None) return;
    const auto data = visual.GetTileVisualData(grid,result.tileIndex);
    if (!data.valid) return;
    Bind(grid);
    if (const auto hit=hoe_.Finish(grid)) ScatterHoeImpact(*hit);
    auto p = data.center;
    const float floor = p.y + kFarmSoilRidgeHeight + .007f;
    p.y = floor+.045f;
    if (result.status == FarmToolActionStatus::Applied) {
        if (result.tool == FarmTool::Hoe && tile->state == FarmTileState::Tilled) {
            const auto contact=data.center+Vector3{0,kFarmSoilRidgeHeight,-.12f*data.halfExtent};
            hoe_.Start({result.tileIndex,contact,1.85f*data.halfExtent,tile->moisture});
        } else if (result.tool==FarmTool::Seed && tile->state==FarmTileState::Planted && IsPlantableCrop(tile->crop)) {
            Sow(result.tileIndex,tile->crop,data);
        } else if (result.tool == FarmTool::Water && result.moistureAfter > result.moistureBefore) {
            StartPour(result.tileIndex,data,result.moistureAfter-result.moistureBefore);
        }
        return;
    }
    if (!result.harvestedTile || tile->state != FarmTileState::Tilled || !IsHarvestReady(*result.harvestedTile)) return;
    const auto& crop = *result.harvestedTile;
    const float scale = visual.GetCropRenderScale(crop);
    const auto size=harvest::CueFor(result.harvestQuality.harvestSize);
    using harvest::BySize;
    switch (crop.crop) {
    case CropType::Carrot:
    case CropType::TestCrop:
        Mark(sounds_,FieldCue::Pull);
        Scatter(FieldGrain::Clod,p,floor,BySize(size,4,11,13),BySize(size,.48f,1.f,1.25f),
            BySize(size,.94f,2.05f,2.50f),BySize(size,.035f,.055f,.085f));
        p.y += .22f*scale;
        Scatter(FieldGrain::Clod,p,floor,BySize(size,2,5,5),.34f,.6f,.032f);
        if (crop.moisture < .42f) Scatter(FieldGrain::Dust,p,floor,BySize(size,1,4,5),.36f,.48f,BySize(size,.045f,.065f,.090f));
        break;
    case CropType::Tomato:
        Mark(sounds_,FieldCue::Pick);
        p.y += .48f*scale;
        Scatter(FieldGrain::Leaf,p,floor,BySize(size,2,6,7),BySize(size,.60f,1.02f,1.18f),1.22f,BySize(size,.083f,.115f,.125f));
        if (wetLeafSeconds_ > 0) Scatter(FieldGrain::Drop,p,floor,3,.38f,.25f,.048f);
        break;
    case CropType::Pumpkin:
        Mark(sounds_,FieldCue::Cut);
        p.y += .15f*scale;
        Scatter(FieldGrain::Clod,p,floor,BySize(size,3,10,10),BySize(size,.45f,.98f,1.06f),.38f,BySize(size,.042f,.080f,.085f),.41f*scale);
        break;
    default: break;
    }
}
void FarmActionParticles::OnBasketLanding(const FarmGrid& grid,const harvest::BasketLanding& hit) noexcept {
    if(board_!=&grid || generation_!=grid.GetGeneration() || hit.unit<=0.f) return;
    Mark(sounds_,hit.spillsSoil && hit.size==harvest::SizeCue::Large ? FieldCue::HeavyBasket : FieldCue::Basket);
    if(!hit.spillsSoil) return;
    const bool dry=hit.moisture<.42f;
    const int grains=hit.size==harvest::SizeCue::Small?1:(dry?3:2);
    for(int i=0;i<grains;++i) {
        const float spread=static_cast<float>(i)-.5f*(grains-1);
        auto start=hit.rim;
        start.z+=spread*.044f*hit.unit;
        const float r=hit.unit*(.044f+.014f*Random());
        const Vector3 velocity{hit.side*(.52f+.20f*Random())*hit.unit,(.62f+.38f*Random())*hit.unit,
            (-.38f+.25f*spread)*hit.unit};
        auto* grain=Emit(FieldGrain::Clod,start,velocity,hit.floor+r*.5f,r,.54f+.12f*Random());
        if(!grain) break;
        if(!dry) grain->tone*=.4f;
    }
    // A heavy landing presses the ground at the box feet, not in mid-air at the fruit.
    if(hit.size!=harvest::SizeCue::Large) return;
    for(float x:{-1.f,1.f}) for(float z:{-1.f,1.f}) {
        const Vector3 foot{hit.crateCenter.x+x*hit.footprintHalf.x,hit.floor,hit.crateCenter.z+z*hit.footprintHalf.y};
        const float radius=(dry?.17f:.058f)*hit.unit;
        const Vector3 at=foot+Vector3{0,radius*.55f,0};
        const Vector3 impulse{x*(dry?.85f:.58f)*hit.unit,(dry?.34f:.75f)*hit.unit,z*.56f*hit.unit};
        auto* puff=Emit(dry?FieldGrain::LandingPuff:FieldGrain::Clod,at,impulse,at.y,radius,dry?.46f:.52f);
        if(!puff) return;
        if(!dry) puff->tone*=.4f;
    }
}
void FarmActionParticles::ScatterHoeImpact(const hoeing::Stroke& stroke) {
    Mark(sounds_,FieldCue::Hoe);
    const bool dry=stroke.moisture<.42f;
    const auto first=count_;
    const float unit=stroke.scale;
    Scatter(FieldGrain::Clod,stroke.contact,stroke.contact.y,dry?6:4,(dry?.72f:.48f)*unit,
        (dry?1.85f:1.42f)*unit,(dry?.054f:.066f)*unit,.038f*unit);
    for(size_t i=first;i<count_;++i) {
        particles_[i].velocity.x-=.26f*unit;
        if(!dry) particles_[i].tone*=.35f;
    }
    if(dry) Scatter(FieldGrain::Dust,stroke.contact,stroke.contact.y,3,.43f*unit,.72f*unit,.11f*unit,.04f*unit);
}
void FarmActionParticles::Step(float dt,const FarmGrid& grid,const FarmIrrigationSystem& water) noexcept {
    sowing_=false;
    for(auto& cue:pours_) if(cue.tile>=0) {
        if(cue.age<watering::kTip && cue.age+dt>=watering::kTip) Mark(sounds_,FieldCue::Pour);
        cue.age+=dt;
        if(cue.age>=watering::kSoaked+watering::kSpacing*watering::kPatches) cue={};
    }
    for (std::size_t i = 0; i < count_;) {
        auto& p = particles_[i];
        const float before=p.age;
        p.age += dt;
        if (p.age >= p.life) { p = particles_[--count_]; continue; }
        if (p.flowTile>=0) {
            if(!arrivals_.StillConnected(p.flowTile,grid,water)) { p=particles_[--count_]; continue; }
            if(p.kind==FieldGrain::Seep) { ++i; continue; }
        }
        if (p.wateredTile>=0) {
            using namespace watering;
            if (p.kind==FieldGrain::PourDrop && p.age>=0) {
                const float u=std::clamp(p.age/kDropFlight,0.f,1.f);
                const float air=kDropFlight*(u-std::clamp(before/kDropFlight,0.f,1.f));
                const float bounce=std::clamp((p.age-kDropFlight)/kBounce,0.f,1.f);
                const float step=bounce-std::clamp((before-kDropFlight)/kBounce,0.f,1.f);
                p.position.x+=p.velocity.x*air+std::cos(p.angle)*step*p.radius*3.6f;
                p.position.z+=p.velocity.z*air+std::sin(p.angle)*step*p.radius*3.6f;
                p.position.y=p.floor-p.velocity.y*kDropFlight*(1.f-u)*(1.f+.68f*u)
                    +p.radius*3.5f*4.f*bounce*(1.f-bounce);
                p.bounced=p.age>=kDropFlight;
            }
            ++i; continue;
        }
        if (p.seedbed>=0) {
            const auto* bed=grid.GetTile(p.seedbed);
            if (!bed || bed->state!=FarmTileState::Planted || bed->crop!=p.plantedCrop) {
                p=particles_[--count_]; continue;
            }
            if(p.kind==FieldGrain::Seed && before<kFlight && p.age>=kFlight) Mark(sounds_,FieldCue::Seed);
            if(p.kind==FieldGrain::CoverSoil && before<0 && p.age>=0) Mark(sounds_,FieldCue::Cover);
            StepSowing(p,before); sowing_=true; ++i; continue;
        }
        p.angle += p.spin*dt;
        switch (p.kind) {
        case FieldGrain::LandingPuff:
            p.velocity=p.velocity*(1.f-1.8f*dt); break;
        case FieldGrain::Dust: p.velocity.x += (.32f-p.velocity.x)*dt; p.velocity.y *= 1.f-dt; break;
        case FieldGrain::Leaf:
            p.velocity.x += .9f*std::cos(p.angle)*dt;
            p.velocity.y -= 1.9f*dt; break;
        case FieldGrain::Drop: case FieldGrain::Clod: p.velocity.y -= kGravity*dt; break;
        default: break;
        }
        p.position.x += p.velocity.x*dt; p.position.y += p.velocity.y*dt; p.position.z += p.velocity.z*dt;
        const bool stream=p.kind==FieldGrain::Foam;
        if (stream) {
            p.position += p.foamAcceleration*(.5f*dt*dt);
            p.velocity += p.foamAcceleration*dt;
        }
        if (!stream && p.position.y <= p.floor) {
            p.position.y = p.floor;
            if (p.kind == FieldGrain::Drop) {
                p.kind=FieldGrain::Ripple; p.velocity={}; p.age=0; p.life=.32f; p.radius*=2.5f;
            } else if (p.kind == FieldGrain::Clod && !p.bounced) {
                p.bounced=true; p.velocity={p.velocity.x*.4f,-p.velocity.y*.25f,p.velocity.z*.4f}; p.spin*=.45f;
            } else { p.velocity={}; p.spin=0; }
        }
        ++i;
    }
    if(const auto hit=hoe_.Advance(grid,dt)) ScatterHoeImpact(*hit);
}
void FarmActionParticles::EmitArrival(arrival::Beat beat,int tile,int parent,const FarmGrid& grid,const FarmIrrigationSystem& water,const FarmVisualSystem& visual) {
    const auto dst=visual.GetTileVisualData(grid,tile), src=visual.GetTileVisualData(grid,parent);
    if(!dst.valid || !src.valid) return;
    if(beat==arrival::Beat::Soil) {
        if(count_>=kWaterBudget) return;
        // Start on the donor side of the flat soil plateau, below the raised ridges.
        const auto delta=dst.center-src.center;
        const float angle=std::atan2(delta.z,delta.x);
        constexpr float kBankOffset=.84f, kSurfaceLift=.009f;
        const auto entry=dst.center-Vector3{std::cos(angle),0,std::sin(angle)}*(kBankOffset*dst.halfExtent);
        auto p=entry; p.y+=kSurfaceLift;
        if(auto* stain=Emit(FieldGrain::Seep,p,{},p.y,dst.halfExtent,arrival::kWick)) {
            stain->angle=angle; stain->flowTile=tile;
            Mark(sounds_,FieldCue::Seep);
        }
        return;
    }
    if(count_+2>kWaterBudget) return;
    const int out=arrivals_.Surface(tile).outlet;
    if(out>=0 && water.GetUpstreamTileIndex(out)==tile && out%grid.GetWidth()!=parent%grid.GetWidth() &&
        out/grid.GetWidth()!=parent/grid.GetWidth()) {
        auto bank=dst.waterSurfaceCenter+(dst.center-src.center)*.21f;
        Mark(sounds_,FieldCue::Bend);
        bank.y=dst.waterSurfaceCenter.y+.018f;
        Scatter(FieldGrain::Drop,bank,bank.y-.012f,2,.27f,.70f,.032f);
    }
}
void FarmActionParticles::SampleWater(const FarmGrid& grid, const FarmIrrigationSystem& water, const FarmVisualSystem& visual,
    std::span<const FarmIrrigationTileFlow> flows) {
    if (flows.empty()) return;
    std::size_t emitted = 0, checked = 0;
    for (; checked < flows.size() && emitted < kEdgesPerPulse && count_+4 <= kWaterBudget; ++checked) {
        const auto index = (nextWaterTile_+checked)%flows.size();
        const auto& flow = flows[index];
        if (flow.incoming <= 0 && flow.soilReceived <= 0) continue;
        const auto i = static_cast<int>(index);
        if(arrivals_.Playing(i)) continue;
        const auto dst = visual.GetTileVisualData(grid,i);
        if (!dst.valid) continue;
        if (flow.soilReceived > 0) {
            auto p=dst.center; p.y += kFarmSoilRidgeHeight+.009f;
            Emit(FieldGrain::WetSoil,p,{},p.y,.21f,.58f);
        } else {
            const int parent = water.GetUpstreamTileIndex(i);
            const auto src = visual.GetTileVisualData(grid,parent);
            if (!src.valid) continue;
            auto from=src.waterSurfaceCenter, to=dst.waterSurfaceCenter;
            from.y+=.032f; to.y+=.032f;
            const int previous=water.GetUpstreamTileIndex(parent);
            if(arrivals_.Playing(parent) || arrivals_.Playing(previous)) continue;
            const bool turn = previous>=0 && previous%grid.GetWidth()!=i%grid.GetWidth() &&
                previous/grid.GetWidth()!=i/grid.GetWidth();
            // Adjacent spans meet at the edge midpoint. The upstream tile center controls the bend.
            const float travel=.40f+.12f*Random();
            auto start=from, end=to;
            if (previous>=0) {
                auto upstream=visual.GetTileVisualData(grid,previous).waterSurfaceCenter;
                upstream.y+=.032f;
                start=(upstream+from)*.5f;
            }
            if (water.GetDownstreamCanalCount(i)>0) end=(from+to)*.5f;
            auto control=previous>=0 ? from : (start+end)*.5f;
            const float jitter=(Random()-.5f)*dst.waterHalfExtent*.42f;
            if (from.x==to.x) control.x+=jitter; else control.z+=jitter;
            if (auto* foam=Emit(FieldGrain::Foam,start,(control-start)*(2.f/travel),end.y,.088f+.035f*Random(),travel))
                foam->foamAcceleration=(end-control*2.f+start)*(2.f/(travel*travel));
            if (turn || src.center.y > dst.center.y+.05f) {
                auto impact = turn ? from : to;
                if (turn) {
                    const auto upstream=visual.GetTileCenter(grid,previous);
                    impact.x += (from.x-upstream.x)*.19f; impact.z += (from.z-upstream.z)*.19f;
                }
                Scatter(FieldGrain::Drop,impact,impact.y-.014f,3,.45f,.95f,.035f);
            }
        }
        ++emitted;
    }
    nextWaterTile_ = (nextWaterTile_+checked)%flows.size();
}
void FarmActionParticles::Update(const FarmGrid& grid, const FarmIrrigationSystem& water,
    const FarmVisualSystem& visual, float dt, float rain, bool visible) {
    Bind(grid);
    const auto revision=water.MeasurementRevision();
    if (!visible) { sounds_.reset(); count_=0; pours_={}; hoe_={}; arrivals_.Clear(); sowing_=false; lastMeasurement_=revision; waterCooldown_=0; return; }
    if (!std::isfinite(dt) || dt <= 0) return;
    dt=(std::min)(dt,.1f);
    wetLeafSeconds_=rain>.1f ? kRainDripMemory : (std::max)(0.f,wetLeafSeconds_-dt);
    Step(dt,grid,water);
    arrivals_.Advance(dt);
    waterCooldown_=(std::max)(0.f,waterCooldown_-dt);
    std::span<const FarmIrrigationTileFlow> flows;
    if (lastMeasurement_ != revision) {
        flows=water.GetLastTileFlows(grid);
        arrivals_.Observe(grid,water,flows,lastMeasurement_);
        lastMeasurement_=revision;
    }
    arrivals_.Dispatch(grid,water,[&](arrival::Beat beat,int tile,int parent){EmitArrival(beat,tile,parent,grid,water,visual);});
    if (!flows.empty() && waterCooldown_<=0) { SampleWater(grid,water,visual,flows); waterCooldown_=kWaterPulseSeconds; }
}
}
