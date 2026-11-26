#pragma once
#include "farm/render/FarmMeshLayout.h"
#include "farm/system/FarmBasketLanding.h"

namespace farm::harvest {
inline constexpr float kRelease=.30f,kFlight=.64f,kStagger=.18f,kSettle=.15f,kPack=1.45f,kDuration=kPack+.25f;
inline constexpr float kContact=kRelease+kFlight;
// Local extents of harvest_basket.obj and its two end grips, before crop-specific scaling.
inline constexpr float kCrateHalfWidth=.53f,kCrateBase=.015f,kCrateFloor=.075f,kGripHeight=.345f;
inline constexpr size_t kMaxParts=5; // Two tomatoes, their stationary vine, and two basket materials.
struct Parts { std::array<FarmMeshPart,kMaxParts> parts{}; size_t count=0; };
struct Pickup {
    FarmCropMeshParts crop{};
    Vector3 bed{},basket{};
    float unit{};
    float moisture{};
    SizeCue size=SizeCue::Regular;
};
inline float Smooth(float t) noexcept { t=std::clamp(t,0.f,1.f); return t*t*(3.f-2.f*t); }
inline Vector3 Mix(Vector3 a,Vector3 b,float u) noexcept { return a+(b-a)*u; }
inline Vector3 Roll(Vector3 v,float angle) noexcept {
    const float c=std::cos(angle),s=std::sin(angle);
    return {v.x*c-v.y*s,v.x*s+v.y*c,v.z};
}
inline float LandingSide(const Pickup& p) noexcept { return p.bed.x>=p.basket.x?1.f:-1.f; }
inline Vector3 PumpkinBasket(const Pickup& p) noexcept {
    // Scale the floor with the prize fruit so it need not shrink to fit a fixed crate.
    const float load=std::clamp((p.crop.parts[0].scale.z/p.unit-.48f)/.50f,0.f,1.f);
    return {1.16f+.52f*load,.76f+.24f*load,1.16f+.88f*load};
}
inline BasketLanding Contact(const Pickup& p) noexcept {
    if(p.crop.parts[0].shape!=FarmMeshShape::Pumpkin) {
        BasketLanding hit{}; hit.unit=p.unit; hit.size=p.size; hit.spillsSoil=false;
        return hit;
    }
    const float side=LandingSide(p);
    const auto size=PumpkinBasket(p)*p.unit;
    return {p.basket+Vector3{side*.515f*size.x,kGripHeight*size.y,-.249f*size.z},
        p.bed.y+kFarmSoilRidgeHeight,side,p.unit,p.moisture,p.size,p.basket,{kCrateHalfWidth*size.x,.36f*size.z}};
}
inline float BasketRoll(const Pickup& p,float age) noexcept {
    constexpr float tip=.10f, rebound=.24f, rest=.44f;
    const float t=age-kContact;
    if(t<=0.f || t>=rest) return 0.f;
    const float swing=t<tip?1.f-(1.f-t/tip)*(1.f-t/tip):t<rebound?
        1.f-1.23f*Smooth((t-tip)/(rebound-tip)):-.23f*(1.f-Smooth((t-rebound)/(rest-rebound)));
    const float weight=std::clamp(p.crop.parts[0].scale.x/p.unit,.45f,1.2f);
    return -LandingSide(p)*(.019f+.018f*weight)*BySize(p.size,.35f,1.f,1.12f)*swing;
}
inline Parts Sample(const Pickup& p,float age) noexcept {
    Parts out;
    const float pack=1.f-Smooth((age-kPack)/(kDuration-kPack));
    if(pack<=.001f || p.unit<=0 || p.crop.count!=2) return out;
    const auto append=[&](FarmMeshPart part) {
        if(part.scale.x>=.0001f && part.scale.y>=.0001f && part.scale.z>=.0001f && out.count<out.parts.size())
            out.parts[out.count++]=part;
    };
    const auto& body=p.crop.parts[0];
    const bool tomato=body.shape==FarmMeshShape::Tomato;
    const bool pumpkin=body.shape==FarmMeshShape::Pumpkin;
    const bool root=body.shape==FarmMeshShape::Carrot || body.shape==FarmMeshShape::Turnip;
    const float basketSize=p.unit*Smooth(age/.13f)*pack;
    const auto dimensions=pumpkin?PumpkinBasket(p):Vector3{1,tomato?.68f:1.f,1};
    const float depth=dimensions.y, width=dimensions.x;
    const float roll=pumpkin?BasketRoll(p,age):0.f;
    // Roll about whichever bottom edge bears the weight; the base never sinks through the bed.
    const Vector3 foot{(roll<0.f?1.f:-1.f)*kCrateHalfWidth*width*basketSize,kCrateBase*depth*basketSize,0};
    const Vector3 origin=p.basket+foot-Roll(foot,roll);
    const auto basket=[&](FarmMeshShape shape,Vector4 color){
        FarmMeshPart part{}; part.position=origin; part.scale=dimensions*basketSize; part.roll=roll;
        part.shape=shape; part.color=color; append(part);
    };
    basket(FarmMeshShape::HarvestBasket,{.50f,.31f,.17f,1.f});
    basket(FarmMeshShape::HarvestBasketRim,{.71f,.49f,.28f,1.f});
    if(tomato) {
        auto vine=p.crop.parts[1];
        vine.scale=vine.scale*(1.f-Smooth((age-.56f)/.40f));
        append(vine);
    }
    for(int i=0;i<(tomato?2:1);++i) {
        auto produce=body;
        float fit=1.f;
        Vector3 start=body.position,free=start,seat=p.basket;
        if(tomato) {
            // Centres from the field mesh at cultivar size 1. Only the two skin islands move.
            const float size=body.scale.x/.42f;
            start=p.bed+(i?Vector3{.202f,.540f,-.034f}:Vector3{-.171f,.409f,.045f})*size;
            produce.shape=i?FarmMeshShape::PickedTomatoSmall:FarmMeshShape::PickedTomatoLarge;
            fit=(std::min)(1.f,.175f*p.unit/(body.scale.x*(i?.382f:.505f)));
            free=start+Vector3{(i?.075f:-.075f)*p.unit,-.070f*p.unit,-.085f*p.unit};
            seat.x+=(i?.195f:-.195f)*p.unit;
            seat.y+=.080f*p.unit*depth+(i?.215f:.282f)*body.scale.y*fit;
        } else if(pumpkin) {
            free.y+=.23f*p.unit;
            fit=(std::min)({1.f,.35f*p.unit*dimensions.z/body.scale.z,.40f*p.unit*width/body.scale.x});
            seat.x+=LandingSide(p)*.055f*p.unit*width;
            seat.y+=kCrateFloor*p.unit*depth;
        } else {
            free.y=(std::max)(start.y,p.bed.y)+p.unit*(root?BySize(p.size,.29f,.42f,.64f):.31f);
            fit=root?(std::min)(1.f,.81f*p.unit/body.scale.y):(std::min)(1.f,.27f*p.unit/body.scale.z);
            seat.y+=.085f*p.unit;
        }
        const float clock=(std::max)(0.f,age-i*kStagger);
        const float release=std::clamp(clock/kRelease,0.f,1.f);
        const float tug=std::clamp((clock-.055f)/(kRelease-.055f),0.f,1.f);
        const float u=std::clamp((clock-kRelease)/kFlight,0.f,1.f), travel=Smooth(u);
        Vector3 anchor=Mix(start,free,tomato?release*release:1.f-(1.f-tug)*(1.f-tug)*(1.f-tug));
        if(tomato && u>0.f) {
            // Carry the berry just over the near rim, then let it drop inside the opening.
            const float across=Smooth(u/.68f),fall=std::clamp((u-.68f)/.32f,0.f,1.f);
            const float lip=p.basket.y+kGripHeight*p.unit*depth+(i?.215f:.282f)*body.scale.y+.012f;
            anchor=Mix(free,seat,across);
            const float high=free.y+((std::max)(free.y,lip)-free.y)*Smooth(u/.32f);
            anchor.y=high+(seat.y-high)*fall*fall;
        } else if(pumpkin && u>0) {
            // Carry the wide fruit over the mouth before the short, accelerating drop.
            const float dropAt=BySize(p.size,.57f,.66f,.76f);
            const float across=Smooth(u/dropAt),fall=std::clamp((u-dropAt)/(1.f-dropAt),0.f,1.f);
            anchor=Mix(free,seat,across);
            const float high=(std::max)(free.y,p.basket.y+.64f*p.unit);
            anchor.y=free.y+(high-free.y)*Smooth(u/.38f)+(seat.y-high)*fall*fall;
        } else if(!tomato && !pumpkin) {
            anchor=Mix(anchor,seat,travel);
            anchor.y+=4.f*u*(1.f-u)*.67f*p.unit;
        }
        const float bounce=std::clamp((clock-kRelease-kFlight)/kSettle,0.f,1.f);
        if(!pumpkin) anchor.y+=4.f*bounce*(1.f-bounce)*BySize(p.size,.091f,.042f,.016f)*p.unit;
        // After its own contact each tomato rolls away from the other without moving the crate.
        const float slide=tomato?(i?1.f:-1.f)*BySize(p.size,.093f,.055f,.025f)*p.unit*Smooth(bounce):0.f;
        anchor.x+=slide;
        const float fruitRoll=tomato?-slide/(std::max)(.0001f,body.scale.x*fit*(i?.382f:.505f)):0.f;
        // The flattened, faceted skin needs slightly more floor clearance when tilted.
        anchor.y+=.10f*body.scale.x*fit*std::abs(std::sin(fruitRoll));
        const float scale=(1.f+(fit-1.f)*travel)*pack;
        produce.position=origin+Roll((anchor-p.basket)*pack,roll);
        produce.scale=body.scale*scale;
        produce.roll=root?.40f*travel:tomato?fruitRoll:roll;
        append(produce);
        if(root) {
            auto leaf=p.crop.parts[1];
            leaf.position=produce.position+Roll((leaf.position-start)*scale,produce.roll);
            leaf.scale=leaf.scale*scale; leaf.roll=produce.roll;
            append(leaf);
        }
    }
    return out;
}
}
