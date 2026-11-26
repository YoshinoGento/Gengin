#pragma once
#include "farm/ui/FarmRuntimeUI.h"

namespace farmui {
enum class GuideDevice { Mouse, Keys, Both, Pad };
namespace GuideLayout {
inline constexpr Rect fieldDetail{24,254,188,44}, fieldHelp{24,306,188,44};
inline constexpr Rect board{96,92,1088,516};
inline constexpr Rect tab(int index) noexcept { return {132,188.f+62*index,216,50}; }
inline constexpr Rect keycap(Rect sign) noexcept { return {sign.x+sign.width-50,sign.y+8,41,sign.height-16}; }
inline constexpr Rect signText(Rect sign, bool field) noexcept {
    return {sign.x+7,sign.y,field ? keycap(sign).x-sign.x-11 : sign.width-14,sign.height};
}
inline constexpr float firstRowY=188, rowStep=64;
}
inline constexpr std::array kGuideTabs{Label::GuideMouse, Label::GuideKeys, Label::GuideBoth, Label::GuidePad};
[[nodiscard]] constexpr bool ValidGuideTab(int index) noexcept {
    return index >= 0 && index < static_cast<int>(kGuideTabs.size());
}
[[nodiscard]] constexpr GuideDevice StepGuideTab(GuideDevice current, int step) noexcept {
    constexpr int count = static_cast<int>(kGuideTabs.size());
    return static_cast<GuideDevice>((static_cast<int>(current) + (step > 0) - (step < 0) + count) % count);
}
inline void BuildFieldGuideEntry(View& view, bool detailed) {
    view.Add(detailed ? Label::GuidePlaqueSimple : Label::GuideDetail, GuideLayout::fieldDetail, {Action::FieldDetails});
    if (!detailed) view.Add(Label::GuidePlaqueHelp, GuideLayout::fieldHelp, {Action::OpenControlGuide});
}
inline void BuildControlGuide(View& view, GuideDevice device) {
    view.modal = view.controlGuide = true;
    view.Add(Label::GuideTitle, {176,113,740,44});
    for (int i=0; i<static_cast<int>(kGuideTabs.size()); ++i)
        view.Add(kGuideTabs[i], GuideLayout::tab(i), {Action::GuideTab,i}, true, i==static_cast<int>(device));
    float y=GuideLayout::firstRowY;
    const auto row=[&](Label action,Label gesture) {
        view.Add(action,{370,y,222,48});
        view.Add(gesture,{598,y,554,48});
        y+=GuideLayout::rowStep;
    };
    switch (device) {
    case GuideDevice::Mouse:
        row(Label::GuideWork, Label::GuideClick); row(Label::GuideEquip, Label::GuideWheel);
        row(Label::GuideBag, Label::GuideRight); row(Label::GuideSeeds, Label::GuideBagWheel); break;
    case GuideDevice::Keys:
        row(Label::GuideAim, Label::GuideArrows); row(Label::GuideEquip, Label::GuideQE);
        row(Label::GuideWork, Label::GuideEnter); row(Label::GuideSeeds, Label::GuideKeySeeds); break;
    case GuideDevice::Both:
        row(Label::GuideWork, Label::GuideMixedWork); row(Label::GuideSeeds, Label::GuideMixedSeeds); break;
    case GuideDevice::Pad:
        row(Label::GuideMenu, Label::GuidePadMenu); row(Label::GuidePadChoose, Label::GuidePadButtons);
        row(Label::GuideFirstWater, Label::GuidePadWater);
        view.Add(Label::GuidePadScope,{370,y,780,48}); break;
    }
    view.Add(Label::GuideFooter, {132,523,734,44});
    view.Add(Label::Close, {974,522,176,46}, {Action::GuideClose});
}
}
