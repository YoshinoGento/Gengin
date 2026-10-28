#include "title/TitlePresentationSystem.h"
#include "title/TitleCelestialGeometry.h"
#include "title/TitleCropPresentation.h"
#include "title/TitleLogoRipplePoints.h"
#include "title/TitleGrassPresentation.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <source_location>

namespace {
void Check(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::cerr << "Title assertion at line " << location.line() << std::endl;
    throw std::runtime_error("title presentation assertion failed");
}
bool Near(float a, float b) { return std::abs(a-b) < 0.002f; }
void CloudMotionRegression() {
    for (const std::uint32_t seed : {0u,0x5A17u,12345u}) {
        title::TitleCloudMotionSystem motion;
        motion.Reset(seed);
        Vector2 offset{},velocity{},target{};
        float remaining=0;
        auto rng=seed;
        const auto next=[&] {
            rng=rng*1664525u+1013904223u;
            return static_cast<float>(rng >> 8)/16777216.f;
        };
        for (int step=0;step<12000;++step) {
            const float dt=step%4==0 ? .25f : 1.f/60;
            // Independent copy of the pre-extraction title rule guards the wind/grass contract.
            remaining-=dt;
            if (remaining<=0) {
                remaining=8.f+next()*6.f;
                target={(next()-.35f)*.003f,(next()-.5f)*.00035f};
            }
            const float response=1.f-std::exp(-dt*.45f);
            velocity.x+=(target.x-velocity.x)*response;
            velocity.y+=(target.y-velocity.y)*response;
            offset.x=std::fmod(offset.x+velocity.x*dt+1.f,1.f);
            offset.y=std::clamp(offset.y+velocity.y*dt,-.025f,.025f);
            motion.Update(dt);
            Check(motion.Offset().x==offset.x && motion.Offset().y==offset.y);
            Check(motion.Velocity().x==velocity.x && motion.Velocity().y==velocity.y);
        }
        for (const float invalid : {0.f,-1.f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()})
            motion.Update(invalid);
        Check(motion.Offset().x==offset.x && motion.Offset().y==offset.y);
        motion.Reset(seed);
        Check(motion.Offset().x==0 && motion.Offset().y==0 && motion.Velocity().x==0);
    }
    std::cout<<"PASS shared cloud motion: 36000 legacy-rule frames, invalid delta and reset\n";
}
}
int main() {
    CloudMotionRegression();
    title::TitlePresentationSystem blink;
    Check(blink.Initialize(1,2));
    Check(blink.GetFrame().promptAlpha == 1);
    for (int repeat=0; repeat<16; ++repeat) {
        for (int tick=1; tick<=25; ++tick) {
            blink.Update(.1f);
            const float alpha = blink.GetFrame().promptAlpha;
            Check(std::isfinite(alpha) && alpha >= 0 && alpha <= 1);
            if (tick <= 15 || tick == 25) Check(Near(alpha,1));
            if (tick == 16 || tick == 24) Check(Near(alpha,.5f));
            if (tick >= 17 && tick <= 23) Check(Near(alpha,0));
        }
    }
    const float beforeInvalid = blink.GetFrame().promptAlpha;
    for (const float bad : {-1.0f,0.0f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()})
        blink.Update(bad);
    Check(blink.GetFrame().promptAlpha == beforeInvalid);
    for (const float bad : {-1.0f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()}) {
        Check(title::MakeAudioMix(bad,0).water == 0);
        Check(title::MakeAudioMix(0,bad).water == (bad == -1 ? title::kWaterGain : 0));
    }
    for (int i=0; i<60000; ++i) {
        const float seconds = i*.01f;
        const auto mix = title::MakeAudioMix(seconds,0);
        float total = 0;
        int audible = 0;
        for (auto gain : mix.ambience) {
            Check(std::isfinite(gain) && gain >= 0 && gain <= title::kAmbienceGain);
            total += gain;
            audible += gain>0 ? 1 : 0;
        }
        Check(audible <= 2 && Near(total,title::kAmbienceGain) && Near(mix.water,title::kWaterGain));
        const auto next = title::MakeAudioMix(seconds+.001f,0);
        for (std::size_t j=0; j<mix.ambience.size(); ++j) Check(std::abs(next.ambience[j]-mix.ambience[j]) < .001f);
    }
    for (std::size_t i=0; i<title::kAmbienceCount; ++i) {
        const auto mix = title::MakeAudioMix(static_cast<float>(i)*10,0);
        Check(Near(mix.ambience[i],title::kAmbienceGain));
        const auto silent = title::MakeAudioMix(static_cast<float>(i)*10,1);
        for (float gain : silent.ambience) Check(gain == 0);
        Check(silent.water == 0);
    }
    title::AudioMix malformed;
    malformed.ambience.fill(100);
    malformed.water = 100;
    auto safe = title::SanitizeAudioMix(malformed);
    float total = 0;
    for (float gain : safe.ambience) total += gain;
    Check(Near(total,title::kAmbienceGain) && safe.water == title::kWaterGain);
    malformed.ambience.fill(std::numeric_limits<float>::quiet_NaN());
    malformed.water = std::numeric_limits<float>::infinity();
    safe = title::SanitizeAudioMix(malformed);
    for (float gain : safe.ambience) Check(gain == 0);
    Check(safe.water == 0);
    Check(title::HitTestTitleCanvas(title::kStartCenter));
    Check(title::HitTestTitleCanvas({0,0}));
    Check(title::HitTestTitleCanvas({1280,720}));
    for (const auto resolution : {Vector2{1600,900},Vector2{1280,720},Vector2{1920,1080}}) {
        Check(title::HitTestTitleCanvasInViewport({resolution.x*.5f,resolution.y*650/720},resolution));
        Check(title::HitTestTitleCanvasInViewport({resolution.x*.8f,resolution.y*.9f},resolution));
    }
    Check(!title::HitTestTitleCanvasInViewport({640,650},{0,720}));
    Check(!title::HitTestTitleCanvasInViewport({640,650},{1280,std::numeric_limits<float>::infinity()}));
    for (const auto point : {Vector2{-1,650}, Vector2{1281,650}, Vector2{640,-1},
            Vector2{640,721}, Vector2{640,std::numeric_limits<float>::quiet_NaN()},
            Vector2{std::numeric_limits<float>::infinity(),650}}) {
        Check(!title::HitTestTitleCanvas(point));
    }
    const auto cameraWorld = MatrixMath::MakeAffineMatrix({1,1,1},title::kCameraRotation,title::kCameraPosition);
    const auto projection = MatrixMath::MakePerspectiveFovMatrix(title::kCameraFovY,title::kWidth/title::kHeight,.1f,180);
    float previousExposure = .25f;
    title::TitlePresentationSystem system;
    Check(system.Initialize());
    const auto generation = system.GetFarm().GetGeneration();
    Check(system.GetFarm().GetTileCount() == 20);
    Check(system.GetFarm().GetTile(17)->feature == farm::FarmTileFeature::WaterSource);
    for (int row = 1; row < 4; ++row) {
        const auto* upstream = system.GetFarm().GetTile(row*5+2);
        const auto* downstream = system.GetFarm().GetTile((row-1)*5+2);
        Check(downstream->feature == farm::FarmTileFeature::Canal);
        Check(upstream->heightLevel >= downstream->heightLevel);
    }
    const auto initial = system.GetFrame();
    std::array<bool, title::kTitleTileCount> sawGrowth{}, sawMature{}, sawEmpty{};
    std::array<int, title::kTitleTileCount> clearCounts{};
    system.Update(std::numeric_limits<float>::quiet_NaN());
    system.Update(std::numeric_limits<float>::infinity());
    system.Update(-1.0f);
    Check(system.GetFrame().skyBlend == initial.skyBlend);
    for (int phase = 0; phase < 4; ++phase) {
        const auto frame = system.GetFrame();
        Check(frame.skyFrom == static_cast<std::size_t>(phase));
        Check(frame.skyTo == (frame.skyFrom + 1) % 4);
        Check(Near(frame.skyBlend,0.0f));
        if (phase == 1) Check(Near(frame.groundExposure,.82f));
        if (phase == 3) Check(Near(frame.groundExposure,.10f));
        if (phase == 1) {
            Check(Near(frame.sun.center.x, .50f) && Near(frame.sun.center.y, .10f));
            Check(Near(frame.moon.center.x, .50f) && Near(frame.moon.center.y, 1.06f));
        } else if (phase == 3) {
            Check(Near(frame.moon.center.x, .50f) && Near(frame.moon.center.y, .10f));
            Check(Near(frame.sun.center.x, .50f) && Near(frame.sun.center.y, 1.06f));
        } else {
            // Both discs are below the terrain silhouette before either rises again.
            Check(frame.sun.center.y-.12f > .44f && frame.moon.center.y-.12f > .44f);
            Check(Near(frame.sun.center.x, phase == 0 ? .08f : .92f));
        }
        for (int step = 0; step < 40; ++step) {
            const auto previous = system.GetFrame();
            system.Update(0.25f);
            const auto current = system.GetFrame();
            const int blinkQuarter = (step+1)%10;
            Check(Near(current.promptAlpha,blinkQuarter <= 6 ? 1.0f : 0.0f));
            for (const auto& ripple : current.logoRipples) {
                Check(std::isfinite(ripple.alpha) && ripple.alpha >= 0 && ripple.alpha <= .86f);
                Check(std::isfinite(ripple.radius) && ripple.radius >= 0);
            }
            for (std::size_t i=0; i<current.crops.size(); ++i) {
                const auto& crop = current.crops[i];
                Check(std::isfinite(crop.growth) && crop.growth >= 0 && crop.growth <= 1);
                const auto* tile = system.GetFarm().GetTile(static_cast<int>(i));
                if (!farm::IsPlantableCrop(tile->crop)) { Check(!crop.visible); continue; }
                sawGrowth[i] = sawGrowth[i] || (crop.visible && crop.growth < .5f);
                sawMature[i] = sawMature[i] || (crop.visible && crop.growth == 1);
                sawEmpty[i] = sawEmpty[i] || !crop.visible;
                if (previous.crops[i].visible && !crop.visible) ++clearCounts[i];
                farm::FarmTileVisualData visual;
                visual.valid = true; visual.center = {1,.54f,6}; visual.cropAnchor = visual.center;
                visual.crop = tile->crop;
                const auto parts = title::BuildTitleCropParts(visual,crop);
                if (!crop.visible) Check(parts.count == 0);
                else Check(parts.count >= 1 && parts.count <= 2);
                for (std::size_t part=0; part<parts.count; ++part) {
                    const auto& mesh = parts.parts[part];
                    Check(std::isfinite(mesh.position.y) && mesh.scale.x > .0001f && mesh.scale.y > .0001f);
                    if (mesh.shape == farm::FarmMeshShape::Carrot) Check(mesh.position.y < visual.center.y);
                    if (mesh.shape == farm::FarmMeshShape::TomatoStems || mesh.shape == farm::FarmMeshShape::PumpkinVines)
                        Check(Near(mesh.position.y,visual.center.y+.02f));
                }
            }
            Check(current.skyFrom < 4 && current.skyTo < 4);
            Check(current.skyBlend >= 0 && current.skyBlend <= 1);
            Check(std::isfinite(current.skyYaw) && current.skyYaw >= 0 && current.skyYaw < 1);
            Check(std::isfinite(current.cloudLift) && std::abs(current.cloudLift) <= .025f);
            Check(current.nightAmount >= 0 && current.nightAmount <= 1);
            for (const auto& body : {current.sun, current.moon}) {
                Check(std::isfinite(body.center.x) && std::isfinite(body.center.y));
                Check(body.center.x >= .079f && body.center.x <= .921f);
                Check(body.center.y >= .099f && body.center.y <= 1.061f);
            }
            Check(Near(current.sun.center.x+current.moon.center.x, 1.0f));
            Check(Near(current.sun.center.y+current.moon.center.y, 1.16f));
            if (std::abs(current.sun.center.y-.58f) < .02f) {
                Check(current.sun.center.y-.12f > .44f);
                Check(current.moon.center.y-.12f > .44f);
            }
            Check(std::abs(current.sun.center.x-previous.sun.center.x) < .02f);
            Check(std::abs(current.sun.center.y-previous.sun.center.y) < .02f);
            Check(std::abs(current.moon.center.x-previous.moon.center.x) < .02f);
            Check(std::abs(current.moon.center.y-previous.moon.center.y) < .02f);
            if (phase < 2) Check(current.sun.center.x >= previous.sun.center.x-.0001f);
            else Check(current.moon.center.x >= previous.moon.center.x-.0001f);
            const auto sun = title::CelestialDirection(current.sun,projection,cameraWorld);
            const auto moon = title::CelestialDirection(current.moon,projection,cameraWorld);
            const auto lighting = title::MakeCelestialLighting(sun,moon,current.groundExposure);
            Check(std::isfinite(lighting.direction.y) && lighting.direction.y < 0);
            Check(lighting.shadowStrength >= 0 && lighting.shadowStrength <= .70f);
            Check(Near(MatrixMath::Length(sun),1) && Near(MatrixMath::Length(lighting.direction),1));
            if (sun.y <= .05f) Check(lighting.shadowStrength == 0);
            else Check(MatrixMath::Dot(sun,lighting.direction) < -.9999f);
            if (phase >= 2) Check(lighting.shadowStrength == 0);
            auto movedCamera = cameraWorld;
            movedCamera.m[3][0] += 100;
            Check(Near(title::CelestialDirection(current.sun,projection,movedCamera).x,sun.x));
            Check(std::isfinite(lighting.intensity) && lighting.intensity >= 0 && lighting.intensity <= 1/.18f);
            const float received = lighting.intensity*(.18f+.82f*(-lighting.direction.y));
            Check(Near(received,current.groundExposure));
            Check(std::abs(received-previousExposure) < .023f);
            if (phase == 0 || phase == 3) Check(received >= previousExposure-.0001f);
            else Check(received <= previousExposure+.0001f);
            previousExposure = received;
            Check(std::isfinite(current.waterPhase) && current.waterPhase >= 0 && current.waterPhase < 1);
            for (const auto& star : current.stars) Check(star.alpha >= 0 && star.alpha <= 1);
        }
    }
    const auto wrapped = system.GetFrame();
    Check(wrapped.skyFrom == initial.skyFrom && Near(wrapped.waterPhase, initial.waterPhase));
    Check(Near(wrapped.lightColor.x,initial.lightColor.x));
    Check(Near(wrapped.windAngle,initial.windAngle));
    Check(Near(wrapped.sun.center.x,initial.sun.center.x));
    Check(Near(wrapped.sun.center.y,initial.sun.center.y));
    Check(Near(wrapped.moon.center.x,initial.moon.center.x));
    Check(Near(wrapped.moon.center.y,initial.moon.center.y));
    Check(system.GetFarm().GetGeneration() == generation);
    for (std::size_t i=0; i<wrapped.crops.size(); ++i) {
        Check(Near(wrapped.crops[i].growth,initial.crops[i].growth));
        Check(wrapped.crops[i].visible == initial.crops[i].visible);
        if (farm::IsPlantableCrop(system.GetFarm().GetTile(static_cast<int>(i))->crop))
            Check(sawGrowth[i] && sawMature[i] && sawEmpty[i] && clearCounts[i] == 4);
    }
    farm::FarmTileVisualData visual;
    visual.valid = true; visual.crop = farm::CropType::Carrot;
    const title::CropFrame mature{1,true}, cleared{1,false};
    const auto buried = title::BuildTitleCropParts(visual,mature);
    Check(buried.count == 2 && buried.parts[0].position.y < 0);
    visual.cropStage = farm::FarmCropGrowthStage::Ready;
    const auto shared = farm::BuildFarmCropMeshParts(visual,1.0f);
    for (std::size_t i=0; i<buried.count; ++i) {
        Check(buried.parts[i].position.y == shared.parts[i].position.y);
        Check(buried.parts[i].scale.y == shared.parts[i].scale.y);
    }
    Check(title::BuildTitleCropParts(visual,cleared).count == 0);
    title::TitlePresentationSystem rain, identicalRain, differentRain;
    Check(rain.Initialize(123,987) && identicalRain.Initialize(123,987) && differentRain.Initialize(123,654));
    constexpr float rainDelta = .03125f;
    std::array<bool,title::kLogoRipplePoints.size()> visited{};
    std::uint32_t lastId = 0;
    Vector2 lastCenter{};
    float lastBirth = 0, minimumGap = 10, maximumGap = 0, idle = 0;
    bool distinctSeed = false, sawOverlap = false, sawNightAttenuation = false;
    for (int step=1; step<=19200; ++step) {
        const auto before = rain.GetFrame();
        rain.Update(rainDelta); identicalRain.Update(rainDelta); differentRain.Update(rainDelta);
        const auto frame = rain.GetFrame(), twinFrame = identicalRain.GetFrame(), otherFrame = differentRain.GetFrame();
        int active = 0, births = 0;
        for (std::size_t i=0; i<frame.logoRipples.size(); ++i) {
            const auto& ripple = frame.logoRipples[i];
            const auto& twinRipple = twinFrame.logoRipples[i];
            Check(ripple.id == twinRipple.id && ripple.radius == twinRipple.radius && ripple.alpha == twinRipple.alpha);
            Check(ripple.center.x == twinRipple.center.x && ripple.center.y == twinRipple.center.y);
            distinctSeed = distinctSeed || ripple.id != otherFrame.logoRipples[i].id ||
                ripple.center.x != otherFrame.logoRipples[i].center.x;
            Check(std::isfinite(ripple.radius) && std::isfinite(ripple.alpha));
            Check(ripple.alpha >= 0 && ripple.alpha <= .86f-.22f*frame.nightAmount+.0001f);
            if (!ripple.id) { Check(ripple.alpha == 0); continue; }
            ++active;
            sawNightAttenuation = sawNightAttenuation || (frame.nightAmount > .9f && ripple.alpha > .5f && ripple.alpha < .67f);
            bool validPoint = false;
            for (std::size_t p=0; p<title::kLogoRipplePoints.size(); ++p) {
                const auto point = title::kLogoRipplePoints[p];
                if (ripple.center.x == point.x && ripple.center.y == point.y) { visited[p] = true; validPoint = true; }
            }
            Check(validPoint);
            if (ripple.id == before.logoRipples[i].id) Check(ripple.radius >= before.logoRipples[i].radius);
            if (ripple.id <= lastId) continue;
            ++births;
            Check(ripple.id == lastId+1);
            const float seconds = step*rainDelta;
            if (lastId == 0) Check(seconds >= .9f && seconds <= 1.5f+rainDelta);
            else {
                const float gap = seconds-lastBirth;
                Check(gap >= 1.8f-rainDelta && gap <= 2.8f+rainDelta);
                Check(ripple.center.x != lastCenter.x || ripple.center.y != lastCenter.y);
                minimumGap = (std::min)(minimumGap,gap); maximumGap = (std::max)(maximumGap,gap);
            }
            lastId = ripple.id; lastCenter = ripple.center; lastBirth = seconds;
        }
        Check(active <= 2 && births <= 1);
        if (lastId) Check(active >= 1);
        sawOverlap = sawOverlap || active == 2;
        idle = active ? 0 : idle+rainDelta;
        Check(idle <= 1.5f+rainDelta);
        Check(frame.skyYaw == otherFrame.skyYaw); // Ripple seeds never consume the wind stream.
        const auto repeated = rain.GetFrame();
        Check(repeated.logoRipples[0].radius == frame.logoRipples[0].radius);
    }
    int visitedCount = 0;
    for (bool seen : visited) visitedCount += seen ? 1 : 0;
    Check(lastId >= 214 && lastId <= 334 && visitedCount >= 35);
    std::cout << "Ripple600s: " << lastId << " events; observed gaps " << minimumGap << ".." << maximumGap << "s\n";
    Check(maximumGap-minimumGap > .7f && distinctSeed && sawOverlap && sawNightAttenuation);
    const auto validRain = rain.GetFrame();
    for (const float bad : {0.0f,-1.0f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}) rain.Update(bad);
    Check(rain.GetFrame().logoRipples[0].radius == validRain.logoRipples[0].radius);
    Check(rain.Initialize(123,987));
    for (const auto& ripple : rain.GetFrame().logoRipples) Check(ripple.id == 0 && ripple.alpha == 0);
    title::TitlePresentationSystem coarseRain, fineRain;
    Check(coarseRain.Initialize(123,789) && fineRain.Initialize(123,789));
    for (int step=0; step<4800; ++step) {
        coarseRain.Update(.125f);
        for (int sub=0; sub<4; ++sub) fineRain.Update(.03125f);
        const auto coarse = coarseRain.GetFrame(), fine = fineRain.GetFrame();
        for (std::size_t i=0; i<coarse.logoRipples.size(); ++i) {
            Check(coarse.logoRipples[i].id == fine.logoRipples[i].id);
            Check(coarse.logoRipples[i].center.x == fine.logoRipples[i].center.x);
            Check(std::abs(coarse.logoRipples[i].radius-fine.logoRipples[i].radius) < .1f);
        }
    }
    for (std::uint32_t seed=1; seed<=12; ++seed) {
        Check(rain.Initialize(123,seed));
        std::uint32_t id = 0;
        float seconds = 0, birth = 0;
        for (int step=0; step<1200; ++step) {
            const float delta = step%3 == 0 ? .25f : (step%3 == 1 ? .0625f : .125f);
            rain.Update(delta); seconds += delta;
            const auto frame = rain.GetFrame();
            int births = 0;
            for (const auto& ripple : frame.logoRipples) {
                if (ripple.id <= id) continue;
                Check(ripple.id == id+1);
                if (id) Check(seconds-birth >= title::kLogoRippleMinimumInterval-.25f &&
                    seconds-birth <= title::kLogoRippleMaximumInterval+.25f);
                birth = seconds; id = ripple.id; ++births;
            }
            Check(births <= 1);
        }
        Check(id > 35);
    }
    for (const auto invalid : {title::CropFrame{-1,true},
        title::CropFrame{std::numeric_limits<float>::quiet_NaN(),true},
        title::CropFrame{2,true}}) Check(title::BuildTitleCropParts(visual,invalid).count == 0);
    Check(!system.ConsumeStart());
    system.RequestStart();
    Check(system.IsLeaving() && system.GetFrame().leaving);
    Check(system.GetFrame().uiAlpha == 1.0f);
    Check(system.ConsumeStart()); // Manager, not Title, waits for the opaque cover.
    Check(!system.ConsumeStart());
    system.Update(0.25f);
    Check(system.GetFrame().uiAlpha == 0.0f && system.GetFrame().promptAlpha == 0.0f);
    system.RequestStart();
    for (int i=0; i<4; ++i) system.Update(0.25f);
    Check(!system.ConsumeStart());
    Check(!system.ConsumeStart());
    Check(system.GetFrame().fadeAlpha == 1.0f);
    Check(system.GetFrame().uiAlpha == 0.0f);
    for (const auto& ripple : system.GetFrame().logoRipples) Check(ripple.alpha == 0.0f);
    Check(system.GetFrame().audio.water == 0);
    for (float gain : system.GetFrame().audio.ambience) Check(gain == 0);
    Check(system.Initialize());
    system.Update(.125f);
    const float earlyEntryFade = system.GetFrame().entryFadeAlpha;
    Check(earlyEntryFade > 0 && earlyEntryFade < 1);
    system.RequestStart();
    Check(system.GetFrame().entryFadeAlpha == earlyEntryFade);
    Check(system.ConsumeStart());
    system.Update(.125f);
    Check(system.GetFrame().entryFadeAlpha < earlyEntryFade && system.GetFrame().entryFadeAlpha > 0);
    Check(system.Initialize());
    system.Update(10000.0f);
    Check(system.GetFrame().skyFrom == 0 && system.GetFrame().skyBlend < 0.01f);
    Check(system.Initialize(1,2));
    const auto initialTitle = system.GetFrame();
    Check(initialTitle.entryFadeAlpha == 1);
    system.CompleteEntryFade();
    system.CompleteEntryFade();
    const auto restoredTitle = system.GetFrame();
    Check(restoredTitle.entryFadeAlpha == 0 && restoredTitle.fadeAlpha == 0);
    Check(restoredTitle.skyBlend == initialTitle.skyBlend && restoredTitle.promptAlpha == initialTitle.promptAlpha);
    Check(restoredTitle.grassPhase == initialTitle.grassPhase && !system.IsLeaving());
    Check(system.Initialize(1,2) && system.GetFrame().entryFadeAlpha == 1);
    title::TitlePresentationSystem twin, other;
    Check(system.Initialize(123) && twin.Initialize(123) && other.Initialize(456));
    float previousOffset = 0;
    float previousGrassPhase = 0;
    const auto positiveGrass = title::GrassWindFromCloudVelocity({.003f,0});
    const auto negativeGrass = title::GrassWindFromCloudVelocity({-.003f,0});
    Check(positiveGrass.x > 0 && negativeGrass.x < 0);
    Check(std::abs(positiveGrass.x+negativeGrass.x)<.00001f && std::abs(positiveGrass.y+negativeGrass.y)<.00001f);
    Check(title::GrassWindFromCloudVelocity({std::numeric_limits<float>::quiet_NaN(),0}).x == 0);
    const auto strongGrass = title::GrassWindFromCloudVelocity({1000,0});
    Check(std::abs(std::hypot(strongGrass.x,strongGrass.y)-title::kGrassMaximumBend)<.00001f);
    for (int i=0; i<4800; ++i) {
        system.Update(.016f); twin.Update(.016f); other.Update(.016f);
        const auto frame = system.GetFrame();
        const float difference = std::abs(frame.skyYaw-previousOffset);
        Check((std::min)(difference,1-difference) < .0001f);
        Check(frame.skyYaw == twin.GetFrame().skyYaw);
        Check(frame.grassWindXZ.x == twin.GetFrame().grassWindXZ.x && frame.grassWindXZ.y == twin.GetFrame().grassWindXZ.y);
        Check(frame.grassPhase >= 0 && frame.grassPhase < 2*std::numbers::pi_v<float>);
        Check(std::hypot(frame.grassWindXZ.x,frame.grassWindXZ.y) <= title::kGrassMaximumBend+.00001f);
        const float uvStep = std::fmod(frame.skyYaw-previousOffset+1.5f,1.0f)-.5f;
        const float velocity = uvStep/.016f;
        const float grassStrength = frame.grassWindXZ.x*std::cos(title::kCameraRotation.y)-frame.grassWindXZ.y*std::sin(title::kCameraRotation.y);
        const float waveSpeed = std::clamp(velocity/title::kGrassWaveWindReference,-1.0f,1.0f);
        Check(std::abs(grassStrength-waveSpeed*title::kGrassMaximumBend)<.004f);
        const float phaseStep = std::remainder(frame.grassPhase-previousGrassPhase,2*std::numbers::pi_v<float>);
        const float expectedPhaseStep = .016f*waveSpeed*2*std::numbers::pi_v<float>/title::kGrassFlutterPeriod;
        Check(std::abs(phaseStep-expectedPhaseStep)<.0005f);
        previousGrassPhase = frame.grassPhase;
        previousOffset = frame.skyYaw;
    }
    Check(std::abs(system.GetFrame().skyYaw-other.GetFrame().skyYaw) > .001f);
    auto invalidProjection = projection;
    invalidProjection.m[0][0] = 0;
    Check(title::CelestialDirection(initial.sun,invalidProjection,cameraWorld).y == -1);
    invalidProjection.m[0][0] = std::numeric_limits<float>::quiet_NaN();
    Check(title::CelestialDirection(initial.sun,invalidProjection,cameraWorld).y == -1);
    title::TitlePresentationSystem dawn;
    Check(dawn.Initialize(1,2));
    float previousLeaf = .25f;
    bool sawOldAmplification = false;
    for (int tick=0; tick<8000; ++tick) {
        const auto frame = dawn.GetFrame();
        const auto sun = title::CelestialDirection(frame.sun,projection,cameraWorld);
        const auto moon = title::CelestialDirection(frame.moon,projection,cameraWorld);
        const auto lighting = title::MakeCelestialLighting(sun,moon,frame.groundExposure);
        const float leaf = title::GrassExposure(frame.groundExposure);
        Check(std::isfinite(leaf) && leaf >= 0 && leaf <= 1 && std::abs(leaf-previousLeaf) < .001f);
        const float lateral = std::abs(lighting.direction.x);
        sawOldAmplification |= lighting.intensity*(.38f+.62f*lateral) > leaf*1.5f;
        // Shader response bounds independent of a steeply changing horizon direction or shadow.
        for (float ndotl : {0.0f,.25f,.75f,1.0f}) for (float visibility : {0.0f,.5f,1.0f}) {
            const float received = leaf*(.90f+.10f*ndotl*visibility);
            Check(received >= leaf*.90f-.00001f && received <= leaf+.00001f);
        }
        previousLeaf = leaf;
        dawn.Update(.01f);
    }
    Check(sawOldAmplification);
    Check(title::GrassExposure(-1) == 0 && title::GrassExposure(5) == 1);
    Check(title::GrassExposure(std::numeric_limits<float>::quiet_NaN()) == 0);
    const auto invalidLighting = title::MakeCelestialLighting({0,std::numeric_limits<float>::quiet_NaN(),0},{0,0,0},.1f);
    Check(invalidLighting.shadowStrength == 0 && invalidLighting.direction.y == -1);
    Check(title::MakeCelestialLighting({0,1,0},{0,-1,0},std::numeric_limits<float>::quiet_NaN()).intensity == 0);
    std::cout << "Title: prompt visible/fades/hidden/repeat, bounded rain, 600s sky/crops/start PASS\n";
}
