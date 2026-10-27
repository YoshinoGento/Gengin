#include "farm/ui/FarmContestDetailView.h"
#include "farm/render/FarmContestStageLayout.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>

namespace {
void Check(bool value, const char* message) {
    if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
FarmEconomySystem::HarvestRecord Record(farm::CropType crop, int quality, float size, int day) {
    FarmEconomySystem::HarvestRecord r;
    r.id=1; r.quantity=1; r.saleProtected=true; r.harvestedDay=day;
    r.quality.crop=crop; r.quality.score=quality; r.quality.basePrice=100; r.quality.salePrice=100;
    r.quality.maturity=1; r.quality.waterBalance=.5f; r.quality.terrainFit=1;
    r.quality.nutrientBalance=.8f; r.quality.nutrientKnown=true; r.quality.harvestSize={size,true};
    return r;
}
FarmEconomySystem::ContestResults Results(FarmEconomySystem::HarvestRecord r, int index) {
    FarmEconomySystem::ContestResults results{};
    const auto judge=FarmContestJudgeSystem::Evaluate(&r);
    Check(judge.IsValid(),"fixture judge");
    results[index]={r,FarmContestEntrySystem::kContestDays[index],judge.qualityPoints,judge.sizePoints};
    return results;
}
void Layout(const farmui::View& view) {
    Check(view.modal && view.contestDetail && view.count < view.items.size(),"bounded detail modal");
    for (std::size_t i=0;i<view.count;++i) {
        const auto& item=view.items[i]; const auto& r=item.rect;
        Check(r.x>=0 && r.y>=0 && r.x+r.width<=1280 && r.y+r.height<=720,"screen bounds");
        const float available=item.value.empty()?r.width-20:item.valueOffset-30;
        const float textWidth=farmui::kLabels[static_cast<std::size_t>(item.label)].width;
        if (available/textWidth<20.f/26.f) std::cerr << "small label " << static_cast<int>(item.label) << '\n';
        Check(available/textWidth>=20.f/26.f,"20px readable labels");
        Check(item.value.empty() || item.valueOffset+14*(item.value.size()-1)+24<=r.width,"numeric text fits");
        for (std::size_t j=i+1;j<view.count;++j) {
            const auto& b=view.items[j].rect;
            Check(r.x+r.width<=b.x || b.x+b.width<=r.x || r.y+r.height<=b.y || b.y+b.height<=r.y,"no text overlap");
        }
    }
}
bool Has(const farmui::View& view, farmui::Label label) {
    for (std::size_t i=0; i<view.count; ++i) if (view.items[i].label==label) return true;
    return false;
}
void Pages(FarmContestPresentationSystem& p) {
    using Page=FarmContestPresentationSystem::Page;
    const auto before=p.GetFrame().comparison;
    Check(!p.SelectPage(-1) && !p.SelectPage(3),"invalid tab rejected");
    for (int page=0; page<3; ++page) {
        Check(p.SelectPage(page),"completed page selectable");
        farmui::View view; farmui::BuildContestDetailView(view,p.GetFrame()); Layout(view);
        Check(view.contestFrame.page==static_cast<Page>(page),"selected tab snapshot");
        Check(view.radar.visible==(page==1),"radar only in quality analysis");
        Check(!view.referenceRadar.visible,"no misleading mandatory perfect radar");
        Check(view.contestFrame.comparison.total==before.total &&
            view.contestFrame.comparison.grams==before.grams,"tabs preserve judged values");
        Check(!Has(view,farmui::Label::ContestRuleShort),"rules hidden by default");
        if (page==0) Check(!Has(view,farmui::Label::ContestWeightShort) && view.count<=17,"compact summary");
        if (page!=2 && before.qualityAdvice.focus==FarmQualityFocus::Water) {
            bool found=false;
            for (std::size_t i=0;i<view.count;++i) if(view.items[i].label==farmui::Label::ContestReviewWater) {
                found=true;
                Check(view.items[i].value==std::to_string(before.qualityAdvice.lowestPercent)+"%","hint evidence matches stored analysis");
            }
            Check(found && Has(view,farmui::Label::ContestHintWater),"measured focus and actionable hint together");
        }
        if (page==1) {
            Check(!view.radar.numberedAxes && Has(view,farmui::Label::ContestAxisWater),"direct axis labels");
            const auto center=Vector2{farmui::QualityRadar::center.x+view.radar.offset.x,
                farmui::QualityRadar::center.y+view.radar.offset.y};
            const float radius=farmui::QualityRadar::radius*view.radar.scale;
            for (std::size_t i=0;i<view.count;++i) {
                const auto& r=view.items[i].rect;
                Check(r.x+r.width<=center.x-radius || r.x>=center.x+radius ||
                    r.y+r.height<=center.y-radius || r.y>=center.y+radius,"radar avoids labels");
            }
            Check(view.radar.known[3]==before.quality.nutrientKnown,"unknown nutrition not invented");
        }
        if (page==2) Check(Has(view,farmui::Label::ContestExampleCondition)==before.referenceAvailable,
            "reference condition remains visible");
        Check(view.Hit({1050,676}).action==(page==0 ? farmui::Action::ContestDetailClose : farmui::Action::ContestTab),
            "analysis footer returns to summary");
        for (int tab=0;tab<3;++tab) {
            const auto rect=farmui::contestdetail::kTabs[tab];
            const auto request=view.Hit({rect.x+10,rect.y+10});
            Check(request.action==farmui::Action::ContestTab && request.argument==tab,"tab hit target");
        }
        int focus=-1;
        for (int i=0;i<5;++i) {
            focus=view.NextActionable(focus,1);
            Check(focus>=0 && view.items[focus].enabled,"tabs help and back are keyboard reachable");
        }
        p.ToggleExplanation(); view={}; farmui::BuildContestDetailView(view,p.GetFrame()); Layout(view);
        Check(Has(view,farmui::Label::ContestRuleShort) && Has(view,farmui::Label::ContestWeightRuleShort),"optional rules visible");
        Check(p.Back() && !p.GetFrame().explanation && p.GetFrame().page==static_cast<Page>(page),"back closes help first");
        if(page!=0) Check(p.Back() && p.GetFrame().page==Page::Result,"back returns to summary");
        Check(!p.Back() && p.Active(),"summary delegates exit without mutating records");
    }
}
void SpecimenFraming() {
    constexpr std::array<const char*,8> meshes{{"crop_turnip","crop_turnip_leaves","crop_carrot","crop_carrot_leaves",
        "crop_tomato","crop_tomato_stems","crop_pumpkin","crop_pumpkin_vines"}};
    std::size_t checked = 0;
    for (int slot=0;slot<farm::kFarmCropTypeCount;++slot) {
        const auto crop=static_cast<farm::CropType>(slot+1);
        Check(farm::ToCropSlot(crop)==slot,"fixture crop slot");
        for (auto page : {FarmContestPresentationSystem::Page::Result,FarmContestPresentationSystem::Page::Size})
        for (int side=0;side<2;++side) {
            if (page==FarmContestPresentationSystem::Page::Result && side==1) continue;
            const auto pose=FarmContestStageLayout::Specimen(crop,2,2,side,page);
            for (int part=0;part<2;++part) {
                std::ifstream file(std::string("project/Resources/farm/")+meshes[slot*2+part]+".obj");
                Check(file.good(),"mesh fixture opens");
                const auto scale=part==0?pose.bodyScale:pose.leafScale;
                const auto position=part==0?pose.position:Vector3{pose.position.x,pose.leafY,0};
                std::string line;
                while (std::getline(file,line)) {
                    std::istringstream fields(line); std::string tag;
                    fields>>tag; if(tag!="v") continue;
                    Vector3 vertex; Check(static_cast<bool>(fields>>vertex.x>>vertex.y>>vertex.z),"mesh vertex parses");
                    // Assimp may mirror X on import. Both orientations must stay inside the model-only region.
                    for (const float mirror : {-1.f,1.f}) {
                        const float vx=vertex.x*scale.x*mirror, vz=vertex.z*scale.z;
                        const float x=position.x+vx*std::cos(FarmContestStageLayout::kYaw)+vz*std::sin(FarmContestStageLayout::kYaw);
                        const float z=position.z-vx*std::sin(FarmContestStageLayout::kYaw)+vz*std::cos(FarmContestStageLayout::kYaw);
                        for (const float reveal : {0.f,.25f,.5f,.75f,1.f}) {
                        const float depth=FarmContestStageLayout::CameraDistance(reveal)+z;
                        Check(depth>0,"specimen in front of camera");
                        const float perspective=FarmContestStageLayout::kCameraDistance/depth/FarmContestStageLayout::UnitsPerPixel();
                        const float screenX=640+x*perspective;
                        const float screenY=360-(position.y+vertex.y*scale.y)*perspective;
                        Check(screenX>=160+640*side && screenX<=480+640*side,"specimen stays in model-only region");
                        const float top=page==FarmContestPresentationSystem::Page::Result?300.f:334.f;
                        if (screenY<top || screenY>498) std::cerr << "crop=" << slot << " page=" << static_cast<int>(page)
                            << " part=" << part << " x=" << screenX << " y=" << screenY << '\n';
                        Check(screenY>=top && screenY<=498,"specimen avoids name and weight");
                        ++checked;
                        }
                    }
                }
            }
            const auto smallerPose=FarmContestStageLayout::Specimen(crop,.5f,2,side,page);
            Check(std::abs(pose.bodyScale.y/smallerPose.bodyScale.y-std::cbrt(4.f))<.0001f,"common volume scale");
        }
    }
    Check(checked>1000,"actual geometry checked");
    std::cout << "PASS: " << checked << " projected mesh vertices and shared scale\n";
}
}

int main() {
    SpecimenFraming();
    int cases=0;
    for (const auto crop : {farm::CropType::Carrot,farm::CropType::Tomato,farm::CropType::Pumpkin}) {
        const float maximum=crop==farm::CropType::Pumpkin?3.f:2.f;
        for (int index=0;index<3;++index) for (int quality=0;quality<=100;++quality) {
            for (const float size : {.5f,.723f,1.f,1.527f,2.f}) {
                const auto r=Record(crop,quality,size,index*10+1);
                const auto results=Results(r,index);
                const auto model=FarmContestComparisonSystem::Build(index,30,results,true,maximum);
                Check(model.valid && model.referenceAvailable,"valid comparison and example");
                Check(model.target==kFarmStageRulesV1[index].passingPoints,"stage targets");
                Check(model.passed==(model.total>=model.target),"verdict parity");
                const int needed=FarmContestComparisonSystem::MinimumGrams(r,model.target,maximum);
                if (needed) {
                    auto candidate=r;
                    candidate.quality.harvestSize={static_cast<float>(needed)/static_cast<float>(FarmContestComparisonSystem::BaseGrams(crop)),true};
                    Check(FarmContestJudgeSystem::Evaluate(&candidate).totalPoints>=model.target,"integer target passes");
                    candidate.quality.harvestSize.multiplier=static_cast<float>(needed-1)/static_cast<float>(FarmContestComparisonSystem::BaseGrams(crop));
                    Check(FarmContestJudgeSystem::Evaluate(&candidate).totalPoints<model.target,"previous gram does not pass");
                }
                if (model.sizeAdvice==FarmContestSizeAdvice::MoreSize)
                    Check(needed>model.grams && model.additionalGrams==needed-model.grams,"conditional gram advice");
                if (!needed && !model.passed) Check(model.sizeAdvice==FarmContestSizeAdvice::ImproveQuality,"quality-only advice");
                FarmContestPresentationSystem presentation;
                presentation.Begin(model,false);
                Check(presentation.Active() && !presentation.GetFrame().verdictVisible,"begin reveal");
                presentation.Update(std::numeric_limits<float>::quiet_NaN());
                presentation.Update(-1);
                Check(presentation.GetFrame().specimenReveal==0,"invalid dt ignored");
                farmui::View view; farmui::BuildContestDetailView(view,presentation.GetFrame()); Layout(view);
                Check(!presentation.SelectPage(1),"cannot hide ongoing judgement with a tab");
                presentation.ToggleExplanation(); Check(!presentation.GetFrame().explanation,"rules gated during reveal");
                Check(view.Hit({250,120}).action==farmui::Action::None,"tab not clickable during reveal");
                Check(view.Hit({1050,676}).action==farmui::Action::ContestReveal,"skip revealed through UI");
                for (int step=0;step<180;++step) presentation.Update(1.f/60);
                Check(presentation.GetFrame().verdictVisible,"time reveals once");
                view={}; farmui::BuildContestDetailView(view,presentation.GetFrame()); Layout(view);
                Check(view.Hit({1050,676}).action==farmui::Action::ContestDetailClose,"result back action");
                (void)presentation.ConsumeCue();
                Pages(presentation);
                Check(presentation.ConsumeCue()==FarmContestPresentationSystem::Cue::None,"navigation does not replay audio");
                presentation.Close(); Check(!presentation.Active(),"close detail");
                presentation.Begin(model,true); Check(presentation.GetFrame().verdictVisible,"replay immediately readable");
                ++cases;
            }
        }
    }
    auto r=Record(farm::CropType::Carrot,80,1.f,1);
    auto results=Results(r,0);
    Check(!FarmContestComparisonSystem::Build(-1,30,results,true,2).valid,"negative index");
    Check(!FarmContestComparisonSystem::Build(3,30,results,true,2).valid,"OOB index");
    Check(!FarmContestComparisonSystem::Build(0,9,results,true,2).valid,"future contest");
    auto legacy=FarmContestComparisonSystem::Build(0,10,results,false,2);
    Check(legacy.valid && !legacy.staged && !legacy.referenceAvailable && legacy.target==0,"legacy not relabeled failure");
    FarmContestPresentationSystem p; p.Begin(legacy,true);
    Check(p.ConsumeCue()==FarmContestPresentationSystem::Cue::None,"replay silent");
    farmui::View v; farmui::BuildContestDetailView(v,p.GetFrame()); Layout(v);
    Pages(p);
    results[0].harvest.quality.nutrientKnown=false;
    auto partial=FarmContestComparisonSystem::Build(0,10,results,true,2);
    p.Begin(partial,true); Check(p.SelectPage(1),"partial quality page"); v={}; farmui::BuildContestDetailView(v,p.GetFrame());
    Check(!v.radar.known[3],"unknown nutrition leaves gap"); Layout(v);
    Check(Has(v,farmui::Label::ContestPartialShort),"partial record disclosed");
    Pages(p);
    results[0].qualityPoints++;
    Check(!FarmContestComparisonSystem::Build(0,10,results,true,2).valid,"stored score mismatch");
    results=Results(r,0); results[0].harvest.quality.waterBalance=std::numeric_limits<float>::quiet_NaN();
    Check(!FarmContestComparisonSystem::Build(0,10,results,true,2).valid,"invalid record");
    results=Results(r,0);
    Check(!FarmContestComparisonSystem::Build(0,10,results,true,.1f).referenceAvailable,"unreachable reference hidden");
    Check(FarmContestComparisonSystem::MinimumGrams(r,65,std::numeric_limits<float>::infinity())==0,"invalid cap");
    Check(FarmContestComparisonSystem::ToGrams(farm::CropType::None,1)==0,"invalid crop");
    Check(FarmContestComparisonSystem::ToGrams(farm::CropType::Carrot,std::numeric_limits<float>::infinity())==0,"invalid size");
    p.Begin({},false); Check(!p.Active() && !p.SelectPage(0),"invalid begin closed");
    using Cue = FarmContestPresentationSystem::Cue;
    for (float dt : {1.f/30,1.f/60,1.f/144,.1f})
    for (bool staged : {false,true}) for (bool passed : {false,true}) {
        auto comparison=legacy; comparison.staged=staged; comparison.passed=passed;
        const auto verdict=!staged?Cue::Recorded:passed?Cue::Passed:Cue::Failed;
        p.Begin(comparison,false);
        float previous=0, previousTotal=0, previousAdvice=0;
        int assessments=0, verdicts=0;
        for (int step=0;step<300;++step) {
            p.Update(dt);
            const auto frame=p.GetFrame();
            Check(frame.specimenReveal>=previous && frame.specimenReveal<=1,"monotonic bounded specimen");
            Check(frame.totalReveal>=previousTotal && frame.totalReveal<=1,"monotonic bounded total");
            Check(frame.adviceReveal>=previousAdvice && frame.adviceReveal<=1,"monotonic bounded advice");
            Check(frame.totalReveal==0 || frame.specimenReveal==1,"specimen before total");
            Check(!frame.verdictVisible || frame.totalReveal==1,"verdict after count");
            Check(frame.adviceReveal==0 || frame.verdictVisible,"advice after verdict");
            const auto cue=p.ConsumeCue();
            if(cue==Cue::Assess) {
                ++assessments; Check(frame.specimenReveal==1 && !frame.verdictVisible,"assessment matches visible count");
            }
            else if(cue==verdict) ++verdicts;
            else Check(cue==Cue::None,"no wrong verdict cue");
            Check(p.ConsumeCue()==Cue::None,"consume once");
            farmui::View frameView; farmui::BuildContestDetailView(frameView,frame); Layout(frameView);
            Check(frame.complete==p.Complete(),"UI and controller share exact complete state");
            if ((step+1)*dt<1.49f) Check(!frame.complete,"no early controls");
            if ((step+1)*dt>1.51f) Check(frame.complete,"complete without hidden waits");
            previous=frame.specimenReveal;
            previousTotal=frame.totalReveal; previousAdvice=frame.adviceReveal;
        }
        Check(assessments==1 && verdicts==1,"one assessment one verdict");
        for(int skipStep=0;skipStep<180;++skipStep) {
            p.Begin(comparison,false);
            bool verdictHeard=false;
            for(int step=0;step<skipStep;++step) {
                p.Update(1.f/60);
                if(p.ConsumeCue()==verdict) verdictHeard=true;
            }
            p.Reveal();
            Check(p.ConsumeCue()==(verdictHeard?Cue::None:verdict),"skip verdict once");
            p.Reveal(); Check(p.ConsumeCue()==Cue::None,"repeated skip silent");
            Check(p.GetFrame().adviceReveal==1,"skip fully readable");
            p.Close(); Check(p.ConsumeCue()==Cue::None,"close discards sound");
        }
        p.Begin(comparison,false);
        p.Update(std::numeric_limits<float>::infinity());
        Check(p.GetFrame().specimenReveal==0,"infinite dt ignored");
        p.Update(100); Check(p.GetFrame().specimenReveal<1,"large dt bounded");
        p.Close(); p.Reveal(); Check(p.ConsumeCue()==Cue::None,"inactive skip silent");
    }
    std::cout << "PASS: " << cases << " comparisons, integer boundaries, advice, legacy, layout, reveal\n";
}
