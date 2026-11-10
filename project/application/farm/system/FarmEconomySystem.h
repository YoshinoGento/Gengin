#pragma once

#include "farm/core/FarmTypes.h"
#include "farm/data/FarmRules.h"
#include "farm/system/FarmCropQualitySystem.h"
#include "farm/system/FarmStageAccessSystem.h"
#include "farm/system/FarmCropRecordSystem.h"

#include <array>
#include <cstdint>

struct FarmSaleResult {
	farm::CropType crop = farm::CropType::None;
	int soldCount = 0;
	int earnedMoney = 0;
	int protectedCount = 0;

	[[nodiscard]] bool Succeeded() const noexcept {
		return soldCount > 0 && earnedMoney > 0;
	}
};

enum class FarmSeedPurchaseStatus {
	None,
	Purchased,
	UnsupportedCrop,
	InvalidQuantity,
	Unavailable,
	InsufficientMoney,
	Overflow,
	Locked,
};

struct FarmSeedPurchaseResult {
	FarmSeedPurchaseStatus status = FarmSeedPurchaseStatus::None;
	farm::CropType crop = farm::CropType::None;
	int purchasedCount = 0;
	int spentMoney = 0;

	[[nodiscard]] bool Succeeded() const noexcept {
		return status == FarmSeedPurchaseStatus::Purchased &&
			purchasedCount > 0 && spentMoney > 0;
	}
};

class FarmEconomySystem final {
public:
	static constexpr std::size_t kMaxHarvestRecords = 128;
	struct HarvestRecord {
		FarmCropQualityResult quality{};
		int quantity = 0;
		int id = 0;
		bool saleProtected = false;
		int harvestedDay = 0; // 0 means unknown, including legacy saves.
	};
	using HarvestRecords = std::array<HarvestRecord, kMaxHarvestRecords>;
	struct ContestResult {
		HarvestRecord harvest{};
		int contestDay = 0; // 0 is an unused slot; other fields are ignored until populated.
		int qualityPoints = 0;
		int sizePoints = 0;
	};
	using ContestResults = std::array<ContestResult, 3>;
	struct Snapshot {
		int money = 0;
		std::array<int, farm::kFarmCropTypeCount> cropCounts{};
		std::array<int, farm::kFarmCropTypeCount> cropValues{};
		std::array<int, farm::kFarmCropTypeCount> sellPrices{};
		std::array<int, farm::kFarmCropTypeCount> seedCounts{};
		std::array<int, farm::kFarmCropTypeCount> seedPrices{};
		FarmCropQualityResult lastHarvestQuality{};
		HarvestRecords harvestRecords{};
		std::size_t harvestRecordCount = 0;
		int nextHarvestRecordId = 1;
		int contestReservationId = 0;
		ContestResults contestResults{};
		int lifetimeSales = 0;
		FarmCropRecordSystem::Records soldCropRecords{};
		int lastRecoveryDay = 0; // 0 means no aid received in this farm.
	};

	void Initialize(const farm::FarmRules& rules = {});
	void BindStageAccess(const FarmStageAccessSystem& access) noexcept { stageAccess_=&access; }
	[[nodiscard]] bool IsCropUnlocked(farm::CropType crop) const noexcept {
		return (stageAccess_ ? stageAccess_->Get() : FarmStageAccess{}).Allows(crop);
	}

	[[nodiscard]] bool AddHarvest(farm::CropType crop, int quantity = 1) noexcept;
	[[nodiscard]] bool RemoveHarvest(farm::CropType crop, int quantity = 1) noexcept;
	[[nodiscard]] bool AddHarvest(
		const FarmCropQualityResult& quality, int quantity = 1, int harvestedDay = 0) noexcept;
	[[nodiscard]] bool RemoveHarvest(
		const FarmCropQualityResult& quality, int quantity = 1) noexcept;
	[[nodiscard]] bool RemoveHarvest(
		const FarmCropQualityResult& quality, int quantity,
		const FarmCropQualityResult& restoredLastHarvestQuality) noexcept;
	[[nodiscard]] bool AddSeed(farm::CropType crop, int quantity = 1) noexcept;
	[[nodiscard]] bool RemoveSeed(farm::CropType crop, int quantity = 1) noexcept;
	[[nodiscard]] FarmSeedPurchaseResult BuySeed(
		farm::CropType crop, int quantity = 1) noexcept;
	[[nodiscard]] FarmSaleResult SellCrop(farm::CropType crop) noexcept;
	[[nodiscard]] FarmSaleResult SellAll() noexcept;
	[[nodiscard]] FarmSaleResult PreviewSale(farm::CropType crop = farm::CropType::None) const noexcept;
	[[nodiscard]] int GetProtectedCropCount() const noexcept;
	[[nodiscard]] std::uint64_t GetInventoryGeneration() const noexcept { return inventoryGeneration_; }
	[[nodiscard]] bool SetHarvestProtection(int id, bool protect, std::uint64_t generation) noexcept;
	[[nodiscard]] static bool CanReserveForContest(const HarvestRecord& record) noexcept;
	[[nodiscard]] bool SetContestReservation(int id, bool reserve, std::uint64_t generation) noexcept;
	[[nodiscard]] int GetContestReservationId() const noexcept { return contestReservationId_; }
	[[nodiscard]] const HarvestRecord* GetContestReservation() const noexcept;
	[[nodiscard]] const ContestResults& GetContestResults() const noexcept { return contestResults_; }

