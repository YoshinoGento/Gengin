#include "farm/system/FarmEconomySystem.h"
#include "farm/system/FarmContestJudgeSystem.h"
#include "farm/system/FarmContestEntrySystem.h"

#include <algorithm>
#include <limits>
#include <cmath>

namespace {
int SaturatingTotal(const std::array<int, farm::kFarmCropTypeCount>& values) noexcept
{
	long long total = 0;
	for (const int value : values) {
		total += value;
	}
	return static_cast<int>((std::min)(
		total, static_cast<long long>((std::numeric_limits<int>::max)())));
}

bool AreValuesValid(const std::array<int, farm::kFarmCropTypeCount>& values) noexcept
{
	return std::all_of(values.begin(), values.end(), [](int value) { return value >= 0; });
}

bool TryMultiplyNonNegative(int left, int right, int& result) noexcept
{
	if (left < 0 || right < 0) {
		return false;
	}
	const long long product = static_cast<long long>(left) * right;
	if (product > (std::numeric_limits<int>::max)()) {
		return false;
	}
	result = static_cast<int>(product);
	return true;
}

bool SameQuality(const FarmCropQualityResult& a, const FarmCropQualityResult& b) noexcept
{
	return a.crop == b.crop && a.maturity == b.maturity && a.waterBalance == b.waterBalance &&
		a.terrainFit == b.terrainFit && a.nutrientBalance == b.nutrientBalance &&
		a.nutrientKnown == b.nutrientKnown && a.score == b.score && a.basePrice == b.basePrice &&
		a.salePrice == b.salePrice && a.harvestSize.known == b.harvestSize.known &&
		a.harvestSize.multiplier == b.harvestSize.multiplier && a.pestPenalty == b.pestPenalty;
}
}

void FarmEconomySystem::Initialize(const farm::FarmRules& rules)
{
	money_ = rules.initialMoney >= 0 ? rules.initialMoney : 0;
	lifetimeSales_ = 0;
	soldCropRecords_ = {};
	cropCounts_.fill(0);
	cropValues_.fill(0);
	sellPrices_ = {
		rules.normalHarvestPrice > 0 ? rules.normalHarvestPrice : 0,
		rules.carrotHarvestPrice > 0 ? rules.carrotHarvestPrice : 0,
		(std::max)(rules.tomatoHarvestPrice, 0), (std::max)(rules.pumpkinHarvestPrice, 0),
	};
	seedCounts_ = {
		rules.initialTestCropSeedCount >= 0 ? rules.initialTestCropSeedCount : 0,
		rules.initialCarrotSeedCount >= 0 ? rules.initialCarrotSeedCount : 0,
		(std::max)(rules.initialTomatoSeedCount, 0), (std::max)(rules.initialPumpkinSeedCount, 0),
	};
	lastRecoveryDay_ = 0;
	seedPrices_ = {
		rules.testCropSeedPrice > 0 ? rules.testCropSeedPrice : 0,
		rules.carrotSeedPrice > 0 ? rules.carrotSeedPrice : 0,
		(std::max)(rules.tomatoSeedPrice, 0), (std::max)(rules.pumpkinSeedPrice, 0),
	};
	lastHarvestQuality_ = {};
	harvestRecords_ = {};
	harvestRecordCount_ = 0;
	nextHarvestRecordId_ = 1;
	contestReservationId_ = 0;
	contestResults_ = {};
	++inventoryGeneration_;
}

bool FarmEconomySystem::AddHarvest(farm::CropType crop, int quantity) noexcept
{
	const int slot = farm::ToCropSlot(crop);
	int addedValue = 0;
	if (slot < 0 || quantity <= 0 ||
		!TryMultiplyNonNegative(sellPrices_[slot], quantity, addedValue) ||
		cropCounts_[slot] > (std::numeric_limits<int>::max)() - quantity ||
		cropValues_[slot] > (std::numeric_limits<int>::max)() - addedValue) {
		return false;
	}
	cropCounts_[slot] += quantity;
	cropValues_[slot] += addedValue;
	return true;
}

