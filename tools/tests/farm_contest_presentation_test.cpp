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
void SpecimenFraming() {
    constexpr std::array<const char*,8> meshes{{"crop_turnip","crop_turnip_leaves","crop_carrot","crop_carrot_leaves",
        "crop_tomato","crop_tomato_stems","crop_pumpkin","crop_pumpkin_vines"}};
    std::size_t checked = 0;
    for (int slot=0;slot<farm::kFarmCropTypeCount;++slot) {
        const auto crop=static_cast<farm::CropType>(slot+1);
        Check(farm::ToCropSlot(crop)==slot,"fixture crop slot");
        for (int side=0;side<2;++side) {
            const auto pose=FarmContestStageLayout::Specimen(crop,2,2,side);
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
                        const float depth=FarmContestStageLayout::kCameraDistance+z;
                        Check(depth>0,"specimen in front of camera");
                        const float perspective=FarmContestStageLayout::kCameraDistance/depth/FarmContestStageLayout::UnitsPerPixel();
                        const float screenX=640+x*perspective;
                        const float screenY=360-(position.y+vertex.y*scale.y)*perspective;
                        Check(screenX>=64+624*side && screenX<=356+624*side,"specimen avoids radar and column edge");
                        Check(screenY>=180 && screenY<=364,"specimen avoids name and weight");
                        ++checked;
                    }
                }
            }
            const auto smallerPose=FarmContestStageLayout::Specimen(crop,.5f,2,side);
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
                Check(presentation.GetFrame().reveal==0,"invalid dt ignored");
                farmui::View view; farmui::BuildContestDetailView(view,presentation.GetFrame()); Layout(view);
                Check(view.Hit({1050,676}).action==farmui::Action::ContestReveal,"skip revealed through UI");
                for (int step=0;step<90;++step) presentation.Update(1.f/60);
                Check(presentation.GetFrame().verdictVisible,"time reveals once");
                view={}; farmui::BuildContestDetailView(view,presentation.GetFrame()); Layout(view);
                Check(view.Hit({1050,676}).action==farmui::Action::ContestDetailClose,"result back action");
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
    farmui::View v; farmui::BuildContestDetailView(v,p.GetFrame()); Layout(v);
    results[0].harvest.quality.nutrientKnown=false;
    auto partial=FarmContestComparisonSystem::Build(0,10,results,true,2);
    p.Begin(partial,true); v={}; farmui::BuildContestDetailView(v,p.GetFrame());
    Check(!v.radar.known[3],"unknown nutrition leaves gap"); Layout(v);
    results[0].qualityPoints++;
    Check(!FarmContestComparisonSystem::Build(0,10,results,true,2).valid,"stored score mismatch");
    results=Results(r,0); results[0].harvest.quality.waterBalance=std::numeric_limits<float>::quiet_NaN();
    Check(!FarmContestComparisonSystem::Build(0,10,results,true,2).valid,"invalid record");
    results=Results(r,0);
    Check(!FarmContestComparisonSystem::Build(0,10,results,true,.1f).referenceAvailable,"unreachable reference hidden");
    Check(FarmContestComparisonSystem::MinimumGrams(r,65,std::numeric_limits<float>::infinity())==0,"invalid cap");
    Check(FarmContestComparisonSystem::ToGrams(farm::CropType::None,1)==0,"invalid crop");
    Check(FarmContestComparisonSystem::ToGrams(farm::CropType::Carrot,std::numeric_limits<float>::infinity())==0,"invalid size");
    p.Begin({},false); Check(!p.Active(),"invalid begin closed");
    std::cout << "PASS: " << cases << " comparisons, integer boundaries, advice, legacy, layout, reveal\n";
}
