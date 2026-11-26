#pragma once
#include "farm/core/FarmTypes.h"

namespace farm {
inline bool CanStartCanal(const FarmTile& tile) noexcept {
    return tile.feature==FarmTileFeature::None && !tile.repellentPlant &&
        tile.state==FarmTileState::Empty && tile.crop==CropType::None &&
        tile.moisture==0.f && tile.growth==0.f;
}
}
