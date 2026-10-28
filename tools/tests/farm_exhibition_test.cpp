#include "farm/system/FarmDocumentSystem.h"
#include "farm/ui/FarmExhibitionView.h"
#include "io/JsonFile.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <limits>

namespace {
void Check(bool value, const char* message) {
    if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
void Layout(const FarmExhibitionSystem& system) {
    farmui::View view; farmui::BuildExhibitionView(view, system);
    Check(view.modal && view.count < view.items.size(), "bounded modal");
    for (std::size_t i=0; i<view.count; ++i) {
        const auto& item=view.items[i]; const auto& r=item.rect;
        Check(r.x>=0 && r.y>=0 && r.x+r.width<=1280 && r.y+r.height<=720, "screen bounds");
        const float available=item.value.empty()?r.width-20:item.valueOffset-30;
        Check(available/farmui::kLabels[static_cast<std::size_t>(item.label)].width>=20.f/26.f, "readable label");
        Check(item.request.action!=farmui::Action::SeasonRetry && item.request.action!=farmui::Action::Save &&
            item.request.action!=farmui::Action::Restart && item.request.action!=farmui::Action::SubmitContest, "no farm mutation controls");
        for (std::size_t j=i+1; j<view.count; ++j) {
            const auto& b=view.items[j].rect;
            Check(r.x+r.width<=b.x || b.x+b.width<=r.x || r.y+r.height<=b.y || b.y+b.height<=r.y, "no overlapping labels");
        }
    }
}
}
int main() {
    namespace fs=std::filesystem;
    using System=FarmExhibitionSystem;
    using Cue=FarmContestPresentationSystem::Cue;
    const fs::path temp="generated/codex_checks/exhibition_test";
    fs::create_directories(temp);
    const std::string fixture="project/Resources/exhibition";
    const auto absent=(temp/"absent.json").string();
    System system; system.Initialize(fixture, absent);
    Check(!system.Active() && !system.Startup() && system.GetPhase()==System::Phase::Closed,
        "ordinary startup unchanged");
    Check(!system.Available(-1) && !system.Available(3), "bounded cases");
    Check(!system.Begin(3), "closed cannot play");
    system.Open(); Layout(system);
    Check(!system.Select(-1) && !system.Select(3), "invalid selection rejected");
    int frames=0;
    for(int testCase=0; testCase<3; ++testCase) {
        system.Home(); Check(system.Select(testCase), "all cultivated fixtures loaded");
        for(float dt : {1.f/30,1.f/60,1.f/144,.1f}) {
            Check(system.Begin(3), "start or replay");
            Check(system.ConsumeBeginning() && !system.ConsumeBeginning(), "audio begin once");
            Check(!system.Begin(3) && !system.Select(0), "no playback restart/case switch during reveal");
            Check(!system.SelectPage(1), "analysis unavailable during reveal");
            system.Update(std::numeric_limits<float>::quiet_NaN()); system.Update(-1);
            Check(!system.Complete(), "invalid delta ignored");
            int assess=0,verdict=0;
            for(int i=0; i<720; ++i) {
                Layout(system); ++frames; system.Update(dt);
                const auto cue=system.ConsumeCue();
                if(cue==Cue::Assess) ++assess;
                else if(cue!=Cue::None) ++verdict;
            }
            Check(system.Complete() && verdict==1 && assess==(testCase==2?0:1), "once-only complete cues");
            if(testCase<2) {
                const auto comparison=system.ContestFrame().comparison;
                Check(comparison.total==(testCase==0?45:98) && comparison.target==85, "genuine score parity");
                Check(comparison.grams==(testCase==0?1202:3758), "genuine crop parity");
                for(int page=0; page<3; ++page) {
                    Check(system.SelectPage(page), "optional analysis"); Layout(system);
                    system.ToggleExplanation(); Layout(system); system.Back();
                    Check(!system.ContestFrame().explanation && system.Active(), "back hides explanation first");
                }
                Check(!system.SelectPage(-1) && !system.SelectPage(3), "bounded pages");
            } else Check(system.OutcomeFrame().goals.back().status==FarmContestGoalStatus::Missed, "missed not scored as zero");
            system.Home(); Check(system.Selection()==testCase, "home preserves chosen case");
        }
        Check(system.Begin(3), "skip start"); system.Back();
        Check(system.Complete() && system.GetPhase()!=System::Phase::Menu, "skip does not navigate");
        system.Back(); Check(system.GetPhase()==System::Phase::Menu, "second action returns to menu");
    }
    system.Close(); Check(!system.Active(), "close restores ordinary owner");
    const auto startup=(temp/"enabled.json").string();
    {std::ofstream file(startup); file << R"({"enabled":true})";}
    system.Initialize(fixture,startup); Check(system.Active() && system.Startup(), "exhibition startup");
    {std::ofstream file(startup); file << R"({"enabled":false})";}
    system.Initialize(fixture,startup);
    Check(!system.Active() && !system.Startup() && system.GetPhase()==System::Phase::Closed,
        "opt out clears previous demo startup");
    {std::ofstream file(startup); file << "invalid";}
    system.Initialize((temp/"missing").string(),startup);
    Check(system.Active() && !system.Available(0) && !system.Begin(3), "missing/corrupt safe menu"); Layout(system);
    FarmEconomySystem::ContestResults unchanged{}; int day=777;
    Check(!FarmDocumentSystem::ReadExhibitionResults(startup,unchanged,day) && day==777, "failed extraction leaves caller unchanged");
    nlohmann::json record;
    Check(JsonFile::Load(fixture + "/failed.json",record), "read immutable fixture");
    record["economy"]["contestResults"][2]["qualityPoints"] = 60;
    Check(JsonFile::Save(startup,record), "write independent invalid fixture");
    Check(!FarmDocumentSystem::ReadExhibitionResults(startup,unchanged,day) && day==777, "tampered score rejected");
    Check(JsonFile::Load(fixture + "/failed.json",record), "read fixture again");
    record["date"]["day"] = 1;
    Check(JsonFile::Save(startup,record), "write invalid date");
    Check(!FarmDocumentSystem::ReadExhibitionResults(startup,unchanged,day), "future result rejected");
    {std::ofstream file(startup); file << std::string(1024*1024+1,'x');}
    Check(!FarmDocumentSystem::ReadExhibitionResults(startup,unchanged,day), "oversized record rejected");
    std::cout << "PASS exhibition: " << frames << " layouts, three genuine cases, replay/skip/cues, isolation and malformed data\n";
}