bool FarmEconomySystem::RemoveHarvest(farm::CropType crop, int quantity) noexcept
{
	const int slot = farm::ToCropSlot(crop);
	int removedValue = 0;
	if (slot < 0 || quantity <= 0 || quantity > cropCounts_[slot] ||
		!TryMultiplyNonNegative(sellPrices_[slot], quantity, removedValue) ||
		removedValue > cropValues_[slot]) {
		return false;
	}
	// The legacy API may remove only aggregate-only stock, never a recorded harvest.
	long long recordedCount = 0, recordedValue = 0;
	for (std::size_t i = 0; i < harvestRecordCount_; ++i) {
		const auto& record = harvestRecords_[i];
		if (record.quality.crop != crop) continue;
		recordedCount += record.quantity;
		recordedValue += static_cast<long long>(record.quantity) * record.quality.salePrice;
	}
	if (cropCounts_[slot] - recordedCount < quantity || cropValues_[slot] - recordedValue < removedValue)
		return false;
	if ((cropCounts_[slot] - recordedCount == quantity) != (cropValues_[slot] - recordedValue == removedValue))
		return false;
	cropCounts_[slot] -= quantity;
	cropValues_[slot] -= removedValue;
	return true;
}

bool FarmEconomySystem::AddHarvest(
	const FarmCropQualityResult& quality, int quantity, int harvestedDay) noexcept
{
	const int slot = farm::ToCropSlot(quality.crop);
	int addedValue = 0;
	if (!IsRecordedQualityValid(quality) || slot < 0 || quantity <= 0 || harvestedDay < 0 ||
		harvestRecordCount_ >= kMaxHarvestRecords ||
		nextHarvestRecordId_ == (std::numeric_limits<int>::max)() ||
		!TryMultiplyNonNegative(quality.salePrice, quantity, addedValue) ||
		cropCounts_[slot] > (std::numeric_limits<int>::max)() - quantity ||
		cropValues_[slot] > (std::numeric_limits<int>::max)() - addedValue) {
		return false;
	}
	cropCounts_[slot] += quantity;
	cropValues_[slot] += addedValue;
	lastHarvestQuality_ = quality;
	harvestRecords_[harvestRecordCount_++] = {quality, quantity, nextHarvestRecordId_++, false, harvestedDay};
	return true;
}

bool FarmEconomySystem::RemoveHarvest(
	const FarmCropQualityResult& quality, int quantity) noexcept
{
	return RemoveHarvest(quality, quantity, {});
}

bool FarmEconomySystem::RemoveHarvest(
	const FarmCropQualityResult& quality, int quantity,
	const FarmCropQualityResult& restoredLastHarvestQuality) noexcept
{
	const int slot = farm::ToCropSlot(quality.crop);
	int removedValue = 0;
	if (!IsRecordedQualityValid(quality) || slot < 0 || quantity <= 0 ||
		quantity > cropCounts_[slot] ||
		!TryMultiplyNonNegative(quality.salePrice, quantity, removedValue) ||
		removedValue > cropValues_[slot] ||
		(restoredLastHarvestQuality.crop != farm::CropType::None &&
			!IsRecordedQualityValid(restoredLastHarvestQuality))) {
		return false;
	}
	// Tile history is LIFO and is cleared by sales. Reject stale/mismatched removal.
	if (harvestRecordCount_ == 0) return false;
	auto& record = harvestRecords_[harvestRecordCount_ - 1];
	if (record.saleProtected || !SameQuality(record.quality, quality) || record.quantity < quantity) return false;
	record.quantity -= quantity;
	if (record.quantity == 0) harvestRecords_[--harvestRecordCount_] = {};
	cropCounts_[slot] -= quantity;
	cropValues_[slot] -= removedValue;
	lastHarvestQuality_ = restoredLastHarvestQuality;
	return true;
}

