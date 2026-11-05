#include "farm/ui/FarmSeedShopView.h"
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
void Check(bool condition, const char* message) {
    if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}
void ValidateView(const FarmSeedShopSystem& shop, const FarmEconomySystem& economy) {
    farmui::View view;
    farmui::BuildSeedShopView(view, shop, economy);
    Check(view.modal && view.seedShop && view.count < view.kCapacity, "shop owns input and bounded view");
    Check(view.Covers({1, 400}), "background consumes clicks");
    const bool browsing = shop.GetPhase() == FarmSeedShopSystem::Phase::Browsing;
    for (int i=0;i<FarmSeedShopSystem::kProductCount;++i) {
        const auto hit=view.Hit({200.0f+220*i,112});
        Check(hit.action==(browsing?farmui::Action::ShopProduct:farmui::Action::None),"direct product tabs and confirmation lock");
        if (browsing) Check(hit.argument==i,"tab product identity");
    }
    const int first=(shop.Selection()/2)*2;
    for (int index = 0; index < 2; ++index) {
        const auto& rect = farmui::SeedShopLayout::products[index];
        if(first+index>=FarmSeedShopSystem::kProductCount) {
            Check(view.Hit({rect.x+100,rect.y+100}).action==farmui::Action::None,"empty product slot cannot select");
            continue;
        }
        const auto hit = view.Hit({rect.x + 100, rect.y + 100});
        Check(hit.action == (browsing ? farmui::Action::ShopSelect : farmui::Action::None), "packet hit and confirm locking");
        if (browsing) Check(hit.argument == first+index, "correct product");
        const auto sign = farmui::SeedShopLayout::SignBoard(index);
        Check(sign.y+sign.height < farmui::SeedShopLayout::footer.y, "sign clears footer");
        for (float y : {470.f, 518.f}) {
            const auto signHit=view.Hit({rect.x+160,y});
            Check(signHit.action==(browsing ? farmui::Action::ShopSelect : farmui::Action::None), "name/price hit respects confirm lock");
            if(browsing) Check(signHit.argument==first+index,"sign selects matching packet");
        }
        const auto position = farmui::SeedShopLayout::AtScreen(rect.x+120, farmui::SeedShopLayout::packetCenterY);
        Check(std::isfinite(position.x) && std::isfinite(position.y), "finite render placement");
        for (const bool selected : {false, true}) {
            const auto front = farmui::SeedShopLayout::PacketFront(index, selected);
            Check(front.width / front.height == 0.75f, "packet artwork keeps aspect ratio");
            Check(rect.Contains({front.x, front.y}) && rect.Contains({front.x+front.width-1, front.y+front.height-1}), "entire packet inside product hit target");
            Check(front.y+front.height+5 <=454, "packet shadow clears product name");
        }
    }
    for (std::size_t i = 0; i < view.count; ++i) {
        const auto& a = view.items[i];
        if (a.request.action == farmui::Action::None && (a.rect.y==454 || a.rect.y==498))
            Check(a.darkInk,"sign name and price always use dark ink");
        Check(a.rect.x >= 0 && a.rect.y >= 0 && a.rect.x+a.rect.width <=1280 && a.rect.y+a.rect.height <=720, "viewport bounds");
        if (a.request.action != farmui::Action::ShopSelect) {
            const float space = a.value.empty() ? a.rect.width-20 : a.valueOffset-30;
            Check(space / farmui::kLabels[static_cast<std::size_t>(a.label)].width >=20.f/26.f, "readable Japanese size");
            if (!a.value.empty()) Check(a.valueOffset+14*(a.value.size()-1)+24 <= a.rect.width, "numeric width");
        }
        for (std::size_t j = i+1; j < view.count; ++j) {
            const auto& b = view.items[j];
            if (a.request.action==farmui::Action::ShopSelect && b.request.action==farmui::Action::None &&
                a.rect.Contains({b.rect.x,b.rect.y}) && a.rect.Contains({b.rect.x+b.rect.width-1,b.rect.y+b.rect.height-1})) continue;
            Check(a.rect.x+a.rect.width<=b.rect.x || b.rect.x+b.rect.width<=a.rect.x ||
                a.rect.y+a.rect.height<=b.rect.y || b.rect.y+b.rect.height<=a.rect.y, "no overlapping labels/buttons");
        }
    }
}
}
int main() {
    for (int index : {-1, 2, (std::numeric_limits<int>::max)()}) {
        Check(farmui::SeedShopLayout::SignBoard(index).width == 0, "invalid sign index rejected");
        Check(farmui::SeedShopLayout::SelectionTarget(index).width == 0, "invalid selection index rejected");
    }
    FarmEconomySystem economy; economy.Initialize();
    FarmSeedShopSystem shop;
    const auto money = economy.GetMoney();
    Check(!shop.Confirm(economy).Succeeded() && !shop.Select(0), "closed shop cannot act");
    shop.Open(farm::CropType::TestCrop);
    Check(shop.Quantity()==1 && shop.Selection()==0, "default selection");
    Check(!shop.Select(-1) && !shop.Select(3) && !shop.ChangeQuantity(0) && !shop.ChangeQuantity(999), "bad input rejected");
    Check(!shop.ChangeQuantity(-1), "minimum quantity");
    for(int product=0;product<FarmSeedShopSystem::kProductCount;++product) {
        Check(shop.Select(product),"select formal crop"); ValidateView(shop,economy);
    }
    Check(shop.Select(0) && shop.ChangeQuantity(1), "browse carrot, quantity two");
    Check(economy.GetMoney()==money && economy.GetTotalSeedCount()==0, "browse never buys");
    ValidateView(shop,economy);
    Check(shop.BeginConfirmation(economy), "begin confirmation");
    ValidateView(shop,economy);
    Check(!shop.Select(0) && !shop.ChangeQuantity(1) && !shop.BeginConfirmation(economy), "quote locked");
    shop.CancelConfirmation();
    Check(economy.GetMoney()==money, "cancel no mutation");
    Check(shop.BeginConfirmation(economy), "confirm again");
    const int expectedCost=2*economy.GetSeedPrice(farm::CropType::Carrot);
    const auto purchased=shop.Confirm(economy);
    Check(purchased.Succeeded() && purchased.purchasedCount==2 && economy.GetMoney()==money-expectedCost &&
        economy.GetSeedCount(farm::CropType::Carrot)==2, "exact atomic purchase");
    Check(!shop.Confirm(economy).Succeeded() && economy.GetSeedCount(farm::CropType::Carrot)==2, "no double confirm");
    ValidateView(shop,economy);
    shop.Open(farm::CropType::TestCrop);
    Check(shop.BeginConfirmation(economy), "stale quote setup");
    Check(economy.BuySeed(farm::CropType::TestCrop).Succeeded(), "external purchase");
    const int staleMoney= economy.GetMoney();
    Check(!shop.Confirm(economy).Succeeded() && economy.GetMoney()==staleMoney &&
        shop.GetNotice()==FarmSeedShopSystem::Notice::Changed, "changed quote rejected");
    for (int i=1;i<FarmSeedShopSystem::kMaxQuantity;++i) Check(shop.ChangeQuantity(1), "increase to max");
    Check(!shop.ChangeQuantity(1) && !shop.BeginConfirmation(economy), "max and funds guards");
    ValidateView(shop,economy);
    shop.Close(); Check(!shop.IsOpen() && !shop.Confirm(economy).Succeeded(), "close clears confirmation");
    auto snapshot=economy.CaptureSnapshot();
    snapshot.money=(std::numeric_limits<int>::max)();
    snapshot.seedPrices[farm::ToCropSlot(farm::CropType::Carrot)]=(std::numeric_limits<int>::max)();
    Check(economy.RestoreSnapshot(snapshot), "extreme valid fixture");
    shop.Open(farm::CropType::TestCrop); shop.ChangeQuantity(1);
    Check(!shop.Evaluate(economy).available && !shop.BeginConfirmation(economy), "cost overflow rejected");
    ValidateView(shop,economy);
    snapshot.seedPrices[farm::ToCropSlot(farm::CropType::Carrot)]=60;
    snapshot.seedCounts[farm::ToCropSlot(farm::CropType::Carrot)]=(std::numeric_limits<int>::max)();
    Check(economy.RestoreSnapshot(snapshot), "inventory max fixture");
    Check(!shop.Evaluate(economy).available && !shop.BeginConfirmation(economy), "inventory overflow rejected");
    ValidateView(shop,economy);
    std::cout << "Seed shop: selection, quote/cancel/purchase, stale/double-confirm, bounds/overflow, layout PASS\n";
}
