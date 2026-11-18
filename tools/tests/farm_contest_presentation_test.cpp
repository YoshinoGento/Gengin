#include "farm/ui/FarmContestDetailView.h"
#include "farm/render/FarmContestStageLayout.h"
#include "farm/render/FarmCelebrationGeometry.h"
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
        const auto& label=farmui::kLabels[static_cast<std::size_t>(item.label)];
        const float scale=(std::min)(1.f,available/label.width);
        const float fontSize=item.label==farmui::Label::FinalClearTitle ? 72.f : 26.f;
        Check(fontSize*scale>=20,"20px readable labels");
        Check(fontSize*scale<=r.height,"glyphs fit height without atlas padding");
        Check(item.value.empty() || item.valueOffset+14*(item.value.size()-1)+24<=r.width,"numeric text fits");
        for (std::size_t j=i+1;j<view.count;++j) {
            if (item.opacity==0 || view.items[j].opacity==0) continue;
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
        if (page==0) Check(!Has(view,farmui::Label::ContestWeightShort) &&
            !Has(view,farmui::Label::ContestExplanation) && !Has(view,farmui::Label::ContestHintWater) &&
            !Has(view,farmui::Label::PestAdvice) && !Has(view,farmui::CropLabel(before.quality.crop)) &&
            view.count<=9,"specimen-first summary without screen-space crop name");
        if (page==1 && before.qualityAdvice.focus==FarmQualityFocus::Water) {
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
        Check(Has(view,farmui::Label::ContestRuleShort)==(page!=0) &&
            Has(view,farmui::Label::ContestWeightRuleShort)==(page!=0),"rules only on detail pages");
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

farmui::Rect NameBounds(const FarmContestStageLayout::CeremonyCamera& camera, const char* mesh="board") {
    const auto position=FarmContestStageLayout::kNamePosition;
    const auto scale=FarmContestStageLayout::kNameScale;
    std::ifstream file(std::string("project/Resources/farm/contest/ceremony/")+mesh+".obj");
    Check(file.good(),"name board/face mesh present");
    float minX=1280,maxX=0,minY=720,maxY=0;
    std::size_t count=0;
    std::string line;
    while (std::getline(file,line)) {
        if (!line.starts_with("v ")) continue;
        std::istringstream fields(line.substr(2)); Vector3 v{};
        Check(static_cast<bool>(fields>>v.x>>v.y>>v.z),"name mesh parses");
        const float x=-v.x*scale.x+position.x;
        const float dy=v.y*scale.y+position.y-camera.position.y;
        const float dz=v.z*scale.z+position.z-camera.position.z;
        const float viewY=dy*std::cos(camera.pitch)+dz*std::sin(camera.pitch);
        const float depth=-dy*std::sin(camera.pitch)+dz*std::cos(camera.pitch);
        Check(depth>.1f,"nameplate positive depth");
        const float focal=360/std::tan(FarmContestStageLayout::kFovY*.5f);
        const float px=640+(x-camera.position.x)*focal/depth,py=360-viewY*focal/depth;
        Check(std::isfinite(px) && std::isfinite(py),"name projection finite");
        minX=(std::min)(minX,px); maxX=(std::max)(maxX,px);
        minY=(std::min)(minY,py); maxY=(std::max)(maxY,py); ++count;
    }
    Check(count>0,"name geometry inspected");
    return {minX,minY,maxX-minX,maxY-minY};
}

void WorldNameplate() {
    FarmContestPresentationSystem::Frame frame;
    const auto distantBounds=NameBounds(FarmContestStageLayout::CeremonyView(frame));
    frame.specimenReveal=1; frame.adviceReveal=1;
    const auto camera=FarmContestStageLayout::CeremonyView(frame);
    const auto closeBounds=NameBounds(camera),face=NameBounds(camera,"name_face");
    Check(closeBounds.width>distantBounds.width*1.3f && closeBounds.height>distantBounds.height*1.3f,"rigid sign scales with camera distance");
    Check(face.x>closeBounds.x && face.y>closeBounds.y && face.x+face.width<closeBounds.x+closeBounds.width &&
        face.y+face.height<closeBounds.y+closeBounds.height,"printed face remains inside wood border");
    auto angled=camera; angled.pitch=.3f;
    const auto oblique=NameBounds(angled,"name_face");
    Check(std::abs(oblique.width/oblique.height-face.width/face.height)>.1f,"face perspective changes with viewing angle");
    const auto p=FarmContestStageLayout::kNamePosition, s=FarmContestStageLayout::kNameScale;
    Check(p.x+.78f*s.x < -2.15f-.28f && p.y-.38f*s.y>.08f && p.y+.38f*s.y<.9f,
        "board is left of medal and mounted below table top");
    Check(p.z+.10f*s.z>=-1.18f && p.z-.02f*s.z<-1.18f,"wood attaches to cloth front");
    Check(.003f*s.z>=.001f && .003f*s.z<=.01f,"face clearance avoids coplanar depth fighting");
    std::cout<<"PASS: world nameplate projected width "<<distantBounds.width<<" -> "<<closeBounds.width<<"; face inside rigid board\n";
}

void FinalClearFlow() {
    static_assert(FarmContestPresentationSystem::kClearCelebrationEnd-FarmContestPresentationSystem::kClearAnnouncement==farm::celebration::kEndSeconds);
    {
        using namespace farm::celebration;
        std::array<Vertex,kVertexCount+1> vertices{};
        vertices.back().position={123,456,789};
        const auto output=std::span<Vertex>(vertices.data(),kVertexCount);
        Check(Build(output,false,1,{}, {0,0,1})==0,"inactive confetti");
        Check(Build(output.first(10),true,1,{}, {0,0,1})==0,"short vertex output rejected");
        for(float time:{-1.f,5.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()})
            Check(Build(output,true,time,{}, {0,0,1})==0,"invalid/finished clock rejected");
        Check(Sample(kParticleCount,1).size==0,"particle index bounded");
        std::size_t peak=0;
        const Vector3 forward{0,-std::sin(FarmContestStageLayout::kCeremonyPitch),std::cos(FarmContestStageLayout::kCeremonyPitch)};
        for(int tick=0;tick<=500;++tick) {
            const float seconds=static_cast<float>(tick)*.01f;
            const auto count=Build(output,true,seconds,{-3,4,-10},forward);
            peak=(std::max)(peak,count);
            Check(count<=kVertexCount && count%12==0,"bounded folded petals");
            float previous=1000;
            for(std::size_t i=0;i<count;i+=12) {
                float center=0;
                for(std::size_t j=0;j<12;++j) {
                    const auto& v=vertices[i+j];
                    Check(Finite(v.position) && v.position.y>=kDeckHeight-.001f,"finite particles never sink through deck");
                    Check(v.position.x>-6.704f && v.position.x<-.416f &&
                        v.position.z>-2.85f && v.position.z<2.27f,"particles stay over authored deck and in front of banner");
                    Check(v.position.z<-1.65f || v.position.z>1.65f,"front and rear corridors avoid all crop and stage solids");
                    Check(v.color.w>=0 && v.color.w<=1,"bounded opacity");
                    center+=(v.position.y*forward.y+v.position.z*forward.z)/12;
                }
                Check(center<=previous+.001f,"back-to-front transparency"); previous=center;
            }
            Check(vertices.back().position.x==123,"output canary retained");
        }
        Check(peak==kVertexCount,"full bounded burst reaches budget");
        const auto a=Sample(70,1.5f), b=Sample(70,1.5f), c=Sample(70,2.f);
        Check(a.position.x==b.position.x && a.rotation.x==b.rotation.x && a.rotation.x!=c.rotation.x,"repeatable moving rotating petals");
        std::size_t front=0,rear=0,leftward=0,rightward=0,towardCamera=0,awayCamera=0;
        float minX=100,maxX=-100,minimumLanding=5,maximumLanding=0;
        for(std::size_t i=0;i<kParticleCount;++i) {
            const auto start=Sample(i,.5f), middle=Sample(i,2.5f);
            front+=middle.position.z<0; rear+=middle.position.z>0;
            leftward+=middle.position.x<start.position.x; rightward+=middle.position.x>start.position.x;
            towardCamera+=middle.position.z<start.position.z; awayCamera+=middle.position.z>start.position.z;
            minX=(std::min)(minX,middle.position.x); maxX=(std::max)(maxX,middle.position.x);
            bool landed=false,faded=false,expired=false;
            Piece previous{};
            for(int tick=0;tick<=500;++tick) {
                const float time=static_cast<float>(tick)*.01f;
                const auto p=Sample(i,time);
                if(p.size==0) {
                    if(previous.size>0) { Check(landed && faded,"every expiry follows grounded fade"); expired=true; }
                    previous=p; continue;
                }
                Check(!expired,"expired piece cannot reappear");
                if(time>kSpawnWindow+kFadeInSeconds && p.color.w<.999f)
                    Check(p.landed,"no mid-air fade after launch");
                if(p.landed) {
                    if(!landed) { minimumLanding=(std::min)(minimumLanding,time); maximumLanding=(std::max)(maximumLanding,time); }
                    landed=true; faded|=p.color.w<.999f;
                    const auto normal=Rotate({0,0,1},p.rotation);
                    Check(std::abs(normal.y)>.999f,"landed petal lies flat");
                    if(previous.landed) Check(p.position.x==previous.position.x && p.position.y==previous.position.y &&
                        p.position.z==previous.position.z && p.rotation.x==previous.rotation.x,"landed piece does not hover or slide");
                }
                if(previous.size>0) {
                    Check(std::abs(p.position.x-previous.position.x)<.1f && std::abs(p.position.y-previous.position.y)<.1f &&
                        std::abs(p.position.z-previous.position.z)<.1f,"continuous flight and touchdown");
                    const float verticalSpeed=(p.position.y-previous.position.y)/.01f;
                    Check(verticalSpeed>-(kTerminalMin+kTerminalRange)-.02f,"descent bounded by terminal speed");
                }
                previous=p;
            }
            Check(landed && faded && expired,"all pieces land and fade within ceremony");
        }
        Check(front==48 && rear==144,"sparse foreground and broad rear budget");
        Check(leftward>40 && rightward>40 && towardCamera>40 && awayCamera>40,"scattering in both horizontal and depth directions");
        Check(maxX-minX>4.f,"burst does not collapse into two narrow columns");
        Check(maximumLanding-minimumLanding>.5f,"staggered touchdown, not simultaneous disappearance");
        std::cout<<"PASS: front/rear "<<front<<'/'<<rear<<", touchdown "<<minimumLanding<<".."<<maximumLanding
            <<" seconds, grounded fade and terminal descent for all pieces\n";
        std::cout<<"PASS: 501 particle frames, 192-piece cap, depth order, world bounds and guarded output\n";
    }
    using Presentation=FarmContestPresentationSystem;
    using Cue=Presentation::Cue;
    for (int index=0;index<3;++index) for (bool passed:{false,true})
    for (bool staged:{false,true}) for (bool replay:{false,true}) {
        const int day=FarmContestEntrySystem::kContestDays[index];
        const auto record=Record(farm::CropType::Pumpkin,passed?100:0,3.f,day);
        const auto result=Results(record,index);
        const auto comparison=FarmContestComparisonSystem::Build(index,day,result,staged,3.f);
        Check(comparison.valid,"clear fixture valid");
        Presentation p; p.Begin(comparison,replay);
        const bool eligible=index==2 && passed && staged && !replay;
        Check(p.GetFrame().finalClear==eligible,"fresh final pass starts clear without input");
        if (!eligible) {
            Check(p.ConsumeCue()==Cue::None,"other outcomes do not announce clear");
            p.Reveal(); Check(!p.GetFrame().finalClear,"other outcomes never become clear");
            continue;
        }
        Check(p.ConsumeCue()==Cue::Drumroll && p.ConsumeCue()==Cue::None,"one anticipation cue before announcement");
        Check(!p.Complete() && p.GetFrame().resultReveal==0,"celebration precedes results");
        Check(!p.SelectPage(1) && !p.SelectPage(2),"analysis cannot interrupt clear");
        p.ToggleExplanation(); Check(!p.GetFrame().explanation,"no help overlay in clear");
        for (float dt:{0.f,-1.f,std::numeric_limits<float>::quiet_NaN()}) p.Update(dt);
        Check(p.GetFrame().clearReveal==0,"invalid clear elapsed ignored");
        std::size_t projectedParticles=0;
        int announcements=0;
        std::array<farm::celebration::Vertex,farm::celebration::kVertexCount> particleVertices{};
        const int clearTicks=static_cast<int>(std::ceil(Presentation::kClearSeconds/.05f))+5;
        const auto timing=p.ClearTimeline();
        for(int tick=0;tick<clearTicks;++tick) {
            const auto frame=p.GetFrame();
            farmui::View view; farmui::BuildContestDetailView(view,frame); Layout(view);
            Check(Has(view,farmui::Label::FinalClearTitle) && Has(view,farmui::Label::ContestScoreShort) &&
                !Has(view,farmui::Label::FinalClearBegin),"one stage contains celebration and results, no clear-entry button");
            Check(frame.resultReveal>=0 && frame.resultReveal<=1,"bounded result reveal");
            if(frame.clearSeconds<Presentation::kClearAnnouncement) {
                Check(!frame.verdictVisible && frame.verdictReveal==0 && frame.adviceReveal==0 &&
                    !frame.celebrationActive && frame.celebrationSeconds==0 && view.items[0].opacity==0,
                    "drumroll cannot leak clear title, verdict, award or confetti");
            } else {
                Check(frame.verdictVisible && std::abs(frame.celebrationSeconds-
                    (frame.clearSeconds-Presentation::kClearAnnouncement))<.001f,"announcement and confetti share local time");
            }
            Check(frame.complete==(frame.resultReveal==1),"results become actionable exactly when readable");
            Check(frame.displayedTotal==comparison.total && frame.totalReveal==1,"no under-target count-up after award");
            if (frame.clearSeconds<=timing.resultStart)
                Check(frame.resultReveal==0,"result hidden until celebration title settles");
            if (frame.clearSeconds>=timing.resultEnd)
                Check(frame.resultReveal==1 && frame.clearReveal==1,"result and title settle automatically");
            for (std::size_t i=1;i+1<view.count;++i)
                if (view.items[i].label < farmui::Label::CheerAudienceLeft)
                    Check(view.items[i].opacity<=frame.resultReveal,"no detail leaks before celebration");
            using Side=FarmContestCheerSystem::Side;
            const bool left=frame.clearSeconds>=Presentation::kCheerLeftStart && frame.clearSeconds<timing.leftEnd;
            const bool right=frame.clearSeconds>=timing.rightStart && frame.clearSeconds<timing.rightEnd;
            Check(frame.cheer.side==(left?Side::Left:right?Side::Right:Side::None),"left then right, never simultaneous");
            Check(view.cheerBubble.visible==(frame.cheer.opacity>0),"no stale bubble outside speech");
            Check(!view.cheerBubble.visible || frame.resultReveal==0,"speech finishes before results");
            Check(p.GetFrame().cheer.pair==frame.cheer.pair,"snapshot never rerolls dialogue");
            Check(view.Hit({1050,676}).action==(frame.complete?farmui::Action::ContestDetailClose:farmui::Action::ContestReveal),"skip and continue are distinct");
            const auto camera=FarmContestStageLayout::CeremonyView(frame);
            const auto name=NameBounds(camera);
            Check(name.x>24 && name.x+name.width<1256 && name.y>182 && name.y+name.height<648,"clear camera retains fixed nameplate");
            const float cp=std::cos(camera.pitch),sp=std::sin(camera.pitch);
            const float focal=360/std::tan(FarmContestStageLayout::kFovY*.5f);
            const auto count=farm::celebration::Build(particleVertices,frame.celebrationActive,frame.celebrationSeconds,camera.position,{0,-sp,cp});
            for(std::size_t i=0;i<count;++i) {
                const auto v=particleVertices[i].position;
                const float dy=v.y-camera.position.y,dz=v.z-camera.position.z;
                const float depth=-dy*sp+dz*cp;
                Check(depth>.1f,"confetti stays beyond camera near plane");
                const float px=640+(v.x-camera.position.x)*focal/depth,py=360-(dy*cp+dz*sp)*focal/depth;
                Check(px>0 && px<1280 && py>0 && py<720,"confetti stays inside viewport throughout camera move");
                ++projectedParticles;
            }
            Check(frame.comparison.total==comparison.total && frame.comparison.grams==comparison.grams,"clear never changes judged values");
            const auto oldSeconds=frame.clearSeconds;
            p.Update(.05f);
            const auto cue=p.ConsumeCue();
            const bool announced=oldSeconds<Presentation::kClearAnnouncement && p.GetFrame().clearSeconds>=Presentation::kClearAnnouncement;
            Check(cue==(announced?Cue::Clear:Cue::None),"one clear hit at the visual announcement boundary");
            announcements+=announced;
            Check(p.ConsumeCue()==Cue::None,"phase cue consumed once");
        }
        Check(projectedParticles>100000,"moving-camera particle geometry exercised");
        std::cout<<"PASS: "<<projectedParticles<<" particle vertices within moving-camera viewport\n";
        Check(announcements==1 && p.Complete() && !p.Back(),"one announcement then finished summary delegates to outcome");
        Pages(p);
        Check(p.GetFrame().clearSeconds==timing.resultEnd && p.ConsumeCue()==Cue::None,"details never replay celebration");
        p.Close(); Check(!p.Active() && !p.GetFrame().finalClear,"close clears celebration");
        for(int skip=0;skip<=clearTicks;++skip) {
            p.Begin(comparison,false);
            (void)p.ConsumeCue();
            for(int i=0;i<skip;++i)p.Update(.05f);
            p.Reveal(); p.Reveal();
            Check(p.Complete() && p.Active() && p.GetFrame().finalClear && p.ConsumeCue()==Cue::Silence &&
                p.ConsumeCue()==Cue::None && !p.GetFrame().celebrationActive,"skip stops obsolete audio and confetti without closing");
            Check(p.GetFrame().resultReveal==1 && p.GetFrame().displayedTotal==comparison.total,"skip shows final results in same stage");
            farmui::View view; farmui::BuildContestDetailView(view,p.GetFrame()); Layout(view);
            Check(!view.cheerBubble.visible && p.GetFrame().cheer.side==FarmContestCheerSystem::Side::None,
                "skip removes both speech and bubble immediately");
            Check(view.Hit({1050,676}).action==farmui::Action::ContestDetailClose &&
                Has(view,farmui::Label::FinalClearContinue),"only completed summary offers season list");
        }
        p.BeginUnsubmitted(day); p.Reveal(); Check(!p.GetFrame().finalClear,"missed has no clear");
    }
    std::cout<<"PASS: final-clear anticipation/alternating cheers/results, all .05s skip points, phase cues, layout and immutable scores\n";
}

void CeremonyFraming() {
    constexpr std::array<const char*,8> paths{{"crop_turnip","crop_turnip_leaves","crop_carrot","crop_carrot_leaves",
        "crop_tomato","crop_tomato_stems","crop_pumpkin","crop_pumpkin_vines"}};
    std::size_t checked=0;
    Check((1280-FarmContestStageLayout::kResultLeft)/1280<=.30f,"result uses at most thirty percent");
    float minX=1280,maxX=0,minY=720,maxY=0,maxCropDepth=0;
    for (int slot=0;slot<farm::kFarmCropTypeCount;++slot) for (float size:{.1f,1.f,3.f,8.f})
    for (float reveal:{0.f,.5f,1.f}) for (float settle:{0.f,.5f,1.f})
    for (float clear:{0.f,.5f,1.f}) for (int part=0;part<2;++part) {
        const auto crop=static_cast<farm::CropType>(slot+1);
        Check(farm::ToCropSlot(crop)==slot,"fixture crop slot");
        FarmContestPresentationSystem::Frame frame; frame.specimenReveal=reveal; frame.adviceReveal=settle;
        frame.finalClear=clear>0; frame.resultReveal=1-clear;
        const auto camera=FarmContestStageLayout::CeremonyView(frame);
        const auto name=NameBounds(camera);
        Check(name.x>24 && name.x+name.width<FarmContestStageLayout::kResultLeft &&
            name.y>150 && name.y+name.height<648,"world nameplate stays in specimen region");
        const auto pose=FarmContestStageLayout::CeremonySpecimen(crop,size);
        const auto scale=part==0?pose.bodyScale:pose.leafScale;
        std::ifstream file(std::string("project/Resources/farm/contest/ceremony/")+paths[slot*2+part]+".obj");
        Check(file.good(),"ceremony mesh present");
        std::string line;
        while (std::getline(file,line)) {
            if (!line.starts_with("v ")) continue;
            std::istringstream stream(line.substr(2)); float x=0,y=0,z=0; stream>>x>>y>>z;
            x=-x*scale.x; y=y*scale.y+(part==0?pose.position.y:pose.leafY); z*=scale.z;
            const float c=std::cos(FarmContestStageLayout::kYaw),s=std::sin(FarmContestStageLayout::kYaw);
            const float worldX=x*c+z*s+pose.position.x;
            const float worldZ=-x*s+z*c+pose.position.z;
            maxCropDepth=(std::max)(maxCropDepth,std::abs(worldZ));
            const float dy=y-camera.position.y,dz=worldZ-camera.position.z;
            const float viewY=dy*std::cos(camera.pitch)+dz*std::sin(camera.pitch);
            const float depth=-dy*std::sin(camera.pitch)+dz*std::cos(camera.pitch);
            Check(depth>.1f,"ceremony in front of near plane");
            const float focal=360/std::tan(FarmContestStageLayout::kFovY*.5f);
            const float px=640+(worldX-camera.position.x)*focal/depth,py=360-viewY*focal/depth;
            if(clear==1 && reveal==1 && settle==1) {
                for(const auto& bubble:{farmui::contestcheer::kLeftBody,farmui::contestcheer::kRightBody})
                    Check(px<bubble.x || px>bubble.x+bubble.width || py<bubble.y || py>bubble.y+bubble.height,
                        "all four actual crops avoid both speech regions");
            }
            Check(px<name.x || px>name.x+name.width ||
                py<name.y || py>name.y+name.height,"crop avoids world nameplate");
            minX=(std::min)(minX,px); maxX=(std::max)(maxX,px);
            minY=(std::min)(minY,py); maxY=(std::max)(maxY,py);
            ++checked;
        }
    }
    std::cout<<"ceremony bounds "<<minX<<','<<minY<<" .. "<<maxX<<','<<maxY<<", crop half-depth "<<maxCropDepth<<'\n';
    Check(maxCropDepth<1.60f,"actual crop geometry stays between particle flight corridors with clearance");
    Check(minX>25 && maxX<FarmContestStageLayout::kResultLeft-24 && minY>150 && maxY<555,"ceremony crop avoids UI and frame edges");
    std::cout<<"PASS: "<<checked<<" ceremony vertices across camera/size variants\n";
}

void CheerRules() {
    using Cheer=FarmContestCheerSystem;
    using Pair=Cheer::Pair;
    using Side=Cheer::Side;
    auto q=Record(farm::CropType::Pumpkin,95,1.5f,30).quality;
    for(const auto crop:{farm::CropType::TestCrop,farm::CropType::Carrot,farm::CropType::Tomato,farm::CropType::Pumpkin}) {
        const auto round=Record(crop,95,1.5f,30).quality;
        Check(Cheer::Eligible(Pair::TurnipStar,round,90,54)==(crop==farm::CropType::Pumpkin),
            "round-star recording targets pumpkin, not legacy turnip");
    }
    Check(Cheer::Eligible(Pair::PumpkinPot,q,90,54),"large pumpkin threshold inclusive");
    q.harvestSize.multiplier=1.499f;
    Check(!Cheer::Eligible(Pair::PumpkinPot,q,90,54),"small pumpkin not praised as huge");
    q.harvestSize={0,false};
    Check(!Cheer::Eligible(Pair::LargeDelivery,q,90,54),"unknown size not invented");
    Check(Cheer::Eligible(Pair::QualityMuseum,q,90,54) && !Cheer::Eligible(Pair::QualityMuseum,q,90,53),
        "quality praise follows awarded quality points");
    Check(Cheer::Eligible(Pair::Perfect,q,100,60) && !Cheer::Eligible(Pair::Perfect,q,99,60),
        "perfect praise requires full marks");
    for(float care:{.899f,.9f,1.f,1.01f,std::numeric_limits<float>::quiet_NaN()}) {
        q.waterBalance=q.nutrientBalance=care;
        const bool expected=std::isfinite(care) && care>=.9f && care<=1;
        Check(Cheer::Eligible(Pair::WaterCare,q,90,54)==expected,"water praise uses finite recorded care");
        Check(Cheer::Eligible(Pair::NutrientSoil,q,90,54)==expected,"nutrient praise uses finite recorded care");
        q.nutrientKnown=false;
        Check(!Cheer::Eligible(Pair::NutrientSoil,q,90,54),"unknown soil cannot earn specific praise");
        q.nutrientKnown=true;
    }
    Check(!Cheer::Eligible(Pair::None,q,90,54) && !Cheer::Eligible(Pair::Count,q,90,54),"invalid pair bounded");
    Check(Cheer::Select(q,101,60,Pair::None)==Pair::None && Cheer::Select(q,90,61,Pair::None)==Pair::None &&
        Cheer::Select(q,90,49,Pair::None)==Pair::None,"inconsistent scores rejected");
    q.crop=farm::CropType::None;
    Check(Cheer::Select(q,90,54,Pair::None)==Pair::None,"invalid crop has no dialogue");
    std::array<bool,static_cast<std::size_t>(Pair::Count)> reached{};
    std::size_t selections=0;
    for(const auto crop:{farm::CropType::Carrot,farm::CropType::Tomato,farm::CropType::Pumpkin})
    for(float size:{1.f,1.5f,3.f}) for(int points:{40,53,54,60}) for(float care:{.5f,1.f}) {
        q=Record(crop,95,size,30).quality;
        q.waterBalance=q.nutrientBalance=care;
        Pair previous=Pair::None;
        for(int repeat=0;repeat<32;++repeat) {
            const auto pair=Cheer::Select(q,points+40,points,previous);
            Check(pair!=Pair::None && pair!=previous && Cheer::Eligible(pair,q,points+40,points),
                "bounded rotation returns matching pair without adjacent repeat");
            reached[static_cast<std::size_t>(pair)]=true; previous=pair; ++selections;
        }
    }
    for(bool found:reached) Check(found,"every authored pair reachable with the three playable crops");
    using Presentation=FarmContestPresentationSystem;
    for(int pair=0;pair<static_cast<int>(Pair::Count);++pair) for(auto side:{Side::Left,Side::Right}) {
        const int index=Cheer::ClipIndex(static_cast<Pair>(pair),side);
        Check(index>=0 && index<static_cast<int>(farm::cheerassets::kClips.size()),"safe speech asset index");
        const auto& asset=farm::cheerassets::kClips[index];
        const auto expected=std::string("cheers/")+std::string(farm::cheerassets::kPairNames[pair])+
            (side==Side::Left?"_L.wav":"_R.wav");
        Check(expected==asset.file,"pair and side map to corresponding recording");
        const auto timing=Presentation::TimingFor(static_cast<Pair>(pair));
        const float start=side==Side::Left?Presentation::kCheerLeftStart:timing.rightStart;
        const float end=side==Side::Left?timing.leftEnd:timing.rightEnd;
        Check(start+Cheer::kVoiceDelay+asset.seconds+Presentation::kCheerReadHold<=end-Presentation::kCheerFadeOut+.00001f,
            "every recording finishes before its subtitle fades");
        const float replyGap=timing.rightStart-Presentation::kCheerLeftStart-Cheer::VoiceSeconds(static_cast<Pair>(pair),Side::Left);
        Check(std::abs(replyGap-Cheer::ReplyGap(static_cast<Pair>(pair)))<.00001f && replyGap>=.29f && replyGap<=.46f,
            "response follows each actual recording without longest-clip silence");
        Check(timing.leftEnd<timing.rightStart && timing.rightEnd<timing.resultStart &&
            timing.resultEnd<=Presentation::kClearSeconds+.00001f,"pair-specific timeline exclusive and bounded");
    }
    Check(Cheer::ClipIndex(Pair::None,Side::Left)==-1 && Cheer::ClipIndex(Pair::Count,Side::Left)==-1 &&
        Cheer::ClipIndex(Pair::Guests,Side::None)==-1,"invalid audio index rejected");
    for(int pair=0;pair<static_cast<int>(Pair::Count);++pair) for(auto side:{Side::Left,Side::Right})
    for(float entrance:{0.f,.5f,1.f}) {
        farmui::View view; view.modal=view.contestDetail=true;
        farmui::contestcheer::Build(view,{static_cast<Pair>(pair),side,1,entrance});
        Layout(view);
        const auto& bubble=view.cheerBubble;
        Check(view.count==2 && bubble.visible && bubble.tailLeft==(side==Side::Left),"single bounded dialogue-only two-line speech bubble");
        const auto& r=bubble.rect;
        Check(r.x>=0 && r.x+r.width<=1280 && r.y>=0 && r.y+r.height<=720,"bubble slide remains on screen");
        for(std::size_t i=0;i<view.count;++i) {
            const auto& item=view.items[i];
            Check(item.label==farmui::contestcheer::kLines[pair][(side==Side::Left?0:2)+i],"only authored dialogue appears in speech bubble");
            Check(item.label!=farmui::Label::CheerAudienceLeft && item.label!=farmui::Label::CheerAudienceRight,
                "spectator direction headings are not displayed");
            Check(item.darkInk && item.request.action==farmui::Action::None,"speech is readable display only");
            Check(item.rect.x>=r.x && item.rect.x+item.rect.width<=r.x+r.width &&
                item.rect.y>=r.y && item.rect.y+item.rect.height<=r.y+r.height,"text stays in bubble");
            if (i>0) Check(view.items[i-1].rect.y+view.items[i-1].rect.height<=item.rect.y,"dialogue lines do not overlap");
        }
    }
    for(const auto frame:{Cheer::Frame{},Cheer::Frame{Pair::Count,Side::Left,1,1},
        Cheer::Frame{Pair::Guests,static_cast<Side>(99),1,1},Cheer::Frame{Pair::Guests,Side::Left,0,1},
        Cheer::Frame{Pair::Guests,Side::Left,std::numeric_limits<float>::quiet_NaN(),1}}) {
        farmui::View view; farmui::contestcheer::Build(view,frame);
        Check(view.count==0 && !view.cheerBubble.visible,"invalid speech snapshot draws nothing");
    }
    FarmContestPresentationSystem p;
    const auto comparison=FarmContestComparisonSystem::Build(2,30,Results(Record(farm::CropType::Pumpkin,98,2.5f,30),2),true,3);
    p.Begin(comparison,false); for(int i=0;i<25;++i)p.Update(.1f);
    const auto first=p.GetFrame().cheer.pair;
    Check(first!=Pair::None,"fresh final success has dialogue");
    Check(p.GetFrame().cheer.voiceReady && p.GetFrame().cheer.opacity==1,"voice begins only after readable bubble");
    p.Close(); p.Begin(comparison,false); for(int i=0;i<25;++i)p.Update(.1f);
    Check(p.GetFrame().cheer.pair!=first,"replay retains previous selection across close");
    p.Begin(comparison,true); for(int i=0;i<25;++i)p.Update(.1f);
    Check(p.GetFrame().cheer.side==Side::None,"record review never replays cheers");
    for(int day:{10,20,30}) {
        p.BeginUnsubmitted(day); for(int i=0;i<25;++i)p.Update(.1f);
        Check(p.GetFrame().cheer.side==Side::None,"unsubmitted never celebrates");
    }
    std::cout<<"PASS: "<<selections<<" conditional cheer selections,16 reachable pairs,96 bubble layouts and lifecycle\n";
}

int main() {
    CheerRules();
    FinalClearFlow();
    WorldNameplate();
    CeremonyFraming();
    SpecimenFraming();
    for (bool partial : {false,true}) {
        FarmContestPresentationSystem::Frame frame;
        frame.complete=true; frame.comparison.quality.pestPenalty=15;
        frame.comparison.qualityAdvice.partial=partial;
        frame.page=FarmContestPresentationSystem::Page::Quality;
        farmui::View view; farmui::BuildContestDetailView(view,frame); Layout(view);
        Check(Has(view,farmui::Label::PestAdvice) && Has(view,farmui::Label::PestPenalty),"pest advice retained on detail page");
    }
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
                Check(presentation.Active() && (presentation.GetFrame().finalClear ||
                    !presentation.GetFrame().verdictVisible),"begin automatic clear or ordinary reveal");
                presentation.Update(std::numeric_limits<float>::quiet_NaN());
                presentation.Update(-1);
                Check(presentation.GetFrame().specimenReveal==0,"invalid dt ignored");
                farmui::View view; farmui::BuildContestDetailView(view,presentation.GetFrame()); Layout(view);
                Check(!presentation.SelectPage(1),"cannot hide ongoing judgement with a tab");
                presentation.ToggleExplanation(); Check(!presentation.GetFrame().explanation,"rules gated during reveal");
                Check(view.Hit({250,120}).action==farmui::Action::None,"tab not clickable during reveal");
                Check(view.Hit({1050,676}).action==farmui::Action::ContestReveal,"skip revealed through UI");
                for (int step=0;step<static_cast<int>(std::ceil(FarmContestPresentationSystem::kClearSeconds*60))+5;++step)
                    presentation.Update(1.f/60);
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
        for (int step=0;step<720;++step) {
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
            if ((step+1)*dt<FarmContestPresentationSystem::kRevealSeconds-.01f) Check(!frame.complete,"no early controls");
            if ((step+1)*dt>FarmContestPresentationSystem::kRevealSeconds+.01f) Check(frame.complete,"complete without hidden waits");
            previous=frame.specimenReveal;
            previousTotal=frame.totalReveal; previousAdvice=frame.adviceReveal;
        }
        Check(assessments==1 && verdicts==1,"one assessment one verdict");
        for(int skipStep=0;skipStep<260;++skipStep) {
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
    for (const int day : {10,20,30}) {
        p.BeginUnsubmitted(day);
        Check(p.Active() && p.GetFrame().unsubmitted && !p.GetFrame().comparison.valid,"missing is not a zero-score comparison");
        int sounds=0;
        for (int step=0;step<650;++step) {
            p.Update(1.f/144); v={}; farmui::BuildContestDetailView(v,p.GetFrame()); Layout(v);
            const auto cue=p.ConsumeCue();
            Check(cue==Cue::None || cue==Cue::Recorded,"missing has no assessment/failure sound");
            sounds+=cue==Cue::Recorded;
            Check(!Has(v,farmui::Label::ContestScoreShort) && !v.radar.visible,"missing has no score or radar");
        }
        Check(sounds==1 && p.Complete() && !p.SelectPage(1),"missing completes once without fake analysis");
        p.Reveal(); Check(p.ConsumeCue()==Cue::None,"missing repeated skip silent");
    }
    p.BeginUnsubmitted(9); Check(!p.Active(),"invalid missing day rejected");
    p.Begin(legacy,true);
    const auto initialCloud=p.GetFrame().cloudOffset;
    title::TitleCloudMotionSystem sharedCloud;
    for (int step=0;step<600;++step) {
        p.Update(1.f/60); sharedCloud.Update(1.f/60);
        const auto frame=p.GetFrame();
        Check(frame.complete && frame.ceremonyTime==FarmContestPresentationSystem::kRevealSeconds,"cloud motion never restarts ceremony");
        Check(frame.cloudOffset.x==sharedCloud.Offset().x && frame.cloudOffset.y==sharedCloud.Offset().y,"ceremony uses title wind rule");
        const auto sky=FarmContestStageLayout::CeremonySky(frame);
        Check(sky.skyYaw==frame.cloudOffset.x && sky.cloudLift==frame.cloudOffset.y && sky.nightAmount==0,"daylight sky snapshot carries cloud motion");
    }
    Check(p.GetFrame().cloudOffset.x!=initialCloud.x,"clouds keep moving after verdict settles");
    const auto beforeInvalid=p.GetFrame().cloudOffset;
    p.Update(std::numeric_limits<float>::quiet_NaN());
    Check(p.GetFrame().cloudOffset.x==beforeInvalid.x,"invalid delta leaves clouds unchanged");
    p.Close(); p.Update(.1f);
    Check(p.GetFrame().cloudOffset.x==0 && !p.Active(),"closed ceremony does not advance clouds");
    std::cout << "PASS: " << cases << " comparisons, integer boundaries, advice, legacy, layout, reveal and missing ceremonies\n";
}
