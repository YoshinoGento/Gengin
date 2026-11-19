#pragma once
#include "farm/core/FarmGrid.h"

namespace forecast_fixture {
inline constexpr int branch = 6, gains = 1, loses = 13, closed = 5;
inline bool Build(farm::FarmGrid& g) {
    using namespace farm;
    if (!g.Initialize(5,4)) return false;
    FarmTile c; c.feature = FarmTileFeature::WaterSource; c.waterAmount = 1;
    if (!g.SetTile(10,c)) return false;
    c.feature = FarmTileFeature::Canal;
    if (!g.SetTile(11,c) || !g.SetTile(12,c)) return false;
    FarmTile crop; crop.state = FarmTileState::Planted; crop.crop = CropType::Tomato;
    crop.moisture = .32f; crop.growth = .36f;
    if (!g.SetTile(gains,crop) || !g.SetTile(loses,crop)) return false;
    crop.irrigationEnabled = false;
    return g.SetTile(closed,crop) && g.SetSelectedIndex(branch);
}
}