bool FarmEconomySystem::AddSeed(farm::CropType crop, int quantity) noexcept
{
	const int slot = farm::ToCropSlot(crop);
	if (slot < 0 || quantity <= 0 ||
		seedCounts_[slot] > (std::numeric_limits<int>::max)() - quantity) {
		return false;
	}
	seedCounts_[slot] += quantity;
	return true;
}

bool FarmEconomySystem::RemoveSeed(farm::CropType crop, int quantity) noexcept
{
	const int slot = farm::ToCropSlot(crop);
	if (slot < 0 || quantity <= 0 || quantity > seedCounts_[slot]) {
		return false;
	}
	seedCounts_[slot] -= quantity;
	return true;
}

FarmSeedPurchaseResult FarmEconomySystem::BuySeed(
	farm::CropType crop, int quantity) noexcept
{
	FarmSeedPurchaseResult result{};
	result.crop = crop;
	const int slot = farm::ToCropSlot(crop);
	if (slot < 0) {
		result.status = FarmSeedPurchaseStatus::UnsupportedCrop;
		return result;
	}
	if (quantity <= 0) {
		result.status = FarmSeedPurchaseStatus::InvalidQuantity;
		return result;
	}
	if (!IsCropUnlocked(crop)) { result.status = FarmSeedPurchaseStatus::Locked; return result; }
	if (seedPrices_[slot] <= 0) {
		result.status = FarmSeedPurchaseStatus::Unavailable;
		return result;
	}
	if (seedCounts_[slot] > (std::numeric_limits<int>::max)() - quantity) {
		result.status = FarmSeedPurchaseStatus::Overflow;
		return result;
	}

	const long long cost =
		static_cast<long long>(seedPrices_[slot]) * static_cast<long long>(quantity);
	if (cost > (std::numeric_limits<int>::max)()) {
		result.status = FarmSeedPurchaseStatus::Overflow;
		return result;
	}
	if (cost > money_) {
		result.status = FarmSeedPurchaseStatus::InsufficientMoney;
		return result;
	}

	money_ -= static_cast<int>(cost);
	seedCounts_[slot] += quantity;
	result.status = FarmSeedPurchaseStatus::Purchased;
	result.purchasedCount = quantity;
	result.spentMoney = static_cast<int>(cost);
	return result;
}

FarmSaleResult FarmEconomySystem::PreviewSale(farm::CropType crop) const noexcept
{
	FarmSaleResult result{}; result.crop = crop;
	if (crop != farm::CropType::None && !farm::IsPlantableCrop(crop)) return result;
	long long count = 0, value = 0, protectedCount = 0;
	for (int slot = 0; slot < farm::kFarmCropTypeCount; ++slot) {
		if (crop != farm::CropType::None && farm::CropTypeFromSlot(slot) != crop) continue;
		count += cropCounts_[slot]; value += cropValues_[slot];
	}
	for (std::size_t i = 0; i < harvestRecordCount_; ++i) {
		const auto& record = harvestRecords_[i];
		if (!record.saleProtected || (crop != farm::CropType::None && record.quality.crop != crop)) continue;
		count -= record.quantity;
		value -= static_cast<long long>(record.quantity) * record.quality.salePrice;
		protectedCount += record.quantity;
	}
	result.protectedCount = static_cast<int>((std::min)(protectedCount, static_cast<long long>((std::numeric_limits<int>::max)())));
	if (count <= 0 || value <= 0 || count > (std::numeric_limits<int>::max)() ||
		value > (std::numeric_limits<int>::max)() - static_cast<long long>(money_)) return result;
	result.soldCount = static_cast<int>(count); result.earnedMoney = static_cast<int>(value);
	return result;
}

