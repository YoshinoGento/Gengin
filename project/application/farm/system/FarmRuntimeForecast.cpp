#include "farm/system/FarmRuntimeController.h"
#include "farm/system/FarmRuntimeContext.h"
#include "farm/system/FarmVisualSystem.h"
#include "farm/ui/FarmTerrainView.h"

void FarmRuntimeController::InspectForecast(const FarmRuntimeContext& s, farmui::TerrainViewState& state) {
    const auto& preview = s.farmIrrigationPreviewSystem;
    const auto* grid = preview.GetPreviewGrid();
    if (!grid) return;
    const auto& forecast = preview.Forecast();
    state.forecastAvailable = true;
    state.forecastCounts = forecast.Summary();
    Vector3 origin{}, direction{};
    if (!state.forecastCounts.fields || !s.host.IsViewportFocused() || !s.host.IsViewportHovered() ||
        farmui::View::kTerrainTop.Contains(pointer_) || farmui::View::kTerrainBottom.Contains(pointer_) ||
        !s.host.TryBuildViewportRay(origin,direction)) {
        forecastPick_.valid = false;
        return;
    }
    const auto same = [](const Vector3& a, const Vector3& b) { return a.x == b.x && a.y == b.y && a.z == b.z; };
    if (!forecastPick_.valid || forecastPick_.revision != preview.CandidateRevision() ||
        !same(origin,forecastPick_.origin) || !same(direction,forecastPick_.direction)) {
        int hit = -1;
        if (!s.farmVisualSystem.TryPickTile(*grid,origin,direction,hit) || !forecast.Inspect(hit)) hit = -1;
        forecastPick_ = {preview.CandidateRevision(),origin,direction,hit,true};
    }
    if (const auto* field = forecast.Inspect(forecastPick_.index)) {
        state.inspected = *field;
        const auto* tile = grid->GetTile(forecastPick_.index);
        state.inspectedClosed = tile && !tile->irrigationEnabled;
    }
}
