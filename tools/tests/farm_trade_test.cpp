#include "farm/ui/FarmSaleView.h"
#include "farm/ui/FarmMenuView.h"
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
void Check(bool ok,const char* text) { if (!ok) { std::cerr<<text<<'\n'; std::exit(1); } }
FarmCropQualityResult Quality(farm::CropType crop) {
    FarmCropQualityResult q;
    q.crop=crop; q.basePrice=100; q.salePrice=150; q.score=90;
    q.maturity=1; q.waterBalance=.8f; q.terrainFit=.8f;
    q.nutrientBalance=.8f; q.nutrientKnown=true; q.harvestSize={1.25f,true};
    return q;
}
void Validate(const farmui::SaleViewState& state) {
    using namespace farmui;
    View view;
    BuildMenuShell(view,8,false,state.confirming,false,Label::Ready);
    BuildSaleView(view,state);
    Check(view.modal && view.count<View::kCapacity,"sale owns input within capacity");
    std::size_t digits=0, icons=0;
    for (std::size_t i=0;i<view.count;++i) {
        const auto& item=view.items[i]; const auto& r=item.rect;
        Check(r.x>=0 && r.y>=0 && r.x+r.width<=1280 && r.y+r.height<=720,"sale bounds");
        const float space=(item.value.empty()?r.width-20:item.valueOffset-30)-(item.leadingIcon?54:0);
        Check(space/kLabels[static_cast<std::size_t>(item.label)].width>=20.f/26.f,"sale readable labels");
        if (!item.value.empty()) Check(item.valueOffset+14*(item.value.size()-1)+24<=r.width,"sale numeric fit");
        digits+=item.value.size(); icons+=IsValidIcon(item.icon);
        for (std::size_t j=i+1;j<view.count;++j) {
            const auto& b=view.items[j].rect;
            Check(r.x+r.width<=b.x || b.x+b.width<=r.x || r.y+r.height<=b.y || b.y+b.height<=r.y,"sale no overlap");
        }
        if (state.confirming && item.enabled && item.request.action!=Action::None)
            Check(item.request.action==Action::SaleConfirm || item.request.action==Action::SaleCancel,"confirmation modal input");
    }
    Check(digits<=160 && icons<=View::kCapacity && view.count*2+4<=84,"draw pools");
    Check(view.Covers({0,0}),"no farm click-through");
    for (int i=0;i<4;++i) {
        const auto hit=view.Hit({300,250.0f+i*72});
        Check(hit.action==(state.confirming?Action::None:Action::SaleSelect),"sale selection hit");
        if (!state.confirming) Check(hit.argument==i,"selection identity");
    }
    const auto expected=state.editable && state.quote.Available() ? Action::SaleReview : Action::None;
    if (!state.confirming) Check(view.Hit({700,510}).action==expected,"disabled review gate");
    Check(view.Hit({1100,510}).action==Action::None,"half-open right edge");
}
}
int main() {
    using Crop=farm::CropType;
    FarmEconomySystem economy; economy.Initialize();
    FarmSaleReviewSystem review;
    Check(!review.Begin(economy) && !review.Confirm(economy).Succeeded(),"empty and double confirm rejected");
    Check(!review.Select(static_cast<Crop>(999)),"invalid crop rejected");
    Check(economy.AddHarvest(Quality(Crop::Carrot),2,8),"carrots");
    Check(economy.AddHarvest(Quality(Crop::Tomato),1,8),"tomatoes");
    Check(economy.SetHarvestProtection(economy.GetHarvestRecord(1)->id,true,economy.GetInventoryGeneration()),"protect tomatoes");
    Check(economy.SetContestReservation(economy.GetHarvestRecord(1)->id,true,economy.GetInventoryGeneration()),"reserve tomatoes");
    const auto original=review.Current(economy);
    Check(original.count==2 && original.value==300 && original.protectedCount==1,"economy excludes protected harvests");
    Check(review.Begin(economy),"review begins");
    Check(!review.Select(Crop::Tomato) && !review.Begin(economy),"confirmation locks quote");
    review.Cancel(); Check(review.Current(economy)==original,"cancel never sells");
    Check(review.Begin(economy),"review again");
    const auto sold=review.Confirm(economy);
    Check(sold.soldCount==2 && sold.earnedMoney==300 && economy.GetMoney()==original.money+300,"exact sale");
    Check(economy.GetCropCount(Crop::Tomato)==1 && economy.GetContestReservation()!=nullptr,"reservation survives sale");
    Check(!review.Confirm(economy).Succeeded() && !review.Begin(economy),"no duplicate sale or sale of only protected crops");
    Check(economy.AddHarvest(Quality(Crop::Carrot),1,9),"stale fixture");
    Check(review.Begin(economy),"stale review");
    Check(economy.AddHarvest(Quality(Crop::Carrot),1,9),"change inventory");
    const auto staleMoney=economy.GetMoney();
    Check(!review.Confirm(economy).Succeeded() && economy.GetMoney()==staleMoney &&
        review.GetNotice()==FarmSaleReviewSystem::Notice::Changed,"stale quantity rejected");
    Check(review.Begin(economy),"reload review");
    Check(economy.RestoreSnapshot(economy.CaptureSnapshot()),"reload identical snapshot");
    Check(!review.Confirm(economy).Succeeded(),"generation change rejected");
    Check(review.Begin(economy),"money review");
    Check(economy.BuySeed(Crop::Carrot).Succeeded(),"money changes");
    Check(!review.Confirm(economy).Succeeded(),"money change rejected");
    Check(review.Select(Crop::Carrot) && review.Begin(economy),"select crop");
    Check(economy.SetHarvestProtection(economy.GetHarvestRecord(1)->id,true,economy.GetInventoryGeneration()),"protect during confirmation");
    Check(!review.Confirm(economy).Succeeded(),"protection change rejected");
    review.Reset(); Check(review.Selected()==Crop::None && !review.Confirming(),"reset discards quote");
    auto snapshot=economy.CaptureSnapshot(); snapshot.money=(std::numeric_limits<int>::max)();
    Check(economy.RestoreSnapshot(snapshot) && !review.Begin(economy),"money overflow guarded by economy");
    int combinations=0;
    for (bool confirming:{false,true}) for (bool editable:{false,true}) for (int selected=0;selected<4;++selected)
        for (int count:{0,1,99,2147483647}) for (int notice=0;notice<3;++notice) {
            farmui::SaleViewState state;
            state.confirming=confirming; state.editable=editable;
            state.quote={selected==0?Crop::None:farm::PlayableCrop(selected-1),count,count,count,0,5};
            state.owned.fill(count); state.notice=static_cast<FarmSaleReviewSystem::Notice>(notice);
            Validate(state); ++combinations;
        }
    std::cout<<"PASS: sale atomicity/cancel/double/stale/reload/protection/reservation/overflow; "<<combinations<<" menu layouts\n";
}