FarmSaleResult FarmEconomySystem::SellMatching(farm::CropType crop) noexcept
{
	const auto result = PreviewSale(crop);
	if (!result.Succeeded()) return result;
	// Commit only after the complete sale has passed overflow and availability checks.
	for (int slot = 0; slot < farm::kFarmCropTypeCount; ++slot) {
		if (crop != farm::CropType::None && farm::CropTypeFromSlot(slot) != crop) continue;
		cropCounts_[slot] = 0; cropValues_[slot] = 0;
	}
	std::size_t remaining = 0;
	for (std::size_t i = 0; i < harvestRecordCount_; ++i) {
		const auto record = harvestRecords_[i];
		const bool matches = crop == farm::CropType::None || record.quality.crop == crop;
		if (matches && record.saleProtected) {
			const int slot = farm::ToCropSlot(record.quality.crop);
			cropCounts_[slot] += record.quantity;
			cropValues_[slot] += record.quantity * record.quality.salePrice;
		}
		if (!matches || record.saleProtected) harvestRecords_[remaining++] = record;
		else FarmCropRecordSystem::Accumulate(soldCropRecords_, record.quality);
	}
	std::fill(harvestRecords_.begin() + remaining, harvestRecords_.end(), HarvestRecord{});
	harvestRecordCount_ = remaining;
	money_ += result.earnedMoney;
	// Saturate the historical counter without blocking otherwise valid sales.
	lifetimeSales_ += (std::min)(result.earnedMoney, (std::numeric_limits<int>::max)() - lifetimeSales_);
	return result;
}

FarmSaleResult FarmEconomySystem::SellCrop(farm::CropType crop) noexcept
{
	if (!farm::IsPlantableCrop(crop)) return {};
	return SellMatching(crop);
}

FarmSaleResult FarmEconomySystem::SellAll() noexcept
{
	return SellMatching(farm::CropType::None);
}

int FarmEconomySystem::GetProtectedCropCount() const noexcept
{
	long long count = 0;
	for (std::size_t i = 0; i < harvestRecordCount_; ++i)
		if (harvestRecords_[i].saleProtected) count += harvestRecords_[i].quantity;
	return static_cast<int>((std::min)(count, static_cast<long long>((std::numeric_limits<int>::max)())));
}

bool FarmEconomySystem::SetHarvestProtection(int id, bool protect, std::uint64_t generation) noexcept
{
	if (id <= 0 || generation != inventoryGeneration_) return false;
	if (!protect && id == contestReservationId_) return false;
	for (std::size_t i = 0; i < harvestRecordCount_; ++i) {
		auto& record = harvestRecords_[i];
		if (record.id != id) continue;
		if (record.saleProtected == protect) return false;
		record.saleProtected = protect;
		return true;
	}
	return false;
}

bool FarmEconomySystem::CanReserveForContest(const HarvestRecord& record) noexcept
{
	return record.id > 0 && record.quantity == 1 && record.saleProtected &&
		IsRecordedQualityValid(record.quality) && record.quality.harvestSize.known;
}

const FarmEconomySystem::HarvestRecord* FarmEconomySystem::GetContestReservation() const noexcept
{
	if (contestReservationId_ == 0) return nullptr;
	for (std::size_t i = 0; i < harvestRecordCount_; ++i)
		if (harvestRecords_[i].id == contestReservationId_) return &harvestRecords_[i];
	return nullptr;
}

bool FarmEconomySystem::SetContestReservation(int id, bool reserve, std::uint64_t generation) noexcept
{
	if (id <= 0 || generation != inventoryGeneration_) return false;
	if (!reserve) {
		if (contestReservationId_ != id) return false;
		contestReservationId_ = 0;
		return true;
	}
	if (contestReservationId_ == id) return false;
	for (std::size_t i = 0; i < harvestRecordCount_; ++i) {
		const auto& record = harvestRecords_[i];
		if (record.id != id) continue;
		if (!CanReserveForContest(record)) return false;
		// Replacing the reservation never removes protection from the previous crop.
		contestReservationId_ = id;
		return true;
	}
	return false;
}

