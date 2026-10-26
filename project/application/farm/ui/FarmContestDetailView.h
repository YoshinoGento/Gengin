#pragma once
#include "farm/ui/FarmQualityView.h"

namespace farmui {
inline void BuildContestDetailView(View& view, const FarmContestPresentationSystem::Frame& frame) {
    const auto& data = frame.comparison;
    view.modal = true; view.contestDetail = true; view.contestFrame = frame;
    view.Add(Label::ContestDetailTitle, {48,20,460,42});
    view.Metric(Label::ContestEventDay, {520,20,270,42}, 148, std::to_string(data.day));
    view.Metric(Label::StageThreshold, {850,20,382,42}, 178, data.staged ? std::to_string(data.target)+" pt" : "--");
    view.Add(Label::ContestActual, {64,82,500,38});
    view.Add(data.referenceAvailable ? Label::ContestIdeal : Label::ContestNoReference, {688,82,544,38});
    view.Add(CropLabel(data.quality.crop), {64,134,250,38});
    if (data.referenceAvailable) view.Add(CropLabel(data.quality.crop), {688,134,250,38});
    view.radar.visible = true; view.radar.offset = {104,-48}; view.radar.scale = .78f;
    const auto& quality = data.quality;
    const std::array<float,4> values{{quality.maturity,quality.waterBalance,quality.terrainFit,quality.nutrientBalance}};
    for (std::size_t i=0; i<values.size(); ++i) {
        view.radar.known[i] = std::isfinite(values[i]) && (i!=3 || quality.nutrientKnown);
        view.radar.values[i] = view.radar.known[i] ? std::clamp(values[i],0.0f,1.0f)*frame.reveal : 0;
    }
    view.referenceRadar.visible = data.referenceAvailable;
    view.referenceRadar.offset = {728,-48}; view.referenceRadar.scale = .78f;
    view.referenceRadar.color = {.36f,.83f,.83f,1};
    view.referenceRadar.known.fill(true); view.referenceRadar.values.fill(frame.reveal);
    view.Metric(Label::ContestGameWeight, {64,370,540,38}, 240, std::to_string(data.grams)+" g");
    view.Metric(Label::ContestGameWeight, {688,370,544,38}, 240,
        data.referenceAvailable ? std::to_string(data.referenceGrams)+" g" : "--");
    view.Metric(Label::ContestQualityPoints, {64,414,540,36}, 280, std::to_string(data.qualityPoints)+" / 60");
    view.Metric(Label::ContestSizePoints, {64,456,540,36}, 280, std::to_string(data.sizePoints)+" / 40");
    view.Add(Label::ContestIdealNote, {688,414,544,36});
    view.Add(Label::ContestWeightNote, {688,456,544,36});
    view.Add(Label::ContestRadarLegend, {64,500,1152,32});
    view.Add(!quality.nutrientKnown ? Label::HintPartialRecord : Label::ContestAxesNote, {64,538,1152,28});
    const Label verdict = !frame.verdictVisible ? Label::ContestJudging : !data.staged ? Label::ContestEntered :
        data.passed ? Label::StagePassed : Label::StageFailed;
    view.Metric(verdict, {64,584,540,40}, 280,
        frame.verdictVisible ? std::to_string(data.total)+" / 100" : "--");
    if (frame.verdictVisible) {
        if (!data.staged) view.Add(Label::StageLegacy, {688,584,544,40});
        else if (data.sizeAdvice == FarmContestSizeAdvice::MoreSize)
            view.Metric(Label::ContestMoreGrams, {688,584,544,40}, 350, "+"+std::to_string(data.additionalGrams)+" g");
        else if (data.sizeAdvice == FarmContestSizeAdvice::ImproveQuality)
            view.Add(Label::ContestImproveQuality, {688,584,544,40});
        else view.Add(Label::ContestTargetMet, {688,584,544,40});
        view.Add(QualityHintLabel(data.qualityAdvice.focus, true), {64,626,740,30});
        if (data.staged && !data.passed)
            view.Metric(Label::StageShortfall, {64,662,740,36}, 340, std::to_string(data.shortfall)+" pt");
    }
    view.Add(frame.verdictVisible ? Label::ContestDetailBack : Label::ContestReveal,
        {888,654,344,46}, {frame.verdictVisible ? Action::ContestDetailClose : Action::ContestReveal});
}
}
