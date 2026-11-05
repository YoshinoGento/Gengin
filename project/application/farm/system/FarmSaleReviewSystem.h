#pragma once
#include "farm/system/FarmEconomySystem.h"

// Owns sale confirmation, not pricing. Economy remains the mutation authority.
class FarmSaleReviewSystem final {
public:
    enum class Notice { None, Sold, Changed };
    struct Quote {
        farm::CropType crop=farm::CropType::None;
        int count=0, value=0, protectedCount=0, money=0;
        std::uint64_t generation=0;
        bool operator==(const Quote&) const = default;
        [[nodiscard]] bool Available() const noexcept { return count>0 && value>0; }
    };
    void Reset() noexcept { selected_=farm::CropType::None; Cancel(); notice_=Notice::None; }
    bool Select(farm::CropType crop) noexcept {
        if (confirming_ || (crop!=farm::CropType::None && !farm::IsPlantableCrop(crop))) return false;
        selected_=crop; notice_=Notice::None; return true;
    }
    [[nodiscard]] farm::CropType Selected() const noexcept { return selected_; }
    [[nodiscard]] bool Confirming() const noexcept { return confirming_; }
    [[nodiscard]] Notice GetNotice() const noexcept { return notice_; }
    [[nodiscard]] Quote Current(const FarmEconomySystem& economy) const noexcept {
        const auto preview=economy.PreviewSale(selected_);
        return {selected_,preview.soldCount,preview.earnedMoney,preview.protectedCount,
            economy.GetMoney(),economy.GetInventoryGeneration()};
    }
    [[nodiscard]] const Quote& Frozen() const noexcept { return quote_; }
    bool Begin(const FarmEconomySystem& economy) noexcept {
        if (confirming_) return false;
        const auto quote=Current(economy);
        if (!quote.Available()) return false;
        quote_=quote; confirming_=true; notice_=Notice::None; return true;
    }
    void Cancel() noexcept { confirming_=false; quote_={}; }
    [[nodiscard]] FarmSaleResult Confirm(FarmEconomySystem& economy) noexcept {
        if (!confirming_) return {};
        const auto current=Current(economy);
        const bool unchanged=current==quote_;
        Cancel();
        if (!unchanged || !current.Available()) { notice_=Notice::Changed; return {}; }
        const auto result=selected_==farm::CropType::None ? economy.SellAll() : economy.SellCrop(selected_);
        notice_=result.Succeeded() ? Notice::Sold : Notice::Changed;
        return result;
    }
private:
    farm::CropType selected_=farm::CropType::None;
    bool confirming_=false;
    Quote quote_{};
    Notice notice_=Notice::None;
};