	[[nodiscard]] int GetMoney() const noexcept { return money_; }
	[[nodiscard]] int GetLastRecoveryDay() const noexcept { return lastRecoveryDay_; }
	[[nodiscard]] int GetLifetimeSales() const noexcept { return lifetimeSales_; }
	[[nodiscard]] const FarmCropRecordSystem::Records& GetSoldCropRecords() const noexcept { return soldCropRecords_; }
	[[nodiscard]] int GetCropCount(farm::CropType crop) const noexcept;
	[[nodiscard]] int GetTotalCropCount() const noexcept;
	[[nodiscard]] int GetSellPrice(farm::CropType crop) const noexcept;
	[[nodiscard]] int GetCropInventoryValue(farm::CropType crop) const noexcept;
	[[nodiscard]] int GetTestCropSellPrice() const noexcept {
		return GetSellPrice(farm::CropType::TestCrop);
	}
	[[nodiscard]] int GetSeedCount(farm::CropType crop) const noexcept;
	[[nodiscard]] int GetTotalSeedCount() const noexcept;
	[[nodiscard]] int GetSeedPrice(farm::CropType crop) const noexcept;
	[[nodiscard]] int GetSalePreviewValue() const noexcept;
	[[nodiscard]] const FarmCropQualityResult& GetLastHarvestQuality() const noexcept {
		return lastHarvestQuality_;
	}
	[[nodiscard]] Snapshot CaptureSnapshot() const noexcept;
	[[nodiscard]] static bool ValidateSnapshot(const Snapshot& snapshot) noexcept;
	[[nodiscard]] static bool IsRecordedQualityValid(const FarmCropQualityResult& quality) noexcept;
	[[nodiscard]] std::size_t GetHarvestRecordCount() const noexcept { return harvestRecordCount_; }
	[[nodiscard]] const HarvestRecord* GetHarvestRecord(std::size_t index) const noexcept {
		return index < harvestRecordCount_ ? &harvestRecords_[index] : nullptr;
	}
	[[nodiscard]] int GetUnrecordedCropCount() const noexcept;
	bool RestoreSnapshot(const Snapshot& snapshot) noexcept;

private:
	friend class FarmRecoverySystem;
	[[nodiscard]] bool GrantRecoverySeed(farm::CropType crop, int quantity, int day) noexcept;
	int lastRecoveryDay_ = 0;
	const FarmStageAccessSystem* stageAccess_=nullptr; // Borrowed for the owning Scene's lifetime; not saved.
	[[nodiscard]] FarmSaleResult SellMatching(farm::CropType crop) noexcept;
	int money_ = 0;
	int lifetimeSales_ = 0;
	FarmCropRecordSystem::Records soldCropRecords_{};
	std::array<int, farm::kFarmCropTypeCount> cropCounts_{};
	std::array<int, farm::kFarmCropTypeCount> cropValues_{};
	std::array<int, farm::kFarmCropTypeCount> sellPrices_{};
	std::array<int, farm::kFarmCropTypeCount> seedCounts_{};
	std::array<int, farm::kFarmCropTypeCount> seedPrices_{};
	FarmCropQualityResult lastHarvestQuality_{};
	HarvestRecords harvestRecords_{};
	std::size_t harvestRecordCount_ = 0;
	int nextHarvestRecordId_ = 1;
	int contestReservationId_ = 0;
	ContestResults contestResults_{};
	std::uint64_t inventoryGeneration_ = 1;
};
