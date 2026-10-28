#pragma once
#include "farm/ui/FarmQualityView.h"

namespace farmui {
namespace contestdetail {
using Page = FarmContestPresentationSystem::Page;
inline constexpr std::array<Rect,3> kTabs{{{48,100,160,48},{220,100,160,48},{392,100,160,48}}};
inline constexpr Rect kExplanation{988,100,244,48}, kBack{888,654,344,46};

inline Label Hint(FarmQualityFocus focus) noexcept {
    switch (focus) {
    case FarmQualityFocus::Maturity: return Label::ContestHintMaturity;
    case FarmQualityFocus::Water: return Label::ContestHintWater;
    case FarmQualityFocus::Terrain: return Label::ContestHintTerrain;
    case FarmQualityFocus::Nutrients: return Label::ContestHintNutrients;
    case FarmQualityFocus::Balanced: return Label::ContestHintBalanced;
    default: return Label::ContestHintUnknown;
    }
}
inline void Advice(View& view, const FarmQualityAdvice& advice, float y) {
    Label label = Label::ContestHintTitle;
    switch (advice.focus) {
    case FarmQualityFocus::Maturity: label = Label::ContestReviewMaturity; break;
    case FarmQualityFocus::Water: label = Label::ContestReviewWater; break;
    case FarmQualityFocus::Terrain: label = Label::ContestReviewTerrain; break;
    case FarmQualityFocus::Nutrients: label = Label::ContestReviewNutrients; break;
    default: break;
    }
    if (label != Label::ContestHintTitle && advice.lowestPercent >= 0)
        view.Metric(label, {680,y,552,38}, 350, std::to_string(advice.lowestPercent)+"%");
    else view.Add(Label::ContestHintTitle, {680,y,552,38});
    view.Add(Hint(advice.focus), {680,y+48,552,38});
}
inline void Summary(View& view, const FarmContestPresentationSystem::Frame& frame) {
    const auto& data = frame.comparison;
    view.Add(CropLabel(data.quality.crop), {80,560,480,38});
    const auto first = view.count;
    if (data.staged && !data.passed)
        view.Metric(Label::ContestGapShort, {680,316,552,44}, 260, std::to_string(data.shortfall)+" pt");
    else view.Add(data.staged ? Label::ContestPassedShort : Label::ContestLegacyShort, {680,316,552,44});
    Advice(view,data.qualityAdvice,390);
    if (data.sizeAdvice == FarmContestSizeAdvice::ImproveQuality)
        view.Add(Label::ContestQualityNeeded, {680,496,552,38});
    else if (data.sizeAdvice == FarmContestSizeAdvice::MoreSize)
        view.Metric(Label::ContestMoreShort, {680,496,552,38}, 350, "+"+std::to_string(data.additionalGrams)+" g");
    if (data.qualityAdvice.partial) view.Add(Label::ContestPartialShort, {680,548,552,38});
    for (std::size_t i=first; i<view.count; ++i) view.items[i].opacity = frame.adviceReveal;
}
inline void Quality(View& view, const FarmContestComparison& data) {
    const auto& q = data.quality;
    view.radar.visible = true; view.radar.numberedAxes = false;
    view.radar.offset = {-46,68}; view.radar.scale = 1.1f;
    const std::array<float,4> values{{q.maturity,q.waterBalance,q.terrainFit,q.nutrientBalance}};
    constexpr std::array<Label,4> names{{Label::ContestAxisMaturity,Label::ContestAxisWater,
        Label::ContestAxisTerrain,Label::ContestAxisNutrients}};
    constexpr std::array<Rect,4> axes{{{232,232,200,38},{446,362,200,38},{232,496,200,38},{24,362,188,38}}};
    for (std::size_t i=0; i<values.size(); ++i) {
        const bool known = std::isfinite(values[i]) && (i!=3 || q.nutrientKnown);
        view.radar.known[i] = known;
        view.radar.values[i] = known ? std::clamp(values[i],0.f,1.f) : 0;
        view.Metric(names[i], axes[i], 86, known ?
            std::to_string(static_cast<int>(std::lround(view.radar.values[i]*100)))+"%" : "--");
    }
    view.Add(Label::ContestRadarScaleShort, {220,548,240,32});
    view.Metric(Label::ContestQualityScoreShort, {680,260,552,42}, 300, std::to_string(data.qualityPoints)+" / 60");
    Advice(view,data.qualityAdvice,332);
    view.Add(Label::ContestAxesShort, {680,440,552,38});
    if (data.qualityAdvice.partial) view.Add(Label::ContestPartialShort, {680,496,552,38});
}
inline void Size(View& view, const FarmContestComparison& data) {
    view.Add(Label::ContestActual, {80,248,500,38});
    view.Add(CropLabel(data.quality.crop), {80,292,500,38});
    view.Add(data.referenceAvailable ? Label::ContestExampleShort : Label::ContestNoReference, {720,248,512,38});
    if (data.referenceAvailable) view.Add(Label::ContestExampleCondition, {720,292,512,38});
    view.Metric(Label::ContestWeightShort, {80,500,500,38}, 220, std::to_string(data.grams)+" g");
    view.Metric(Label::ContestWeightShort, {720,500,512,38}, 220,
        data.referenceAvailable ? std::to_string(data.referenceGrams)+" g" : "--");
    if (data.sizeAdvice == FarmContestSizeAdvice::MoreSize)
        view.Metric(Label::ContestMoreShort, {48,550,720,38}, 400, "+"+std::to_string(data.additionalGrams)+" g");
    else view.Add(data.sizeAdvice == FarmContestSizeAdvice::ImproveQuality ? Label::ContestQualityNeeded :
        data.staged ? Label::ContestPassedShort : Label::ContestLegacyShort, {48,550,720,38});
    view.Metric(Label::ContestSizeScoreShort, {800,550,432,38}, 240, std::to_string(data.sizePoints)+" / 40");
}
}

inline void BuildContestDetailView(View& view, const FarmContestPresentationSystem::Frame& frame) {
    using namespace contestdetail;
    const auto& data = frame.comparison;
    const bool complete = frame.complete;
    view.modal = true; view.contestDetail = true; view.contestFrame = frame;
    view.Add(Label::ContestDetailTitle, {48,20,500,42});
    view.Metric(Label::ContestEventDay, {680,20,220,42}, 128, std::to_string(data.day));
    if (frame.unsubmitted) {
        view.Add(frame.verdictVisible ? Label::ContestMissed : Label::ContestJudging, {680,174,552,52});
        view.Add(Label::OutcomeNoScore, {680,280,552,46});
        view.Add(Label::OutcomeReservation, {680,366,552,40});
        view.Add(Label::OutcomeSubmitHint, {680,422,552,40});
        for (std::size_t i=3;i<view.count;++i) view.items[i].opacity=frame.adviceReveal;
        view.Add(complete ? Label::ContestDetailBack : Label::ContestReveal,kBack,
            {complete ? Action::ContestDetailClose : Action::ContestReveal});
        return;
    }
    view.Metric(Label::ContestTargetShort, {944,20,288,42}, 166, data.staged ? std::to_string(data.target)+" pt" : "--");
    constexpr std::array<Label,3> tabs{{Label::ContestTabResult,Label::ContestTabQuality,Label::ContestTabSize}};
    for (std::size_t i=0; i<tabs.size(); ++i)
        view.Add(tabs[i], kTabs[i], {Action::ContestTab,static_cast<int>(i)}, complete, static_cast<int>(frame.page)==static_cast<int>(i));
    view.Add(Label::ContestExplanation, kExplanation, {Action::ContestExplanation}, complete, frame.explanation);
    if (!complete) for (std::size_t i=3;i<view.count;++i) view.items[i].opacity=0;
    const auto verdict = !frame.verdictVisible ? Label::ContestJudging : !data.staged ? Label::ContestEntered :
        data.passed ? Label::StagePassed : Label::StageFailed;
    const bool summary=frame.page==Page::Result;
    view.Add(verdict, {summary ? 680.f : 48.f,174,540,46});
    view.Metric(Label::ContestScoreShort, {680,summary ? 234.f : 174.f,552,46}, 210,
        frame.totalReveal > 0 ? std::to_string(frame.displayedTotal)+" / 100" : "--");
    switch (frame.page) {
    case Page::Result: Summary(view,frame); break;
    case Page::Quality: Quality(view,data); break;
    case Page::Size: Size(view,data); break;
    }
    if (frame.explanation) {
        view.Add(Label::ContestRuleShort, {48,606,600,32});
        view.Add(Label::ContestWeightRuleShort, {680,606,552,32});
    }
    view.Add(!complete ? Label::ContestReveal : frame.page==Page::Result ? Label::ContestDetailBack : Label::ContestSummaryBack,
        kBack, {!complete ? Action::ContestReveal : frame.page==Page::Result ? Action::ContestDetailClose : Action::ContestTab,0});
}
}
