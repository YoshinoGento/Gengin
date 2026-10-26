#include "scene/SceneTransitionSystem.h"
#include <iostream>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>

namespace {
void Check(bool value, std::source_location at = std::source_location::current()) {
    if (!value) throw std::runtime_error("transition assertion at line "+std::to_string(at.line()));
}
void Advance(scene::SceneTransitionSystem& system, float seconds, bool released = true) {
    while (seconds > 0) {
        const float step = (std::min)(seconds, .05f);
        system.Update(step, released);
        seconds -= step;
    }
}
void VerifyRipplePacing() {
    using namespace scene;
    constexpr float epsilon = .00001f;
    for (const bool pondEnabled : {false, true}) {
        for (const float step : {1.0f/30, 1.0f/60, 1.0f/144, .25f, 10000.0f}) {
            SceneTransitionSystem system;
            PondRipplePath pond;
            pond.enabled = pondEnabled;
            const float fall = pondEnabled ? kPondDropFallSeconds : kDropFallSeconds;
            const float expectedCover = pondEnabled ? .80f : .66f;
            const float expectedTotal = pondEnabled ? 1.25f : 1.11f;
            Check(std::abs(fall+kRipplePreviewSeconds+kRippleWipeSeconds-expectedCover) < epsilon);
            Check(std::abs(expectedCover+kRippleRevealSeconds-expectedTotal) < epsilon);
            Check(system.Begin({},TransitionTarget::SceneReplacement,
                TransitionSoundTheme::Water,TransitionStyle::Ripple,pond));
            float elapsed = 0;
            unsigned drops = 0, sweeps = 0;
            const float boundedStep = (std::min)(step,kTransitionMaximumDelta);
            const float sweepAt = fall+(pondEnabled ? kPondSweepDelaySeconds : kRipplePreviewSeconds);
            while (!system.CanSwap()) {
                Check(elapsed < 2);
                Check(!system.CanSwap() && !system.BeginReveal());
                const float before = elapsed;
                system.Update(step,false);
                elapsed += boundedStep;
                const auto cues = system.ConsumeSoundCues();
                const bool drop = (cues & SoundBit(TransitionSoundCue::WaterDrop)) != 0;
                const bool sweep = (cues & SoundBit(TransitionSoundCue::WaterSweep)) != 0;
                Check(drop == (before < fall && elapsed >= fall));
                Check(sweep == (before < sweepAt && elapsed >= sweepAt));
                drops += drop ? 1u : 0u;
                sweeps += sweep ? 1u : 0u;
                Check(system.ConsumeSoundCues() == 0);
                Check(!system.CanSwap() && !system.BeginReveal());
                system.NotifyCoverPresented();
            }
            Check(elapsed >= expectedCover-epsilon && elapsed <= expectedCover+boundedStep+epsilon);
            Check(drops == 1 && sweeps == 1 && system.CanSwap());
            const float coveredTime = system.GetFrame().presentationTime;
            Advance(system, 3, false);
            Check(system.IsCovering() && system.CanSwap() && system.GetFrame().progress == 1);
            Check(system.GetFrame().presentationTime > coveredTime+2.99f);
            Check(system.ConsumeSoundCues() == 0);
            Check(system.BeginReveal());
            Check(system.ConsumeSoundCues() == SoundBit(TransitionSoundCue::WaterReveal));
            float revealElapsed = 0;
            while (system.GetFrame().phase == TransitionPhase::Revealing) {
                Check(revealElapsed < 1);
                system.Update(step,false);
                revealElapsed += boundedStep;
                Check(system.ConsumeSoundCues() == 0);
            }
            Check(revealElapsed >= .45f-epsilon && revealElapsed <= .45f+boundedStep+epsilon);
            Check(elapsed+revealElapsed <= expectedTotal+2*boundedStep+epsilon);
            Check(system.GetFrame().phase == TransitionPhase::WaitingForRelease);
            Advance(system, 1, false);
            Check(system.BlocksIncomingUpdate());
            system.Update(0,true);
            Check(!system.IsActive());
        }
    }
    std::cout << "Ripple pacing: pond1.25s/screen1.11s, 30/60/144Hz and hitches, exact-once cues, loading/release gates PASS\n";
}
}
int main() try {
    VerifyRipplePacing();
    using scene::TransitionSoundCue;
    using scene::TransitionSoundTheme;
    const auto bit = scene::SoundBit;
    using scene::TransitionStyle;
    using scene::TransitionPhase;
    for (bool pondEnabled : {false,true}) {
        scene::SceneTransitionSystem mirror;
        scene::PondRipplePath pond;
        pond.enabled = pondEnabled;
        for (int cycle=0; cycle<100; ++cycle) {
            Check(mirror.GetFrame().presentationTime == 0);
            Check(mirror.Begin({},scene::TransitionTarget::SceneReplacement,
                TransitionSoundTheme::Water,TransitionStyle::Ripple,pond));
            float previousTime = 0;
            while (mirror.GetFrame().progress < 1) {
                mirror.Update(.016f,false);
                Check(mirror.GetFrame().presentationTime >= previousTime);
                previousTime = mirror.GetFrame().presentationTime;
            }
            for (int wait=0; wait<10; ++wait) mirror.Update(.1f,false);
            Check(mirror.GetFrame().presentationTime > previousTime+.9f);
            Check(mirror.GetFrame().progress == 1);
            previousTime = mirror.GetFrame().presentationTime;
            Check(!mirror.BeginReveal());
            mirror.NotifyCoverPresented();
            Check(mirror.BeginReveal());
            Check(mirror.GetFrame().presentationTime == previousTime);
            mirror.Update(std::numeric_limits<float>::quiet_NaN(),false);
            Check(mirror.GetFrame().presentationTime == previousTime);
            while (mirror.GetFrame().phase == TransitionPhase::Revealing) {
                mirror.Update(.016f,false);
                Check(mirror.GetFrame().presentationTime >= previousTime);
                previousTime = mirror.GetFrame().presentationTime;
            }
            mirror.Update(0,true);
        }
    }
    std::cout << "Water mirror: 200 pond/screen cycles, continuous replacement time, held cover and reset PASS\n";
    scene::SceneTransitionSystem shop;
    Check(!shop.Begin({},scene::TransitionTarget::SceneReplacement,TransitionSoundTheme::SeedShop,TransitionStyle::ShopEnter));
    Check(!shop.Begin({},scene::TransitionTarget::ScreenPresentation,TransitionSoundTheme::SeedShop,static_cast<TransitionStyle>(99)));
    Check(!shop.IsActive() && shop.ConsumeSoundCues() == 0);
    for (int run=0; run<100; ++run) {
        const bool enter = run % 2 == 0;
        const auto style = enter ? TransitionStyle::ShopEnter : TransitionStyle::ShopExit;
        const float step = std::array{.016f,.1f,.25f,10000.0f}[static_cast<std::size_t>(run)%4];
        Check(shop.Begin({},scene::TransitionTarget::ScreenPresentation,
            enter ? TransitionSoundTheme::SeedShop : TransitionSoundTheme::ReturnToFarm,style));
        Check(shop.ConsumeSoundCues() == bit(enter ? TransitionSoundCue::ShopEnter : TransitionSoundCue::FarmReturn));
        Check(shop.GetFrame().style == style);
        auto motion = scene::MakeTransitionImageMotion(shop.GetFrame());
        Check(motion.scale == 1 && motion.fade == 0);
        float previousFade = 0, previousScale = 1, elapsed = 0;
        while (shop.GetFrame().progress < 1) {
            Check(!shop.ConsumeScreenChange() && !shop.BeginReveal());
            shop.NotifyCoverPresented(); Check(!shop.CanSwap());
            shop.Update(step,false);
            elapsed += (std::min)(step,scene::kTransitionMaximumDelta);
            motion = scene::MakeTransitionImageMotion(shop.GetFrame());
            Check(std::isfinite(motion.scale) && std::isfinite(motion.fade));
            Check(motion.fade >= previousFade);
            Check(enter ? motion.scale >= previousScale : motion.scale <= previousScale);
            previousFade = motion.fade; previousScale = motion.scale;
            for (const auto& drop : shop.GetFrame().drops) Check(!drop.active);
            Check(shop.ConsumeSoundCues() == 0 && !shop.AddDrop({}));
        }
        Check(elapsed >= scene::kShopCoverSeconds && elapsed < scene::kRippleCoverSeconds);
        Check(motion.fade == 1 && !shop.ConsumeScreenChange());
        Check(!shop.Begin({},scene::TransitionTarget::ScreenPresentation));
        shop.NotifyCoverPresented();
        Check(shop.ConsumeScreenChange() && !shop.ConsumeScreenChange() && shop.BeginReveal());
        motion = scene::MakeTransitionImageMotion(shop.GetFrame());
        Check(motion.fade == 1);
        Check(motion.scale == (enter ? scene::kShopDistantScale : scene::kFarmApproachScale));
        previousFade = 1; previousScale = motion.scale;
        while (shop.GetFrame().phase == TransitionPhase::Revealing) {
            shop.Update(step,false);
            motion = scene::MakeTransitionImageMotion(shop.GetFrame());
            Check(motion.fade <= previousFade);
            Check(enter ? motion.scale >= previousScale : motion.scale <= previousScale);
            previousFade = motion.fade; previousScale = motion.scale;
            Check(shop.ConsumeSoundCues() == 0);
        }
        Check(motion.scale == 1 && motion.fade == 0 && shop.BlocksIncomingUpdate());
        for (int held=0; held<100; ++held) shop.Update(.016f,false);
        Check(shop.BlocksIncomingUpdate());
        shop.Update(0,true); Check(!shop.IsActive());
    }
    Check(shop.Begin());
    Check(shop.GetFrame().style == TransitionStyle::Ripple && shop.GetFrame().drops[0].active);
    scene::TransitionFrame invalidFrame;
    invalidFrame.phase = TransitionPhase::Covering;
    invalidFrame.style = TransitionStyle::ShopEnter;
    invalidFrame.progress = std::numeric_limits<float>::quiet_NaN();
    Check(scene::MakeTransitionImageMotion(invalidFrame).scale == 1);
    invalidFrame.progress = .5f;
    invalidFrame.style = static_cast<TransitionStyle>(99);
    Check(scene::MakeTransitionImageMotion(invalidFrame).fade == 0);
    std::cout << "Shop transitions: 100 enter/exit cycles, monotonic motion/fade, opaque-present gate, held input, exact-once audio, ripple reset PASS\n";
    Check(bit(static_cast<TransitionSoundCue>(99)) == 0);
    for (float step : {.016f,.1f,.25f,10000.0f}) {
        scene::SceneTransitionSystem audioTiming;
        Check(audioTiming.ConsumeSoundCues() == 0);
        Check(audioTiming.Begin());
        Check(audioTiming.ConsumeSoundCues() == 0);
        Check(!audioTiming.Begin({},scene::TransitionTarget::ScreenPresentation,TransitionSoundTheme::SeedShop));
        float age = 0;
        unsigned dropCount = 0, sweepCount = 0;
        while (audioTiming.GetFrame().progress < 1) {
            const float before = age;
            age += (std::min)(step,scene::kTransitionMaximumDelta);
            audioTiming.Update(step,true);
            const auto cues = audioTiming.ConsumeSoundCues();
            Check(((cues & bit(TransitionSoundCue::WaterDrop)) != 0) ==
                (before < scene::kDropFallSeconds && age >= scene::kDropFallSeconds));
            constexpr float sweep = scene::kDropFallSeconds+scene::kRipplePreviewSeconds;
            Check(((cues & bit(TransitionSoundCue::WaterSweep)) != 0) == (before < sweep && age >= sweep));
            if (cues & bit(TransitionSoundCue::WaterDrop)) ++dropCount;
            if (cues & bit(TransitionSoundCue::WaterSweep)) ++sweepCount;
            Check(audioTiming.ConsumeSoundCues() == 0);
        }
        Check(dropCount == 1 && sweepCount == 1 && !audioTiming.BeginReveal());
        Check(audioTiming.ConsumeSoundCues() == 0);
        audioTiming.NotifyCoverPresented();
        Check(audioTiming.BeginReveal());
        Check(audioTiming.ConsumeSoundCues() == bit(TransitionSoundCue::WaterReveal));
        Check(!audioTiming.BeginReveal() && audioTiming.ConsumeSoundCues() == 0);
    }
    const std::array themes{TransitionSoundTheme::SeedShop,TransitionSoundTheme::HarvestDisplay,
        TransitionSoundTheme::ReturnToFarm,TransitionSoundTheme::Ledger};
    const std::array entryCues{TransitionSoundCue::ShopEnter,TransitionSoundCue::DisplayEnter,
        TransitionSoundCue::FarmReturn,TransitionSoundCue::Ledger};
    scene::SceneTransitionSystem reentry;
    for (int run=0; run<100; ++run) {
        const auto index = static_cast<std::size_t>(run) % themes.size();
        Check(reentry.Begin({},scene::TransitionTarget::ScreenPresentation,themes[index]));
        Check(reentry.ConsumeSoundCues() == bit(entryCues[index]));
        while (reentry.GetFrame().progress < 1) reentry.Update(.016f,true);
        Check(reentry.ConsumeSoundCues() == 0);
        reentry.NotifyCoverPresented(); Check(reentry.ConsumeScreenChange() && reentry.BeginReveal());
        while (reentry.GetFrame().phase == scene::TransitionPhase::Revealing) reentry.Update(.016f,true);
        reentry.Update(0,true);
        Check(reentry.ConsumeSoundCues() == 0);
    }
    scene::SceneTransitionSystem system;
    Check(!system.IsActive() && !system.CanSwap());
    system.NotifyCoverPresented();
    Check(!system.CanSwap() && !system.BeginReveal());
    Check(system.Begin() && !system.Begin());
    Check(!system.ConsumeScreenChange());
    for (float invalid : {-1.0f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
        system.Update(invalid,true);
        Check(system.GetFrame().progress == 0 && !system.CanSwap());
    }
    Advance(system,scene::kDropFallSeconds+scene::kRipplePreviewSeconds*.5f);
    Check(system.GetFrame().progress == 0 && system.GetFrame().drops[0].age >= scene::kDropFallSeconds);
    system.NotifyCoverPresented();
    Check(!system.CanSwap());
    Advance(system,scene::kRipplePreviewSeconds*.5f+scene::kRippleWipeSeconds*.5f);
    Check(system.GetFrame().progress > 0 && system.GetFrame().progress < 1);
    system.NotifyCoverPresented(); Check(!system.CanSwap());
    Advance(system,scene::kRippleWipeSeconds*.5f+.001f);
    Check(system.GetFrame().progress == 1 && !system.CanSwap());
    system.NotifyCoverPresented();
    Check(!system.ConsumeScreenChange());
    Check(system.CanSwap() && system.BeginReveal());
    Check(!system.CanSwap() && !system.BeginReveal() && !system.Begin());
    Check(system.BlocksIncomingUpdate() && system.GetFrame().progress == 0);
    system.Update(.2f,false);
    Check(system.GetFrame().progress > 0 && system.GetFrame().progress < 1);
    system.Update(.2f,true);
    Check(system.GetFrame().phase == scene::TransitionPhase::Revealing);
    while (system.GetFrame().phase == scene::TransitionPhase::Revealing) system.Update(.016f,true);
    Check(system.GetFrame().phase == scene::TransitionPhase::WaitingForRelease);
    Check(system.IsActive() && system.GetFrame().progress == 1);
    for (int i=0; i<100; ++i) system.Update(.016f,false);
    Check(system.BlocksIncomingUpdate());
    system.Update(0,true);
    Check(!system.IsActive() && !system.BlocksIncomingUpdate());
    for (std::size_t i=0; i<scene::kMaximumWaterDrops; ++i)
        Check(system.AddDrop({static_cast<float>(i)*.2f,.3f}));
    Check(!system.AddDrop({.5f,.5f}) && !system.IsActive());
    const auto before = system.GetFrame();
    system.Update(std::numeric_limits<float>::quiet_NaN(),true);
    Check(system.GetFrame().drops[0].age == before.drops[0].age);
    for (int i=0; i<12; ++i) system.Update(.25f,true);
    for (const auto& drop : system.GetFrame().drops) Check(!drop.active);
    Check(!system.AddDrop({-1,.5f}) && !system.AddDrop({.5f,2}));
    Check(!system.AddDrop({std::numeric_limits<float>::infinity(),.5f}));
    Check(system.AddDrop({0,1}));
    system.ClearDrops(); Check(!system.GetFrame().drops[0].active);
    Check(system.Begin({.12f,.83f}));
    Check(system.GetFrame().center.x == .12f && system.GetFrame().center.y == .83f);
    Check(!system.AddDrop({.5f,.5f}));
    while (system.GetFrame().progress < 1) system.Update(.25f,true);
    system.NotifyCoverPresented(); Check(system.BeginReveal());
    Check(!system.GetFrame().drops[0].active);
    while (system.GetFrame().phase == scene::TransitionPhase::Revealing) system.Update(.016f,true);
    system.Update(0,true);
    Check(system.Begin());
    system.Update(10000,true);
    Check(system.GetFrame().progress < 1); // Hitch cannot bypass the bounded cover.
    for (int run=0; run<100; ++run) {
        const bool screen = run > 0 && run % 2 == 0;
        if (!system.IsActive()) Check(system.Begin({}, screen ? scene::TransitionTarget::ScreenPresentation : scene::TransitionTarget::SceneReplacement));
        Check(!system.ConsumeScreenChange());
        float previous = system.GetFrame().progress;
        while (system.GetFrame().progress < 1) {
            system.Update(.016f,true);
            Check(system.GetFrame().progress >= previous);
            previous = system.GetFrame().progress;
        }
        Check(!system.CanSwap());
        Check(!system.ConsumeScreenChange());
        system.NotifyCoverPresented();
        Check(system.ConsumeScreenChange() == screen);
        Check(!system.ConsumeScreenChange());
        Check(!system.Begin({}, scene::TransitionTarget::ScreenPresentation));
        Check(system.BeginReveal());
        Check(!system.ConsumeScreenChange());
        previous = 0;
        while (system.GetFrame().phase == scene::TransitionPhase::Revealing) {
            system.Update(.016f,true);
            Check(system.GetFrame().progress >= previous);
            previous = system.GetFrame().progress;
        }
        Check(system.IsActive());
        system.Update(0,true);
        Check(!system.IsActive());
    }
    Check(system.Begin({-1,2}));
    Check(system.GetFrame().center.x == 0 && system.GetFrame().center.y == 1);
    std::cout << "Scene transition: scene/screen presented-cover, exact-once consume, held-release, 100 alternating repeats, click center, four-drop cap/lifetime PASS\n";
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
