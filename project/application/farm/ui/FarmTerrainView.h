#pragma once
#include "farm/ui/FarmRuntimeUI.h"
#include "farm/system/FarmIrrigationPreviewSystem.h"
#include "farm/system/FarmGrowthSystem.h"

namespace farmui {
struct TerrainViewState {
    bool preview = false;
    bool canConfirm = false;
    bool canRaise = false, canLower = false, canCanal = false, canSource = false;
    bool canPath = false, canUndo = false, canRedo = false, canCompost = false;
    bool canSetIrrigation = false, irrigationEnabled = true;
    Label status = Label::Terrain;
    std::string tileValues;
    std::size_t changeCount = 0;
    farm::FarmCanalPathIssue pathIssue = farm::FarmCanalPathIssue::None;
    int blockedTileIndex = -1;
    bool forecastAvailable = false, inspectedClosed = false;
    farm::FarmIrrigationForecast::Counts forecastCounts{};
    farm::FarmIrrigationForecast::Field inspected{};
};

// Availability comes from Systems; the view emits requests and never edits tiles.
inline void BuildTerrainView(View& view, const TerrainViewState& state) {
    view.terrain = true;
    const bool rejectedExtension = state.preview &&
        state.pathIssue != farm::FarmCanalPathIssue::None;
    if (state.preview && !state.canConfirm) {
        view.Add(Label::PreviewStale, {26, 24, 610, 38});
    } else if (state.preview && state.pathIssue == farm::FarmCanalPathIssue::BlockedTile) {
        view.Metric(Label::PathBlocked, {26, 24, 610, 38}, 400,
            state.blockedTileIndex >= 0 ? "#" + std::to_string(state.blockedTileIndex) : "--");
    } else {
        view.Add(rejectedExtension ? Label::PathNonStraight : state.status, {26, 24, 610, 38});
    }
    const bool intakeAvailable = !state.preview && state.canSetIrrigation;
    view.Add(Label::IntakeOn, {658, 24, 122, 40}, {Action::SetIrrigation, 1},
        intakeAvailable, state.canSetIrrigation && state.irrigationEnabled);
    view.Add(Label::IntakeOff, {786, 24, 122, 40}, {Action::SetIrrigation, 0},
        intakeAvailable, state.canSetIrrigation && !state.irrigationEnabled);
    view.Add(Label::Resume, {950, 24, 298, 40}, {Action::TerrainExit});
    view.Metric(Label::Tile, {26, 68, 910, 36}, 510, state.tileValues);
    view.Metric(Label::Changed, {950, 68, 298, 36}, 180, std::to_string(state.changeCount));
    if (state.forecastAvailable && state.preview && state.canConfirm) {
        view.irrigationForecast = true;
        view.Metric(Label::ForecastGain,{28,532,552,44},340,std::to_string(state.forecastCounts.stronger));
        view.Metric(Label::ForecastLoss,{648,532,552,44},340,std::to_string(state.forecastCounts.weaker));
        if (state.inspected.Valid() && !state.inspectedClosed)
            view.Metric(Label::ForecastStrength,{28,588,1208,44},492,
                std::to_string(state.inspected.before) + "% -> " + std::to_string(state.inspected.after) + "%");
        else view.Add(state.inspectedClosed ? Label::ForecastClosed : state.forecastCounts.fields == 0 ?
            Label::ForecastEmpty : Label::ForecastHover,{28,588,1208,44});
        view.Add(rejectedExtension ? Label::ConfirmCandidates : Label::Confirm,{28,644,296,44},{Action::Confirm},true,true);
        view.Add(Label::Cancel,{338,644,296,44},{Action::Cancel});
        view.Add(Label::Overview,{958,644,296,44},{Action::Overview});
        return;
    }
    int index = 0;
    const auto button = [&](Label label, Action action, bool enabled, bool selected = false) {
        const int i = index++;
        view.Add(label, {28.0f + (i % 4)*310, 532.0f + (i / 4)*56, 296, 44},
            {action}, enabled, selected);
    };
    button(Label::Raise, Action::Raise, !state.preview && state.canRaise);
    button(Label::Lower, Action::Lower, !state.preview && state.canLower);
    button(Label::Canal, Action::Canal, !state.preview && state.canCanal);
    button(Label::Source, Action::Source, !state.preview && state.canSource);
    button(Label::Path, Action::Path, !state.preview && state.canPath);
    button(Label::RemovePath, Action::RemovePath, !state.preview && state.canPath);
    button(Label::Undo, Action::Undo, !state.preview && state.canUndo);
    button(Label::Redo, Action::Redo, !state.preview && state.canRedo);
    button(rejectedExtension && state.canConfirm ? Label::ConfirmCandidates : Label::Confirm,
        Action::Confirm, state.preview && state.canConfirm, state.preview);
    button(Label::Cancel, Action::Cancel, state.preview);
    button(Label::Compost, Action::Compost, !state.preview && state.canCompost);
    button(Label::Overview, Action::Overview, true);
}

inline void BuildPlayQuickView(View& view, bool paused, bool canEdit, bool canControlTime = true,
    float timeScale = 1.0f) {
    view.IconButton(Label::Terrain, HUDIcon::Terrain, {260, 24, 52, 52}, {Action::TerrainField}, canEdit);
    view.IconButton(!canControlTime ? Label::Paused : paused ? Label::Play : Label::Pause,
        paused ? HUDIcon::Play : HUDIcon::Pause, {328, 24, 52, 52}, {Action::Pause}, canControlTime);
    constexpr std::array labels{Label::SpeedCompact1, Label::SpeedCompact2, Label::SpeedCompact4};
    constexpr std::array speeds{1, 2, 4};
    for (std::size_t i = 0; i < speeds.size(); ++i)
        view.Add(labels[i], {392.0f + static_cast<float>(i) * 60.0f, 30, 56, 40},
            {Action::Speed, speeds[i]}, canControlTime,
            canControlTime && !paused && timeScale == static_cast<float>(speeds[i]));
}

inline void BuildWaterGuidanceView(View& view, const FarmWaterGuidance& state, bool canEdit,
    bool canSetIrrigation = false) {
    if (!state.visible) return;
    view.waterGuidance = true;
    Label supply = Label::SupplyNoneShort;
    if (state.intakeClosed) supply = Label::IntakeClosedShort;
    else switch (state.supply) {
    case farm::FarmWaterStatus::Available: supply = Label::SupplyAvailableShort; break;
    case farm::FarmWaterStatus::Retained: supply = Label::SupplyRetainedShort; break;
    case farm::FarmWaterStatus::Waiting: supply = Label::SupplyWaitingShort; break;
    case farm::FarmWaterStatus::Dry: supply = Label::SupplyDryShort; break;
    default: break;
    }
    Label advice = Label::WaterAdviceUnknownShort;
    switch (state.advice) {
    case FarmWaterAdvice::Till: advice = Label::WaterAdviceTillShort; break;
    case FarmWaterAdvice::Plant: advice = Label::WaterAdvicePlantShort; break;
    case FarmWaterAdvice::Harvest: advice = Label::WaterAdviceHarvestShort; break;
    case FarmWaterAdvice::Water: advice = Label::WaterAdviceWaterShort; break;
    case FarmWaterAdvice::CheckSupply: advice = Label::WaterAdviceSupplyShort; break;
    case FarmWaterAdvice::CloseIntake: advice = Label::WaterAdviceCloseShort; break;
    case FarmWaterAdvice::AvoidWater: advice = Label::WaterAdviceAvoidShort; break;
    case FarmWaterAdvice::Monitor: advice = Label::WaterAdviceMonitorShort; break;
    default: break;
    }
    using M = farm::MoldPhase;
    if (state.mold == M::Warning) advice = state.intakeClosed ? Label::MoldWait : Label::MoldWarning;
    if (state.mold == M::Sick) advice = state.intakeClosed ? Label::MoldSickClosed : Label::MoldSick;
    if (state.mold == M::Recovering) advice = Label::MoldRecovery;
    view.Add(supply, {24, 460, 252, 38}, {Action::TerrainField}, canEdit);
    const auto adviceIndex = view.count;
    view.Add(advice, {24, 502, 314, 38});
    if (view.count > adviceIndex) view.items[adviceIndex].warning = state.mold == M::Sick ||
        state.mold == M::Warning || (state.mold == M::Healthy && state.HasExcessMoisture());
    view.IconButton(state.intakeClosed ? Label::IntakeOn : Label::IntakeOff, HUDIcon::Canal, {284, 460, 54, 38},
        {Action::SetIrrigation, state.intakeClosed ? 1 : 0}, canEdit && canSetIrrigation,
        state.advice == FarmWaterAdvice::CloseIntake);
}
}
