#include <cassert>
#include "farm_test_edit.h"
#include "farm_forecast_fixture.h"
#include "farm/core/FarmGrid.h"
#include "farm/system/FarmIrrigationSystem.h"
#include "farm/system/FarmCanalLessonSystem.h"
#include "farm/system/FarmIrrigationPreviewSystem.h"
#include "farm/system/FarmToolActionSystem.h"
#include "farm/system/FarmEconomySystem.h"
#include "farm/system/FarmGrowthComparisonSystem.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void Check(bool ok, const char* what) { if (!ok) throw std::runtime_error(what); }
}
int main() try {
    using namespace farm;
    {
        namespace F = forecast_fixture;
        using Op = FarmIrrigationPreviewOperation;
        FarmGrid g; Check(F::Build(g), "branch fixture");
        FarmIrrigationPreviewSystem p; p.Initialize();
        const auto& forecast = p.Forecast();
        Check(p.Begin(g,F::branch,Op::PlaceCanalPath), "branch preview");
        const auto summary = forecast.Summary();
        Check(summary.fields == 3 && summary.stronger == 1 && summary.weaker == 1, "branch gain and loss");
        const auto gain = *forecast.Inspect(F::gains), loss = *forecast.Inspect(F::loses);
        Check(gain.before == 0 && gain.after > 0 && loss.before > loss.after && loss.after > 0, "direction");
        Check(forecast.Inspect(F::closed)->after == 0 && !forecast.Inspect(-1) &&
            !forecast.Inspect(20) && !forecast.Inspect(F::branch) && !forecast.Inspect(4), "intake and bounds");
        Check(g.GetTile(F::branch)->feature == FarmTileFeature::None && g.GetTile(F::gains)->moisture == .32f,
            "no simulated moisture in preview");
        const auto rev = p.CandidateRevision();
        Check(!p.VisitCanalPathTile(g,F::closed) && rev == p.CandidateRevision(), "blocked segment keeps forecast");
        Check(p.VisitCanalPathTile(g,7) && p.CandidateRevision() != rev && p.VisitCanalPathTile(g,F::branch),
            "extension and retraction");
        Check(forecast.Inspect(F::gains)->after == gain.after && forecast.Inspect(F::loses)->after == loss.after, "restored prefix");
        FarmToolActionSystem tools; tools.Initialize();
        Check(p.CanConfirm(g) && tools.PlaceCanalPath(g,p.GetChangedTileIndices()) && !p.CanConfirm(g), "commit and detect stale candidate");
        p.Cancel(); Check(!forecast.Inspect(F::gains) && forecast.Summary().stronger == 0, "clear markers");
        FarmIrrigationSystem actual; actual.Initialize(); actual.Rebuild(g);
        Check(std::lround(100*actual.GetIrrigationStrength(F::gains)) == gain.after &&
            std::lround(100*actual.GetIrrigationStrength(F::loses)) == loss.after, "commit matches preview");
        Check(p.Begin(g,F::gains,Op::RaiseTerrain) && p.Forecast().Inspect(F::gains)->after == 0, "no uphill delivery");
        Check(p.Begin(g,10,Op::ToggleWaterSource) && p.Forecast().Inspect(F::loses)->after == 0 &&
            p.GetPreviewIrrigation()->GetWaterStatus(*p.GetPreviewGrid(),F::loses) == FarmWaterStatus::Retained,
            "retained water is not a lasting source");
        std::cout << "PASS: forecast branch gain/loss, closed intake, height, retained water, prefix, parity\n";
    }
    {
        FarmCanalLessonSystem lesson;
        using P = FarmCanalLessonSystem::Phase;
        Check(lesson.Begin() && !lesson.Water().IsInIrrigationRange(lesson.Target()), "lesson starts dry");
        Check(!lesson.Preview(-1) && !lesson.Preview(10) && !lesson.Connect(), "invalid or occupied");
        Check(lesson.Preview(12) && lesson.WillReach(), "first prediction");
        lesson.Tick(1);
        Check(lesson.Board().GetTile(12)->feature == FarmTileFeature::None &&
            lesson.Moisture() == lesson.kDry, "no simulation or edit in preview");
        lesson.Cancel(); Check(!lesson.Connect(), "cancel does not commit");
        for (int i = 0; i < 2; ++i) {
            Check(lesson.Preview(i == 0 ? 12 : 14) && lesson.Connect() && !lesson.Connect(), "commit exactly once");
            lesson.Tick(0); Check(lesson.Moisture() == lesson.kDry, "no instantaneous gain");
            for (int step = 0; step < 60 * 10 && lesson.State() != P::Reached; ++step) lesson.Tick(1.f/60);
            Check(lesson.State() == P::Reached && lesson.Moisture() >= lesson.Before()+lesson.kNoticeableGain, "real delivery");
            if (i == 0) {
                Check(lesson.TrySecond() && lesson.Moisture() == lesson.kDry, "independent second crop");
                Check(lesson.Preview(0) && !lesson.WillReach() && !lesson.Connect(), "disconnected preview");
            }
        }
        Check(!lesson.TrySecond(), "lesson ends after second crop");
        std::cout << "Canal lesson: preview, cancel, disconnected route and two watered crops passed\n";
    }
    for (float speed : {1.f, 2.f, 4.f}) {
        FarmGrid grid; Check(grid.Initialize(3, 2), "initialize");
        FarmTile source; source.feature = FarmTileFeature::WaterSource; source.waterAmount = 1;
        FarmTile canal; canal.feature = FarmTileFeature::Canal; canal.waterAmount = 1;
        FarmTile soil; soil.state = FarmTileState::Planted; soil.crop = CropType::Carrot;
        soil.moisture = 0.2f; soil.growth = 0.1f;
        Check(grid.SetTile(0, source) && grid.SetTile(1, canal) && grid.SetTile(2, soil) &&
            grid.SetTile(4, soil), "fixture");
        FarmIrrigationSystem water; water.Initialize(); water.Rebuild(grid);
        FarmToolActionSystem tools; tools.Initialize();
        Check(!tools.CanSetIrrigation(grid, 0), "reject source intake");
        Check(grid.SetSelectedIndex(2), "selection");
        Check(water.UpdateWater(grid, 0.1f, speed), "initial delivery");
        Check(water.GetLastTileFlows(grid)[2].soilReceived > 0, "open delivery");
        const float moisture = grid.GetTile(2)->moisture, stock = grid.GetTile(1)->waterAmount;
        Check(tools.SetSelectedIrrigation(grid, false), "close");
        Check(grid.GetTile(2)->moisture == moisture && grid.GetTile(1)->waterAmount == stock, "close preserves water");
        Check(!water.GetLastStep(grid).valid, "invalidate stale measurements");
        Check(water.GetAvailableIrrigationStrength(grid, 2) == 0 && water.GetAvailableCanalIndex(grid, 2) == -1,
            "closed forecast");
        Check(water.GetWaterStatus(grid, 2) == FarmWaterStatus::None, "closed status");
        Check(!tools.SetSelectedIrrigation(grid, false) && tools.GetHistory().GetUndoCount() == 1, "no-op history");
        for (int step = 0; step < 10; ++step) {
            water.UpdateWater(grid, 0.1f, speed);
            const auto summary = water.GetLastStep(grid);
            Check(summary.valid && std::isfinite(summary.balanceError) && std::abs(summary.balanceError) < 1e-4, "conservation");
            const auto flows = water.GetLastTileFlows(grid);
            Check(flows[2].soilReceived == 0 && flows[4].soilReceived > 0, "independent intake");
            Check(grid.GetTile(2)->moisture == moisture, "closed moisture retained");
        }
        assert(EditFarmTile(grid, 2, [&](auto& candidate) { candidate.growth = 0.4f; }));
        assert(EditFarmTile(grid, 2, [&](auto& candidate) { candidate.moisture = 0.15f; }));
        const float evolvedStock = grid.GetTile(1)->waterAmount;
        Check(tools.Undo() && grid.GetTile(2)->irrigationEnabled, "undo flag");
        Check(grid.GetTile(2)->growth == 0.4f && grid.GetTile(2)->moisture == 0.15f &&
            grid.GetTile(1)->waterAmount == evolvedStock, "undo does not rewind simulation");
        Check(tools.Redo() && !grid.GetTile(2)->irrigationEnabled, "redo flag");
        FarmEconomySystem economy; economy.Initialize();
        Check(tools.ApplyTool(grid, FarmTool::Water, CropType::Carrot, economy), "manual water while closed");
        Check(!grid.GetTile(2)->irrigationEnabled && grid.GetTile(2)->moisture > 0.15f, "manual water retained flag");
        Check(tools.SetSelectedIrrigation(grid, true), "reopen");
        water.UpdateWater(grid, 0.1f, speed);
        Check(water.GetLastTileFlows(grid)[2].soilReceived > 0, "reopened delivery");
        assert(EditFarmTile(grid, 2, [&](auto& candidate) { candidate.heightLevel = 1; }));
        water.Rebuild(grid);
        Check(water.GetAvailableIrrigationStrength(grid, 2) == 0, "no uphill supply");
        assert(EditFarmTile(grid, 2, [&](auto& candidate) { candidate.heightLevel = 0; }));
        Check(grid.SetTile(0, FarmTile{}), "remove source"); water.Rebuild(grid);
        Check(water.GetWaterStatus(grid, 2) == FarmWaterStatus::Retained, "disconnected reservoir available");
        Check(tools.SetSelectedIrrigation(grid, false), "close retained");
        Check(water.GetAvailableIrrigationStrength(grid, 2) == 0, "closed retained blocked");
        Check(!tools.CanSetIrrigation(grid, -1) && !tools.CanSetIrrigation(grid, 6) &&
            !tools.CanSetIrrigation(grid, 1), "bounds and feature guard");
        Check(grid.SetSelectedIndex(1) && !tools.SetSelectedIrrigation(grid, false), "reject canal");
        Check(grid.SetSelectedIndex(2), "reselect");
        FarmIrrigationPreviewSystem preview; preview.Initialize();
        Check(preview.Begin(grid, 5, FarmIrrigationPreviewOperation::RaiseTerrain), "preview");
        Check(preview.CanConfirm(grid), "preview initially valid");
        Check(tools.SetSelectedIrrigation(grid, true) && !preview.CanConfirm(grid), "stale preview");
        Check(tools.SetSelectedIrrigation(grid, false), "snapshot flag");
        FarmGrid::Snapshot snapshot; grid.CaptureSnapshot(snapshot);
        Check(grid.RestoreSnapshot(snapshot) && !grid.GetTile(2)->irrigationEnabled, "snapshot retains flag");
        Check(!tools.Undo(), "reject stale generation");
        Check(grid.Initialize(3, 2) && grid.GetTile(2)->irrigationEnabled, "reset defaults on");
    }
    FarmGrid empty; FarmToolActionSystem tools; tools.Initialize();
    Check(!tools.SetSelectedIrrigation(empty, false), "empty grid");
    FarmGrid compared; Check(compared.Initialize(2, 1), "comparison grid");
    FarmTile crop; crop.state = FarmTileState::Planted; crop.crop = CropType::Carrot;
    Check(compared.SetTile(0,crop) && compared.SetTile(1,crop), "comparison crops");
    FarmGrowthComparisonSystem comparison; comparison.Initialize();
    Check(comparison.Pin(compared,0,0) && comparison.Pin(compared,1,1) && comparison.Start(compared), "comparison start");
    Check(compared.SetSelectedIndex(0) && tools.SetSelectedIrrigation(compared,false), "comparison intake edit");
    comparison.ObserveBeforeStep(compared);
    Check(comparison.GetView(compared).status == FarmComparisonStatus::Invalidated, "comparison invalidated by editing");
    std::cout << "PASS: intake delivery/conservation 1x2x4x, manual water, history, migration state, stale preview and comparison\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << "FAIL: " << error.what() << '\n'; return 1;
}
