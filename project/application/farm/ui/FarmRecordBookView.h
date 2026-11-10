#pragma once
#include "farm/ui/FarmRuntimeUI.h"
#include "farm/system/FarmRecordBookSystem.h"

namespace farmui {
inline void BuildRecordBookView(View& view, const FarmRecordBookSystem::View& state) {
    view.Add(Label::CropBook, {176,128,450,44});
    view.Metric(Label::BookCollected, {646,128,450,44}, 270,
        std::to_string(state.collected) + " / " + std::to_string(state.rows.size()));
    constexpr std::array labels{Label::Carrot, Label::Tomato, Label::Pumpkin};
    for (std::size_t i = 0; i < state.rows.size(); ++i) {
        const auto& row = state.rows[i];
        const float y = 230.0f + static_cast<float>(i) * 96.0f;
        view.Add(labels[i], {176,y,230,44});
        view.Metric(Label::BookQuality, {416,y,330,44}, 184,
            row.harvested ? std::to_string(row.quality) + "/100" : "--");
        view.Metric(Label::BookWeight, {756,y,340,44}, 184,
            row.weightKnown ? std::to_string(row.grams) + "g" : "--");
    }
    view.Add(state.nextCrop == farm::CropType::None ? Label::BookImprove : Label::BookCollectGoal,
        {176,512,920,38});
    view.Add(Label::BookIndependent, {176,562,920,38});
}
}
