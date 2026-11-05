#pragma once
#include "farm/ui/FarmRuntimeUI.h"
#include "farm/system/FarmSeedShopSystem.h"
#include "farm/ui/FarmQualityView.h"

namespace farmui {
// Virtual 1280x720 framing is shared with the fixed shop camera and hit targets.
struct SeedShopLayout {
    inline static constexpr std::array<Rect, 2> products{{{190, 168, 240, 286}, {520, 168, 240, 286}}};
    inline static constexpr Rect receipt{866, 150, 382, 364};
    inline static constexpr Rect footer{32, 544, 1216, 152};
    inline static constexpr Rect shopReceipt{866, 150, 382, 434};
    inline static constexpr Rect shopFooter{32, 600, 1216, 96};
    inline static constexpr float cameraDistance = 12.0f;
    inline static constexpr float fovY = 0.75f;
    inline static constexpr float packetCenterY = 342.0f;
    [[nodiscard]] static Rect SignBoard(int index) noexcept {
        if (index < 0 || index >= static_cast<int>(products.size())) return {};
        return {products[index].x - 8, 450, 316, 92};
    }
    [[nodiscard]] static Rect SelectionTarget(int index, bool harvest = false) noexcept {
        if (index < 0 || index >= static_cast<int>(products.size())) return {};
        const float top = harvest ? 144.0f : 168.0f;
        return {products[index].x - 8, top, 316, 542 - top};
    }
    [[nodiscard]] static Rect PacketFront(int index, bool selected) noexcept {
        if (index < 0 || index >= static_cast<int>(products.size())) return {};
        return {products[index].x + 30, selected ? 196.0f : 208.0f, 180, 240};
    }
    [[nodiscard]] static Vector3 AtScreen(float x, float y, float z = 0) noexcept {
        const float unit = (cameraDistance + z) * std::tan(fovY * 0.5f) / 360.0f;
        return {(x - 640.0f) * unit, (360.0f - y) * unit, z};
    }
};
inline void BuildSeedShopView(View& view, const FarmSeedShopSystem& shop, const FarmEconomySystem& economy) {
    if (!shop.IsOpen()) return;
    view.modal = true; view.seedShop = true;
    const auto quote = shop.Evaluate(economy);
    const bool confirming = shop.GetPhase() == FarmSeedShopSystem::Phase::Confirming;
    view.Add(Label::ShopTitle, {32, 24, 280, 44});
    view.Add(Label::Paused, {350, 24, 240, 44});
    view.Metric(Label::ShopMoney, {866, 24, 382, 44}, 136, std::to_string(quote.money) + "G");
    for (int product=0;product<FarmSeedShopSystem::kProductCount;++product)
        view.Add(CropLabel(FarmSeedShopSystem::Product(product)), {182.0f+220*product,96,208,44},
            {Action::ShopProduct,product}, !confirming, shop.Selection()==product);
    const int first = (shop.Selection() / 2) * 2;
    for (int i = 0; i < 2 && first+i < FarmSeedShopSystem::kProductCount; ++i) {
        const int product = first+i;
        const auto crop = FarmSeedShopSystem::Product(product);
        const auto& rect = SeedShopLayout::products[i];
        view.Add(CropLabel(crop), SeedShopLayout::SelectionTarget(i), {Action::ShopSelect, product}, !confirming, shop.Selection() == product);
        view.Add(CropLabel(crop), {rect.x, 454, 240, 40}, {}, true, shop.Selection() == product);
        view.items[view.count-1].darkInk = true;
        const bool available = economy.IsCropUnlocked(crop);
        view.Metric(available ? Label::ShopUnitPrice : Label::StageUnlockDay, {rect.x, 498, 300, 38}, 100,
            available ? std::to_string(economy.GetSeedPrice(crop)) + "G" : std::to_string(FarmStageAccessSystem::UnlockDay(crop)));
        view.items[view.count-1].darkInk = true;
    }
    constexpr std::array<Label,3> titles{Label::ShopCarrot, Label::ShopTomato, Label::ShopPumpkin};
    view.Add(titles[shop.Selection()], {882, 164, 346, 42});
    view.Metric(Label::ShopOwned, {882, 218, 346, 38}, 174, std::to_string(quote.owned));
    view.Metric(Label::ShopUnitPrice, {882, 266, 346, 38}, 174, std::to_string(quote.price) + "G");
    view.Metric(Label::ShopQuantity, {882, 314, 346, 38}, 174, std::to_string(quote.quantity));
    view.Metric(Label::ShopTotal, {882, 414, 346, 38}, 174,
        quote.total > (std::numeric_limits<int>::max)() ? "--" : std::to_string(quote.total) + "G");
    view.Metric(Label::ShopRemaining, {882, 462, 346, 38}, 174,
        quote.available ? std::to_string(quote.money-quote.total)+"G" : "--");
    view.Add(Label::ShopMinus, {888, 356, 156, 44}, {Action::ShopQuantity, -1}, !confirming && shop.Quantity() > 1);
    view.Add(Label::ShopPlus, {1060, 356, 156, 44}, {Action::ShopQuantity, 1}, !confirming && shop.Quantity() < FarmSeedShopSystem::kMaxQuantity);
    Label notice = confirming ? Label::ShopConfirm : Label::ShopChoose;
    if (!confirming) {
        if (shop.GetNotice() == FarmSeedShopSystem::Notice::Purchased) notice = Label::ShopPurchased;
        else if (shop.GetNotice() == FarmSeedShopSystem::Notice::Changed) notice = Label::ShopChanged;
        else if (shop.GetNotice() == FarmSeedShopSystem::Notice::Failed) notice = Label::Failure;
        else if (!quote.available) notice = quote.total > quote.money ? Label::ShopNoMoney : Label::ShopUnavailable;
    }
    if (quote.unlockDay > 0) view.Metric(Label::StageUnlockDay, {48,620,810,42}, 200, std::to_string(quote.unlockDay));
    else view.Add(notice, {48, 620, 810, 42});
    view.Add(confirming ? Label::ShopPay : Label::ShopReview, {882, 518, 346, 50},
        {confirming ? Action::ShopConfirm : Action::ShopReview}, quote.available);
    view.Add(confirming ? Label::Cancel : Label::Resume, {928, 620, 300, 52},
        {confirming ? Action::ShopCancel : Action::ShopClose});
}
}