int FarmEconomySystem::GetCropCount(farm::CropType crop) const noexcept
{
	const int slot = farm::ToCropSlot(crop);
	return slot >= 0 ? cropCounts_[slot] : 0;
}

int FarmEconomySystem::GetTotalCropCount() const noexcept
{
	return SaturatingTotal(cropCounts_);
}

int FarmEconomySystem::GetSellPrice(farm::CropType crop) const noexcept
{
	const int slot = farm::ToCropSlot(crop);
	return slot >= 0 ? sellPrices_[slot] : 0;
}

int FarmEconomySystem::GetCropInventoryValue(farm::CropType crop) const noexcept
{
	const int slot = farm::ToCropSlot(crop);
	return slot >= 0 ? cropValues_[slot] : 0;
}

int FarmEconomySystem::GetSeedCount(farm::CropType crop) const noexcept
{
	const int slot = farm::ToCropSlot(crop);
	return slot >= 0 ? seedCounts_[slot] : 0;
}

int FarmEconomySystem::GetTotalSeedCount() const noexcept
{
	return SaturatingTotal(seedCounts_);
}

int FarmEconomySystem::GetSeedPrice(farm::CropType crop) const noexcept
{
	const int slot = farm::ToCropSlot(crop);
	return slot >= 0 ? seedPrices_[slot] : 0;
}

int FarmEconomySystem::GetSalePreviewValue() const noexcept
{
	return PreviewSale().earnedMoney;
}

FarmEconomySystem::Snapshot FarmEconomySystem::CaptureSnapshot() const noexcept
{
	return {
		money_, cropCounts_, cropValues_, sellPrices_, seedCounts_, seedPrices_,
		lastHarvestQuality_, harvestRecords_, harvestRecordCount_, nextHarvestRecordId_, contestReservationId_, contestResults_, lifetimeSales_, soldCropRecords_, lastRecoveryDay_
	};
}

bool FarmEconomySystem::GrantRecoverySeed(farm::CropType crop, int quantity, int day) noexcept
{
	// AddSeed validates capacity. Commit the daily limit and invalidate outstanding inventory requests together.
	if (day <= 0 || lastRecoveryDay_ >= day || !IsCropUnlocked(crop) || !AddSeed(crop, quantity)) return false;
	lastRecoveryDay_ = day;
	++inventoryGeneration_;
	return true;
}

bool FarmEconomySystem::IsRecordedQualityValid(const FarmCropQualityResult& quality) noexcept
{
	const auto unit = [](float value) { return std::isfinite(value) && value >= 0 && value <= 1; };
	return quality.IsValid() && unit(quality.maturity) && unit(quality.waterBalance) &&
		unit(quality.terrainFit) && unit(quality.nutrientBalance) && quality.score >= 0 && quality.score <= 100;
}

int FarmEconomySystem::GetUnrecordedCropCount() const noexcept
{
	long long count = 0;
	for (int value : cropCounts_) count += value;
	for (std::size_t i = 0; i < harvestRecordCount_; ++i) count -= harvestRecords_[i].quantity;
	return static_cast<int>((std::min)(count, static_cast<long long>((std::numeric_limits<int>::max)())));
}

