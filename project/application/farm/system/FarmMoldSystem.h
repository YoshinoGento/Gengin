#pragma once
#include "farm/core/FarmTypes.h"

class FarmMoldSystem final {
public:
    static constexpr float kRecoverySeconds = 6.0f;
    static constexpr float kSickGrowthMultiplier = 0.90f;
    [[nodiscard]] static float GrowthMultiplier(const farm::FarmMoldState& state) noexcept {
        return state.sick ? kSickGrowthMultiplier : 1.0f;
    }
    // Growth owns the tile mutation. This transition sees moisture after supply and after crop uptake.
    [[nodiscard]] static farm::FarmMoldState Next(farm::FarmMoldState s, bool excess,
        bool intakeOpen, float endMoisture, float dt) noexcept {
        using State = farm::FarmMoldState;
        if (s.protectedSeconds > 0) {
            const float covered = (std::min)(s.protectedSeconds, dt);
            s.protectedSeconds -= covered;
            dt -= covered;
            if (dt <= 0) return s;
        }
        const bool visible = s.Phase() != farm::MoldPhase::Healthy;
        if (!excess || (!intakeOpen && s.exposure > 0 && endMoisture < s.lastMoisture)) {
            if (!visible) return {};
            s.recovering = true;
            // Stop deterioration immediately; only remove accumulated stress once out of excess.
            if (!excess) s.exposure = (std::max)(0.f, s.exposure - dt * State::kOnsetSeconds / kRecoverySeconds);
            if (s.exposure == 0) return {};
        } else {
            s.recovering = false;
            s.exposure = (std::min)(State::kOnsetSeconds, s.exposure + dt);
            s.sick |= s.exposure >= State::kOnsetSeconds;
        }
        s.lastMoisture = endMoisture;
        return s;
    }
};
