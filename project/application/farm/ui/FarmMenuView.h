#pragma once
#include "farm/ui/FarmRuntimeUI.h"

namespace farmui {
inline constexpr int kMenuPageCount = 5;
inline constexpr int kMenuSettingsPage = 4;
struct MenuOverviewState {
    int page = 0;
    bool unlocked = false, canSave = false, canSaveCopy = false, dirty = false;
    int documentIndex = 0, documents = 0;
    std::array<std::string, 2> recordNames{};
    int money = 0, seeds = 0, maximumHeight = 0, suppliedCanals = 0, suppliedTiles = 0;
    bool comparing = false, canCompare = false, following = false;
    int compareA = -1, compareB = -1;
    Label compareIssue = Label::Ready;
    bool contestSeason = false, freeFarming = false;
};
namespace MenuLayout {
inline constexpr Rect details{166,628,224,44};
inline constexpr Rect status{166,676,964,32};
inline constexpr Rect returnTitle{678,628,224,44};
inline constexpr Rect resume{916,628,214,44};
[[nodiscard]] constexpr Rect Cell(int column, float y) noexcept {
    return {176.0f+470*column,y,450,48};
}
}
inline void BuildMenuShell(View& view, int page, bool detailed, bool pending, bool outcome, Label status) {
    view.modal = view.menuShell = true;
    constexpr std::array tabs{Label::MenuFarm,Label::MenuTerrain,Label::MenuRecords,Label::MenuObserve,Label::MenuSettings};
    const int category = page >= 0 && page < kMenuPageCount ? page : 0;
    for (int i=0; i<kMenuPageCount; ++i)
        view.Add(tabs[i], {150.0f+i*196,52,188,48}, {Action::Tab,i}, !pending, category==i);
    view.Add(detailed || page >= kMenuPageCount ? Label::MenuSummary : Label::MenuDetails,
        MenuLayout::details, {Action::MenuDetails}, !pending);
    view.Add(outcome ? Label::OutcomeBack : Label::ReturnTitle, MenuLayout::returnTitle,
        {outcome ? Action::ContestOutcome : Action::ReturnTitle}, !pending);
    view.Add(Label::Resume, MenuLayout::resume, {Action::Close}, !pending);
    if (status != Label::Ready) view.Add(status, MenuLayout::status);
}
inline void BuildMenuOverview(View& view, const MenuOverviewState& s) {
    const auto button = [&](Label label, Action action, int column, float y, bool enabled=true, int argument=0, bool selected=false) {
        view.Add(label,MenuLayout::Cell(column,y),{action,argument},enabled,selected);
    };
    if (s.page == 0) {
        view.Add(Label::MenuWork,{176,130,920,38});
        button(Label::ShopTitle,Action::OpenSeedShop,0,188,s.unlocked);
        button(Label::HarvestDisplay,Action::HarvestDisplay,1,188);
        button(Label::SoilCare,Action::SoilCare,0,258);
        button(Label::QualityOpen,Action::Quality,1,258);
        button(Label::TradeOpen,Action::HarvestInventory,0,328);
        button(Label::StageGuide,Action::StageGuide,1,328);
        view.Metric(Label::ShopMoney,{176,426,450,38},220,std::to_string(s.money)+"G");
        view.Metric(Label::MenuSeedCount,{646,426,450,38},260,std::to_string(s.seeds));
        button(Label::Development,Action::Development,0,500);
        button(Label::CropBook,Action::CropBook,1,500);
        button(Label::Recovery,Action::Recovery,0,564);
    } else if (s.page == 1) {
        view.Add(Label::MenuTerrainWork,{176,130,920,38});
        button(Label::Overview,Action::TerrainField,0,188,s.unlocked);
        button(Label::StageGuide,Action::StageGuide,1,188);
        view.Metric(Label::StageHeight,{176,298,920,38},540,"H"+std::to_string(s.maximumHeight));
        view.Metric(Label::WaterStats,{176,358,920,38},540,
            std::to_string(s.suppliedCanals)+" / "+std::to_string(s.suppliedTiles));
        button(Label::Development,Action::Development,0,438);
        button(Label::Repellent,Action::Repellent,1,438);
    } else if (s.page == 2) {
        view.Add(Label::MenuSaveHeading,{176,130,920,38});
        button(Label::Save,Action::Save,0,180,s.canSave);
        button(Label::SaveCopy,Action::SaveCopy,1,180,s.canSaveCopy);
        view.Add(Label::MenuLoadHeading,{176,256,920,38});
        view.Add(Label::Previous,{176,308,212,48},{Action::Previous},s.documentIndex>0);
        view.Add(Label::Next,{408,308,218,48},{Action::Next},s.documentIndex+1<s.documents);
        button(Label::Load,Action::Load,1,308,s.documents>0);
        view.Metric(Label::Record,{176,374,920,38},540,
            std::to_string(s.documents ? s.documentIndex+1 : 0)+" / "+std::to_string(s.documents));
        view.recordNames = s.recordNames;
        view.Add(s.documents==0 ? Label::NoSaves : s.dirty ? Label::Dirty : Label::Clean,{176,554,920,38});
    } else if (s.page == 3) {
        view.Add(Label::MenuCompareHeading,{176,130,920,38});
        button(Label::ObserveField,Action::ObserveField,0,188,s.unlocked);
        button(s.comparing ? Label::Stop : Label::Start,s.comparing ? Action::Stop : Action::Start,
            1,188,s.comparing || s.canCompare);
        view.Metric(Label::MenuCompareA,{176,298,450,38},300,s.compareA>=0 ? std::to_string(s.compareA) : "--");
        view.Metric(Label::MenuCompareB,{646,298,450,38},300,s.compareB>=0 ? std::to_string(s.compareB) : "--");
        view.Add(s.comparing ? Label::Running : Label::NotRunning,{176,378,920,38});
        if (s.compareIssue != Label::Ready) view.Add(s.compareIssue,{176,450,920,38});
    } else if (s.page == kMenuSettingsPage) {
        view.Add(Label::MenuSoundHeading,{176,130,920,38});
        view.Add(Label::MenuAudioOpen,{176,176,920,48},{Action::AudioSettings});
        view.Add(Label::MenuCameraHeading,{176,274,920,38});
        button(Label::Follow,Action::Follow,0,320,true,0,s.following);
        button(Label::Overview,Action::Overview,1,320,true,0,!s.following);
        view.Add(Label::MenuModeHeading,{176,418,920,38});
        constexpr std::array labels{Label::MenuModeSeason,Label::MenuModeTrial,Label::MenuModeFree};
        constexpr std::array arguments{1,0,2};
        const std::array selected{s.contestSeason,!s.contestSeason&&!s.freeFarming,s.freeFarming};
        for (int i=0;i<3;++i)
            view.Add(labels[i],{176.0f+i*314,464,292,48},{Action::ChangePlayMode,arguments[i]},true,selected[i]);
    }
}
inline void BuildMenuAudioView(View& view, const title::AudioSettingsFrame& frame) {
    view.modal = view.audioSettings = true;
    view.audioFrame = frame;
    view.Add(Label::MenuAudioTitle,{356,230,568,40});
    constexpr std::array labels{Label::MenuEnvironment,Label::MenuWaterAudio,Label::MenuContestAudio};
    for (std::size_t i=0;i<labels.size();++i)
        view.Add(labels[i],{356,title::kAudioSliderY[i]-20,176,40});
    const auto& reset=title::kAudioSettingsReset;
    const auto& back=title::kAudioSettingsBack;
    view.Add(Label::MenuAudioReset,{reset.position.x,reset.position.y,reset.size.x,reset.size.y});
    view.Add(Label::MenuAudioBack,{back.position.x,back.position.y,back.size.x,back.size.y});
    if (frame.saveFailed) view.Add(Label::MenuAudioFailed,{356,656,568,38});
}
}