bool FarmEconomySystem::ValidateSnapshot(const Snapshot& snapshot) noexcept
{
	if (snapshot.lastRecoveryDay < 0 || snapshot.money < 0 || snapshot.lifetimeSales < 0 || !FarmCropRecordSystem::Validate(snapshot.soldCropRecords) || !AreValuesValid(snapshot.cropCounts) ||
		!AreValuesValid(snapshot.cropValues) ||
		!AreValuesValid(snapshot.sellPrices) || !AreValuesValid(snapshot.seedCounts) ||
		!AreValuesValid(snapshot.seedPrices) ||
		(snapshot.lastHarvestQuality.crop != farm::CropType::None &&
			!IsRecordedQualityValid(snapshot.lastHarvestQuality)) ||
		snapshot.harvestRecordCount > kMaxHarvestRecords || snapshot.nextHarvestRecordId <= 0 ||
		snapshot.contestReservationId < 0) {
		return false;
	}
	std::array<long long, farm::kFarmCropTypeCount> counts{}, values{};
	bool reservationValid = snapshot.contestReservationId == 0;
	for (std::size_t i = 0; i < snapshot.harvestRecordCount; ++i) {
		const auto& record = snapshot.harvestRecords[i];
		if (record.quantity <= 0 || record.harvestedDay < 0 || !IsRecordedQualityValid(record.quality) ||
			record.id <= 0 || record.id >= snapshot.nextHarvestRecordId) return false;
		for (std::size_t j = 0; j < i; ++j) if (snapshot.harvestRecords[j].id == record.id) return false;
		if (record.id == snapshot.contestReservationId) reservationValid = CanReserveForContest(record);
		const int slot = farm::ToCropSlot(record.quality.crop);
		counts[slot] += record.quantity;
		values[slot] += static_cast<long long>(record.quantity) * record.quality.salePrice;
		if (counts[slot] > snapshot.cropCounts[slot] || values[slot] > snapshot.cropValues[slot]) return false;
	}
	for (int slot = 0; slot < farm::kFarmCropTypeCount; ++slot)
		if ((snapshot.cropCounts[slot] == counts[slot]) != (snapshot.cropValues[slot] == values[slot])) return false;
	for (std::size_t i = 0; i < snapshot.contestResults.size(); ++i) {
		const auto& result = snapshot.contestResults[i];
		if (result.contestDay == 0) continue;
		const auto& harvest = result.harvest;
		if (result.contestDay != FarmContestEntrySystem::kContestDays[i] || harvest.id >= snapshot.nextHarvestRecordId ||
			FarmContestEntrySystem::Evaluate(result.contestDay, &harvest).issue != FarmContestEntryIssue::Eligible) return false;
		const auto judge = FarmContestJudgeSystem::Evaluate(&harvest, kFarmContestSubmissionRulesV1);
		if (!judge.IsValid() || result.qualityPoints != judge.qualityPoints || result.sizePoints != judge.sizePoints) return false;
		for (std::size_t j = 0; j < snapshot.harvestRecordCount; ++j)
			if (snapshot.harvestRecords[j].id == harvest.id) return false;
		for (std::size_t j = 0; j < i; ++j)
			if (snapshot.contestResults[j].contestDay && snapshot.contestResults[j].harvest.id == harvest.id) return false;
	}
	return reservationValid;
}

bool FarmEconomySystem::RestoreSnapshot(const Snapshot& snapshot) noexcept
{
	if (!ValidateSnapshot(snapshot)) return false;
	money_ = snapshot.money;
	lastRecoveryDay_ = snapshot.lastRecoveryDay;
	lifetimeSales_ = snapshot.lifetimeSales;
	soldCropRecords_ = snapshot.soldCropRecords;
	cropCounts_ = snapshot.cropCounts;
	cropValues_ = snapshot.cropValues;
	sellPrices_ = snapshot.sellPrices;
	seedCounts_ = snapshot.seedCounts;
	seedPrices_ = snapshot.seedPrices;
	lastHarvestQuality_ = snapshot.lastHarvestQuality;
	harvestRecords_ = snapshot.harvestRecords;
	harvestRecordCount_ = snapshot.harvestRecordCount;
	nextHarvestRecordId_ = snapshot.nextHarvestRecordId;
	contestReservationId_ = snapshot.contestReservationId;
	contestResults_ = snapshot.contestResults;
	++inventoryGeneration_;
	std::fill(harvestRecords_.begin() + harvestRecordCount_, harvestRecords_.end(), HarvestRecord{});
	return true;
}
