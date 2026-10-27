#include "farm/system/FarmDocumentSystem.h"
#include "farm/ui/FarmContestOutcomeView.h"
#include "farm/core/FarmGrid.h"
#include "io/JsonFile.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void CheckView(const farmui::View& view) {
    Check(view.modal && view.contestOutcome && view.count < view.items.size(), "modal/capacity");
    for (std::size_t i=0; i<view.count; ++i) {
        const auto& item=view.items[i]; const auto& r=item.rect;
        Check(r.x>=0 && r.y>=0 && r.width>0 && r.height>0 && r.x+r.width<=1280 && r.y+r.height<=720, "bounds");
        const auto index=static_cast<std::size_t>(item.label);
        Check(index<farmui::kLabels.size(), "label index");
        const float space=item.value.empty() ? r.width-20 : item.valueOffset-30;
        if ((std::min)(1.0f,space/farmui::kLabels[index].width)<20.0f/26.0f) {
            std::cerr << "small label " << index << " space " << space << '\n'; Check(false,"readability");
        }
        Check(item.value.empty() || item.valueOffset+14*(item.value.size()-1)+24<=r.width, "value width");
        Check(std::isfinite(item.opacity) && item.opacity>=0 && item.opacity<=1, "item opacity");
        for (std::size_t j=0; j<i; ++j) {
            const auto& other=view.items[j].rect;
            Check(r.x>=other.x+other.width || other.x>=r.x+r.width || r.y>=other.y+other.height || other.y>=r.y+r.height,
                "overlapping view items");
        }
    }
}
FarmEconomySystem::HarvestRecord Record(int i) {
    FarmEconomySystem::HarvestRecord r{};
    r.id=i+1; r.quantity=1; r.saleProtected=true; r.harvestedDay=(i+1)*10-2;
    r.quality.crop=kFarmStageRulesV1[static_cast<std::size_t>(i)].unlockedCrop;
    r.quality.score=90; r.quality.basePrice=100; r.quality.salePrice=100;
    r.quality.maturity=1; r.quality.waterBalance=.8f; r.quality.terrainFit=.9f;
    r.quality.nutrientBalance=.9f; r.quality.nutrientKnown=true; r.quality.harvestSize={1.6f,true};
    return r;
}
void Run() {
    FarmContestOutcomeSystem system;
    FarmEconomySystem::ContestResults results{};
    for (int day : {1,10,11,20,21,30,31}) {
        system.Reset(); system.Observe(day,results,true,true);
        const auto frame=system.GetFrame();
        Check(frame.missedDay==(day==11 || day==20 ? 10 : day==21 || day==30 ? 20 : 0),"deadline boundary");
        Check(frame.season.finalized==(day==31),"season final boundary");
        if (frame.missedDay) {
            farmui::View view{}; farmui::BuildContestMissedView(view,frame); CheckView(view);
            Check(!system.AcknowledgeMissed(frame.missedDay+1),"stale acknowledgment");
            Check(system.AcknowledgeMissed(frame.missedDay),"acknowledgment");
            Check(!system.AcknowledgeMissed(frame.missedDay),"double acknowledgment");
            system.Observe(day,results,true,true); Check(!system.GetFrame().missedDay,"repeat notice");
        }
    }
    system.Reset(); system.Observe(11,results,true,true);
    system.Update(std::numeric_limits<float>::quiet_NaN()); system.Update(-1);
    Check(system.GetFrame().reveal==0,"bad time");
    float previous=0;
    for (int i=0;i<120;++i) {
        system.Update(1.0f/60); const float value=system.GetFrame().reveal;
        Check(value>=previous && value<=1,"monotonic reveal"); previous=value;
        system.Observe(11,results,true,true);
    }
    Check(previous==1,"reveal finish");
    system.Observe(11,results,false,true); Check(!system.GetFrame().missedDay && !system.GetFrame().season.valid,"free mode");
    system.Observe(21,results,true,true); Check(system.GetFrame().missedDay==20,"latest deadline only");
    system.AcknowledgeMissed(20); system.Observe(1,results,true,true); system.Observe(11,results,true,true);
    Check(system.GetFrame().missedDay==10,"rewind resets seen state");
    // The displayed action is captured before time advances, including the final reveal frame.
    for (int skipFrame = 0; skipFrame < 120; ++skipFrame) {
        system.Reset(); system.Observe(31,results,true,true);
        Check(system.ConsumeOpening() && !system.ConsumeOpening(),"opening cue is once-only");
        system.Update(std::numeric_limits<float>::infinity());
        system.Update(std::numeric_limits<float>::quiet_NaN()); system.Update(-1);
        Check(system.GetFrame().reveal==0,"season invalid dt");
        float previousRows[3]{};
        for (int tick = 0; tick <= skipFrame; ++tick) {
            const auto before = system.GetFrame();
            farmui::View view{}; farmui::BuildContestOutcomeView(view,before,results,false,false); CheckView(view);
            const bool ready = before.controlsReveal>=1;
            const auto hit = view.Hit({900,624});
            Check(hit.action==(ready ? farmui::Action::SeasonRetry : farmui::Action::ContestOutcomeReveal),"skip/retry snapshot");
            Check(ready || view.Hit({500,624}).action==farmui::Action::None,"hidden title action");
            for (std::size_t i=0;i<before.rowReveal.size();++i) {
                Check(before.rowReveal[i]>=previousRows[i] && before.rowReveal[i]<=1,"row monotonic");
                if (i) Check(before.rowReveal[i]<=before.rowReveal[i-1],"row order");
                previousRows[i]=before.rowReveal[i];
            }
            for (std::size_t i=0;i<view.count;++i)
                if (view.items[i].enabled && view.items[i].request.action!=farmui::Action::None)
                    Check(ready || view.items[i].request.action==farmui::Action::ContestOutcomeReveal,"hidden actionable control");
            const int focused=view.NextActionable(-1,1);
            Check(focused>=0 && (ready || view.items[focused].request.action==farmui::Action::ContestOutcomeReveal),"focus gate");
            system.Update(1.0f/60);
            system.Observe(31,results,true,true);
            Check(!system.ConsumeOpening(),"observe replayed cue");
        }
        system.Reveal(); system.Reveal();
        Check(system.GetFrame().controlsReveal==1 && system.GetFrame().summaryReveal==1,"season skip completes");
        for (float row : system.GetFrame().rowReveal) Check(row==1,"skip rows");
        system.Observe(31,results,true,true);
        Check(system.GetFrame().controlsReveal==1 && !system.ConsumeOpening(),"detail return replayed ending");
        system.Reset(); Check(!system.ConsumeOpening(),"reset cue");
    }
    int combinations=0;
    // Submitted score combinations, missing entries and legacy rules never share a false verdict.
    for (int mask=0;mask<8;++mask) for (int points : {0,64,65,74,75,84,85,100}) for (bool staged : {false,true}) {
        results={};
        for (int i=0;i<3;++i) if ((mask & (1<<i))!=0) {
            auto& r=results[static_cast<std::size_t>(i)]; r.harvest=Record(i); r.contestDay=(i+1)*10;
            r.qualityPoints=(std::min)(60,points); r.sizePoints=points-r.qualityPoints;
        }
        system.Reset(); system.Observe(31,results,true,staged);
        const auto frame=system.GetFrame();
        Check(frame.season.finalized && !frame.missedDay,"final replaces missed notices");
        Check((frame.season.rating==FarmContestRating::Unrated)==(mask==0),"all missed is unrated");
        if (!staged) Check(frame.goals.back().status==FarmContestGoalStatus::Unknown,"legacy verdict");
        for (bool confirm : {false,true}) for (bool failed : {false,true}) {
            farmui::View view{}; farmui::BuildContestOutcomeView(view,frame,results,confirm,failed); CheckView(view);
            Check(view.Hit({80,600}).action==farmui::Action::None || !confirm,"confirmation exposes no farm controls");
        }
        system.Update(1000);
        Check(system.GetFrame().controlsReveal<1,"unbounded dt skipped ending");
        for (int tick=0;tick<120;++tick) system.Update(1.0f/60);
        Check(system.GetFrame().controlsReveal==1,"season finish");
        farmui::View complete{}; farmui::BuildContestOutcomeView(complete,system.GetFrame(),results,false,false); CheckView(complete);
        Check(complete.Hit({900,624}).action==farmui::Action::SeasonRetry,"retry missing after timeline");
        for (int i=0;i<3;++i) Check((complete.Hit({1000,230+86.0f*i}).action==farmui::Action::ContestDetail)==((mask&(1<<i))!=0),"detail availability");
        ++combinations;
    }
    results[0].contestDay=9; system.Observe(31,results,true,true);
    Check(!system.GetFrame().season.valid,"invalid summary");

    const auto root=std::filesystem::path("generated/codex_checks") / ("contest_outcome_"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    farm::FarmGrid grid; Check(grid.Initialize(5,4),"grid");
    FarmEconomySystem economy; economy.Initialize();
    FarmCropSelectionSystem selection; selection.Initialize();
    FarmDateSystem date; date.Initialize();
    FarmProgressionSystem progression; progression.Initialize({},FarmProgressionMode::ContestSeason);
    FarmDocumentSystem docs; Check(docs.Initialize((root/"documents").string(),grid,economy,selection,&date,&progression),"documents");
    Check(!docs.ArchiveCompletedSeason(grid,economy,selection),"premature archive");
    Check(date.RestoreSnapshot({31,0,1}),"final day");
    Check(docs.SaveAs("Original result",grid,economy,selection),"original");
    const auto originalId=docs.GetActiveDocumentId(); const auto originalPath=docs.GetPath();
    nlohmann::json original; Check(JsonFile::Load(originalPath,original),"read original");
    Check(docs.ArchiveCompletedSeason(grid,economy,selection),"archive");
    const auto archiveId=docs.GetActiveDocumentId(); const auto archivePath=docs.GetPath();
    Check(archiveId!=originalId && date.GetDay()==31,"archive identity/state");
    nlohmann::json archive; Check(JsonFile::Load(archivePath,archive) && archive["date"]["day"]==31,"archive persisted");
    Check(docs.ArchiveCompletedSeason(grid,economy,selection) && docs.GetActiveDocumentId()!=archiveId,"unique repeated archive");
    Check(docs.Reset(grid,economy,selection),"new farm"); docs.BeginUnsavedSession();
    Check(docs.GetActiveDocumentId().empty() && docs.IsDirty() && !docs.FileExists(),"detached identity");
    Check(!docs.Save(grid,economy,selection),"unnamed save overwrote archive");
    Check(docs.SaveAs("New season",grid,economy,selection),"new save");
    nlohmann::json checkJson;
    Check(JsonFile::Load(originalPath,checkJson) && checkJson==original,"original changed");
    Check(JsonFile::Load(archivePath,checkJson) && checkJson==archive,"archive changed");
    Check(docs.Load(archiveId,grid,economy,selection) && date.GetDay()==31,"archive reload");
    Check(progression.IsContestSeason() && progression.StagePolicy().enabled,"archive mode");

    FarmDocumentSystem blocked;
    std::filesystem::create_directories(root/"blocked");
    { std::ofstream obstruction(root/"blocked/saves"); Check(obstruction.good(),"failure fixture"); obstruction << "not a directory"; }
    Check(!blocked.Initialize((root/"blocked").string(),grid,economy,selection,&date,&progression),"blocked init accepted");
    Check(date.RestoreSnapshot({31,0,1}),"blocked day");
    const auto generation=grid.GetGeneration(); const auto money=economy.GetMoney();
    Check(!blocked.ArchiveCompletedSeason(grid,economy,selection),"IO failure ignored");
    Check(grid.GetGeneration()==generation && date.GetDay()==31 && economy.GetMoney()==money,"IO failure mutated farm");

    // Separate native QA fixtures; synthetic data is not cultivation/balance evidence.
    for (int day : {11,31}) {
        FarmDocumentSystem fixture;
        Check(fixture.Initialize((root/("native_"+std::to_string(day))).string(),grid,economy,selection,&date,&progression),"fixture init");
        Check(date.RestoreSnapshot({day,0,1}),"fixture day");
        Check(fixture.SaveAs("Outcome QA",grid,economy,selection),"fixture save");
    }
    std::cout << "PASS: " << combinations << " outcome combinations, 120 skip positions, view layout/input gates, deadlines, ordered reveal/cues, archive isolation/failure/reload\nQA fixtures: " << root.string() << '\n';
}
}
int main() { try { Run(); } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; } }
