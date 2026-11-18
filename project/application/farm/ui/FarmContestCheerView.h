#pragma once
#include "farm/ui/FarmRuntimeUI.h"

namespace farmui {
namespace contestcheer {
inline constexpr Rect kLeftBody{32,263,336,94}, kRightBody{912,263,336,94};
inline constexpr float kSlidePixels = 12;
inline constexpr float kLineHeight = 38, kTextInsetX = 12, kTextInsetY = 9, kTailWidth = 16;
inline constexpr std::array<std::array<Label,4>, static_cast<std::size_t>(FarmContestCheerSystem::Pair::Count)> kLines{{
        {Label::CheerPerfectL0,Label::CheerPerfectL1,Label::CheerPerfectR0,Label::CheerPerfectR1},
        {Label::CheerPumpkinPotL0,Label::CheerPumpkinPotL1,Label::CheerPumpkinPotR0,Label::CheerPumpkinPotR1},
        {Label::CheerPumpkinFestivalL0,Label::CheerPumpkinFestivalL1,Label::CheerPumpkinFestivalR0,Label::CheerPumpkinFestivalR1},
        {Label::CheerCarrotCurryL0,Label::CheerCarrotCurryL1,Label::CheerCarrotCurryR0,Label::CheerCarrotCurryR1},
        {Label::CheerCarrotBundleL0,Label::CheerCarrotBundleL1,Label::CheerCarrotBundleR0,Label::CheerCarrotBundleR1},
        {Label::CheerTomatoCenterL0,Label::CheerTomatoCenterL1,Label::CheerTomatoCenterR0,Label::CheerTomatoCenterR1},
        {Label::CheerTomatoLunchL0,Label::CheerTomatoLunchL1,Label::CheerTomatoLunchR0,Label::CheerTomatoLunchR1},
        {Label::CheerTurnipStarL0,Label::CheerTurnipStarL1,Label::CheerTurnipStarR0,Label::CheerTurnipStarR1},
        {Label::CheerLargeDeliveryL0,Label::CheerLargeDeliveryL1,Label::CheerLargeDeliveryR0,Label::CheerLargeDeliveryR1},
        {Label::CheerLargeShopL0,Label::CheerLargeShopL1,Label::CheerLargeShopR0,Label::CheerLargeShopR1},
        {Label::CheerQualityMuseumL0,Label::CheerQualityMuseumL1,Label::CheerQualityMuseumR0,Label::CheerQualityMuseumR1},
        {Label::CheerQualityKnifeL0,Label::CheerQualityKnifeL1,Label::CheerQualityKnifeR0,Label::CheerQualityKnifeR1},
        {Label::CheerNutrientSoilL0,Label::CheerNutrientSoilL1,Label::CheerNutrientSoilR0,Label::CheerNutrientSoilR1},
        {Label::CheerWaterCareL0,Label::CheerWaterCareL1,Label::CheerWaterCareR0,Label::CheerWaterCareR1},
        {Label::CheerPortraitL0,Label::CheerPortraitL1,Label::CheerPortraitR0,Label::CheerPortraitR1},
        {Label::CheerGuestsL0,Label::CheerGuestsL1,Label::CheerGuestsR0,Label::CheerGuestsR1}
}};
inline void Build(View& view, const FarmContestCheerSystem::Frame& frame) {
    using Side = FarmContestCheerSystem::Side;
    const int pair = static_cast<int>(frame.pair);
    if (pair < 0 || pair >= static_cast<int>(kLines.size()) ||
        (frame.side != Side::Left && frame.side != Side::Right) ||
        !std::isfinite(frame.opacity) || frame.opacity <= 0 || !std::isfinite(frame.entrance)) return;
    const bool left = frame.side == Side::Left;
    auto body = left ? kLeftBody : kRightBody;
    const float offset = kSlidePixels * (1 - std::clamp(frame.entrance,0.f,1.f));
    body.x += left ? -offset : offset;
    view.cheerBubble = {true, {body.x-(left?kTailWidth:0.f),body.y,body.width+kTailWidth,body.height},
        left, std::clamp(frame.opacity,0.f,1.f)};
    const auto first = view.count;
    for (int line=0;line<2;++line)
        view.Add(kLines[pair][(left?0:2)+line],
            {body.x+kTextInsetX,body.y+kTextInsetY+line*kLineHeight,body.width-2*kTextInsetX,kLineHeight});
    for (std::size_t i=first;i<view.count;++i) {
        view.items[i].opacity = view.cheerBubble.opacity;
        view.items[i].darkInk = true;
    }
}
}
}
