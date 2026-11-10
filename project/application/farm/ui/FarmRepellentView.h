#pragma once

#include "farm/ui/FarmRuntimeUI.h"
#include "farm/system/FarmRepellentSystem.h"

namespace farmui {
inline void BuildRepellentView(View& view, const FarmRepellentSystem::View& state) {
    const auto request = [&](Action action, int index) { return Request{action, index, 0, state.generation}; };
    view.Add(Label::Repellent, {176,130,920,38});
    view.Add(state.present ? Label::RepellentRemovePreview : Label::RepellentPlacePreview, {176,184,920,34});
    for (std::size_t i = 0; i < state.cells.size(); ++i) {
        const auto& cell = state.cells[i];
        const float x = 176.0f + static_cast<float>(i % 3) * 150.0f;
        const float y = 238.0f + static_cast<float>(i / 3) * 108.0f;
        using Kind = FarmRepellentSystem::CellKind;
        const Label label = cell.index < 0 ? Label::RepellentOutside :
            cell.kind == Kind::Plant ? Label::Repellent : cell.kind == Kind::Water ? Label::RepellentWater :
            cell.kind == Kind::Crop ? Label::RepellentCrop : Label::RepellentSoil;
        view.Add(label, {x,y,138,50}, request(Action::RepellentSelect,cell.index), cell.index >= 0,
            cell.index == state.selected);
        if (cell.index >= 0)
            view.Add(cell.protectedAfter ? Label::RepellentProtected : Label::RepellentUnprotected,
                {x,y+54,138,30});
    }
    view.Metric(Label::RepellentTarget, {680,238,416,38}, 190, std::to_string(state.selected));
    view.Add(Label::RepellentRule, {680,294,416,34});
    view.Add(Label::RepellentCost, {680,340,416,34});
    view.Add(Label::RepellentCureRule, {680,386,416,34});
    view.Add(state.pestsEnabled ? Label::RepellentPrevention : Label::RepellentDisabled,
        {680,432,416,34});
    view.Add(state.present ? Label::RepellentRemove : Label::RepellentPlace, {680,494,416,50},
        request(state.present ? Action::RepellentRemove : Action::RepellentPlace,state.selected),
        state.present ? state.canRemove : state.canPlace);
    view.Add(Label::PreviousTile, {176,566,213,44}, request(Action::RepellentSelect,state.selected-1),state.selected>0);
    view.Add(Label::NextTile, {401,566,213,44}, request(Action::RepellentSelect,state.selected+1),
        state.selected>=0 && state.selected+1<state.tileCount);
    view.Add(Label::RepellentEmptyOnly, {680,566,416,34});
}
}
