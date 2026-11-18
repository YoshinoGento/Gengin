#pragma once
#include <array>
#include <string_view>

namespace farm::cheerassets {
// Generated speech metadata; durations measured from the shipped PCM.
inline constexpr char kAssetContract[] = "FarmContestSpeech:sha256:e4cf5c065a62a8b16537ac14237093ce7fa33f29fb2c696f5b3ee9f8e0205613";
struct Clip { const char* file; float seconds; };
inline constexpr float kMaximumVoiceSeconds = 4.5f;
inline constexpr std::array<std::string_view,16> kPairNames{{
    "Perfect",
    "PumpkinPot",
    "PumpkinFestival",
    "CarrotCurry",
    "CarrotBundle",
    "TomatoCenter",
    "TomatoLunch",
    "TurnipStar",
    "LargeDelivery",
    "LargeShop",
    "QualityMuseum",
    "QualityKnife",
    "NutrientSoil",
    "WaterCare",
    "Portrait",
    "Guests",
}};
inline constexpr std::array<Clip,32> kClips{{
    {"cheers/Perfect_L.wav", 1.389917f},
    {"cheers/Perfect_R.wav", 2.509333f},
    {"cheers/PumpkinPot_L.wav", 1.482708f},
    {"cheers/PumpkinPot_R.wav", 1.722583f},
    {"cheers/PumpkinFestival_L.wav", 1.979792f},
    {"cheers/PumpkinFestival_R.wav", 2.627917f},
    {"cheers/CarrotCurry_L.wav", 1.702708f},
    {"cheers/CarrotCurry_R.wav", 1.459958f},
    {"cheers/CarrotBundle_L.wav", 2.645917f},
    {"cheers/CarrotBundle_R.wav", 2.352417f},
    {"cheers/TomatoCenter_L.wav", 1.909917f},
    {"cheers/TomatoCenter_R.wav", 2.020250f},
    {"cheers/TomatoLunch_L.wav", 2.040917f},
    {"cheers/TomatoLunch_R.wav", 2.371042f},
    {"cheers/TurnipStar_L.wav", 2.137042f},
    {"cheers/TurnipStar_R.wav", 2.058542f},
    {"cheers/LargeDelivery_L.wav", 2.407542f},
    {"cheers/LargeDelivery_R.wav", 2.542458f},
    {"cheers/LargeShop_L.wav", 1.649083f},
    {"cheers/LargeShop_R.wav", 1.712292f},
    {"cheers/QualityMuseum_L.wav", 2.792083f},
    {"cheers/QualityMuseum_R.wav", 1.620292f},
    {"cheers/QualityKnife_L.wav", 1.879708f},
    {"cheers/QualityKnife_R.wav", 2.088958f},
    {"cheers/NutrientSoil_L.wav", 1.869167f},
    {"cheers/NutrientSoil_R.wav", 3.065375f},
    {"cheers/WaterCare_L.wav", 1.903375f},
    {"cheers/WaterCare_R.wav", 1.953750f},
    {"cheers/Portrait_L.wav", 1.796375f},
    {"cheers/Portrait_R.wav", 1.974500f},
    {"cheers/Guests_L.wav", 1.736250f},
    {"cheers/Guests_R.wav", 2.541000f},
}};
inline constexpr std::array<float,16> kReplyGaps{{
    0.320f,
    0.340f,
    0.400f,
    0.320f,
    0.360f,
    0.340f,
    0.340f,
    0.360f,
    0.320f,
    0.320f,
    0.400f,
    0.340f,
    0.380f,
    0.360f,
    0.360f,
    0.340f,
}};
inline constexpr unsigned kPcmBytes = 6330544;
}
