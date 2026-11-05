#pragma once
#include "farm/ui/FarmQualityView.h"
#include "farm/system/FarmSaleReviewSystem.h"

namespace farmui {
struct SaleViewState {
    static constexpr int kChoiceCount=1+farm::kPlayableCropCount;
    FarmSaleReviewSystem::Quote quote{};
    FarmSaleReviewSystem::Notice notice=FarmSaleReviewSystem::Notice::None;
    std::array<int,kChoiceCount> owned{}; // All, carrot, tomato, pumpkin; legacy crops remain included in All.
    bool confirming=false, editable=false;
};
inline void BuildSaleView(View& view,const SaleViewState& state) {
    view.Add(Label::TradeTitle,{176,130,450,40});
    view.Add(Label::TradeDetails,{710,130,386,44},{Action::HarvestDetails},!state.confirming);
    view.Add(Label::TradeOwned,{444,188,162,34});
    constexpr std::array names{Label::TradeAll,Label::Carrot,Label::Tomato,Label::Pumpkin};
    constexpr std::array icons{HUDIcon::HarvestBasket,HUDIcon::Carrot,HUDIcon::Tomato,HUDIcon::Pumpkin};
    static_assert(names.size()==SaleViewState::kChoiceCount && icons.size()==names.size());
    for (int i=0;i<SaleViewState::kChoiceCount;++i) {
        const auto crop=i==0 ? farm::CropType::None : farm::PlayableCrop(i-1);
        view.Add(names[i],{176,234.0f+72*i,430,60},{Action::SaleSelect,i},!state.confirming,state.quote.crop==crop);
        auto& item=view.items[view.count-1];
        item.icon=icons[i]; item.leadingIcon=true;
        item.value=std::to_string(state.owned[i]); item.valueOffset=270;
    }
    view.Add(state.quote.crop==farm::CropType::None ? Label::TradeAll : CropLabel(state.quote.crop),{650,188,446,40});
    view.Metric(Label::TradeCount,{650,244,446,38},260,std::to_string(state.quote.count));
    view.Metric(Label::TradeValue,{650,292,446,38},260,std::to_string(state.quote.value)+"G");
    view.Metric(Label::TradeProtected,{650,340,446,38},260,std::to_string(state.quote.protectedCount));
    view.Metric(Label::TradeRemaining,{650,388,446,38},260,
        state.quote.Available() ? std::to_string(static_cast<long long>(state.quote.money)+state.quote.value)+"G" : "--");
    view.Add(state.confirming ? Label::TradeQuestion : Label::TradeProtectionNote,{650,438,446,38});
    if (state.confirming) {
        view.Add(Label::TradeConfirm,{650,490,260,48},{Action::SaleConfirm},state.editable);
        view.Add(Label::Cancel,{928,490,168,48},{Action::SaleCancel});
    } else {
        view.Add(Label::TradeReview,{650,490,446,48},{Action::SaleReview},state.editable && state.quote.Available());
    }
    Label notice=Label::TradeProtectionNote;
    if (state.notice==FarmSaleReviewSystem::Notice::Sold) notice=Label::TradeSold;
    else if (state.notice==FarmSaleReviewSystem::Notice::Changed) notice=Label::TradeChanged;
    else if (!state.editable) notice=Label::TradeUnavailable;
    else if (!state.quote.Available()) notice=Label::TradeEmpty;
    else notice=Label::HarvestSaleNotice;
    view.Add(notice,{176,566,920,38});
}
}
