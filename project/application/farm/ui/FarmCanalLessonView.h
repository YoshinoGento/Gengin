#pragma once
#include "farm/system/FarmCanalLessonSystem.h"
#include "farm/ui/FarmRuntimeUI.h"

namespace farmui {
inline void BuildLiveGapView(View& view, bool missed) {
    view.briefing = true;
    view.Add(Label::GapTitle,{40,25,720,48});
    view.Add(Label::GapSource,{40,516,1160,40});
    view.Add(missed ? Label::GapMiss : Label::GapHint,{40,563,1160,40});
    view.Add(Label::GapDig,{40,628,460,56},{Action::GapDig},true,true);
    view.Add(Label::GapSkip,{888,636,352,44},{Action::GapSkip});
}
inline void BuildCanalLessonView(View& view, const farm::FarmCanalLessonSystem::Frame& frame) {
    using P = farm::FarmCanalLessonSystem::Phase;
    const P p = frame.phase;
    view.terrain = true;
    view.Add(Label::LessonTitle,{36,38,582,52});
    const auto percent = [](float m){ return std::to_string(static_cast<int>(m*100.0f+0.5f)) + "%"; };
    std::string moisture = percent(frame.moisture);
    if (p == P::Flowing || p == P::Reached) moisture = percent(frame.before) + " > " + moisture;
    view.Metric(Label::LessonMoisture,{664,38,570,52},225,moisture);
    const Label hint = p == P::Reached ? Label::LessonReached : p == P::Flowing ? Label::LessonFlow :
        p == P::Preview ? (frame.reaches ? Label::LessonYes : Label::LessonNo) :
        frame.rejected ? Label::LessonMiss : frame.first ? Label::LessonChoose : Label::LessonSecond;
    view.Add(hint,{36,538,1176,50});
    constexpr Rect primary{36,622,352,56}, secondary{412,622,352,56}, leave{872,622,360,56};
    if (p == P::Choose) view.Add(Label::LessonPreview,primary,{Action::LessonPreview});
    if (p == P::Preview) {
        view.Add(Label::LessonConnect,primary,{Action::LessonConnect},frame.reaches);
        view.Add(Label::LessonUndo,secondary,{Action::LessonUndo});
    }
    if (p == P::Reached && frame.first)
        view.Add(Label::LessonAgain,primary,{Action::LessonAgain});
    view.Add(Label::LessonExit,leave,{Action::LessonExit});
}
}
