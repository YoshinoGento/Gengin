#pragma once

#include "farm/system/FarmCropSelectionSystem.h"
#include "farm/system/FarmEconomySystem.h"
#include "farm/system/FarmDateSystem.h"
#include "farm/system/FarmProgressionSystem.h"

#include <string>
#include <vector>

namespace farm {
class FarmGrid;
}

enum class FarmDocumentStatus {
	Ready,
	Modified,
	Saved,
	Loaded,
	Reset,
	Deleted,
	Error,
};

struct FarmDocumentEntry {
	std::string id;
	std::string displayName;
	std::string savedAt;
};

// Owns Farm document persistence and validation. UI code only requests operations.
class FarmDocumentSystem final {
public:
    // Read-only extraction: never selects a document or writes the player's catalog.
    static bool ReadExhibitionResults(const std::string& path,
        FarmEconomySystem::ContestResults& results, int& day);
	bool Initialize(
		const std::string& directoryPath, farm::FarmGrid& grid,
		FarmEconomySystem& economySystem,
		FarmCropSelectionSystem& cropSelectionSystem, FarmDateSystem* dateSystem = nullptr,
		FarmProgressionSystem* progressionSystem = nullptr);
	bool Save(
		const farm::FarmGrid& grid, const FarmEconomySystem& economySystem,
		const FarmCropSelectionSystem& cropSelectionSystem);
	bool SaveAs(
		const std::string& displayName, const farm::FarmGrid& grid,
		const FarmEconomySystem& economySystem,
		const FarmCropSelectionSystem& cropSelectionSystem);
	bool Load(
		const std::string& documentId, farm::FarmGrid& grid,
		FarmEconomySystem& economySystem,
		FarmCropSelectionSystem& cropSelectionSystem);
	bool ArchiveCompletedSeason(const farm::FarmGrid& grid, const FarmEconomySystem& economySystem,
		const FarmCropSelectionSystem& cropSelectionSystem);
	void BeginUnsavedSession();
	bool Rename(const std::string& documentId, const std::string& displayName);
	bool Delete(const std::string& documentId);
	bool Reset(
		farm::FarmGrid& grid, FarmEconomySystem& economySystem,
		FarmCropSelectionSystem& cropSelectionSystem);
	void MarkDirty() noexcept;

	[[nodiscard]] bool IsDirty() const noexcept { return dirty_; }
	[[nodiscard]] bool FileExists() const noexcept { return fileExists_; }
	[[nodiscard]] bool HasDocuments() const noexcept { return !documents_.empty(); }
	[[nodiscard]] bool HasError() const noexcept { return status_ == FarmDocumentStatus::Error; }
	[[nodiscard]] FarmDocumentStatus GetStatus() const noexcept { return status_; }
	[[nodiscard]] const std::string& GetPath() const noexcept { return path_; }
	[[nodiscard]] const std::string& GetActiveDocumentId() const noexcept { return activeDocumentId_; }
	[[nodiscard]] const std::string& GetDisplayName() const noexcept { return displayName_; }
	[[nodiscard]] const std::string& GetStatusMessage() const noexcept { return message_; }
	[[nodiscard]] const std::vector<FarmDocumentEntry>& GetDocuments() const noexcept { return documents_; }

private:
	FarmProgressionSystem& Progression() noexcept { return progressionSystem_ ? *progressionSystem_ : fallbackProgression_; }
	FarmProgressionSystem fallbackProgression_{};
	FarmProgressionSystem* progressionSystem_ = nullptr; // Borrowed, same lifetime requirement as the clock.
	FarmProgressionMode defaultMode_ = FarmProgressionMode::Trial;
	FarmDateSystem& Date() noexcept { return dateSystem_ ? *dateSystem_ : fallbackDateSystem_; }
	FarmDateSystem fallbackDateSystem_{};
	FarmDateSystem* dateSystem_ = nullptr; // Borrowed: must outlive all document operations.
	bool RefreshDocumentList();
	bool WriteCatalog();
	bool LoadCatalog(std::string& activeDocumentId) const;
	bool SaveToDocument(
		const std::string& documentId,
		const std::string& displayName,
		const farm::FarmGrid& grid,
		const FarmEconomySystem& economySystem,
		const FarmCropSelectionSystem& cropSelectionSystem);
	void SetStatus(FarmDocumentStatus status, std::string message);
	void SetError(std::string message);

	std::string directoryPath_;
	std::string saveDirectoryPath_;
	std::string catalogPath_;
	std::string path_;
	std::string activeDocumentId_;
	std::string displayName_ = "Untitled Farm";
	std::string message_ = "Ready";
	FarmDocumentStatus status_ = FarmDocumentStatus::Ready;
	std::vector<FarmDocumentEntry> documents_;
	FarmEconomySystem::Snapshot defaultEconomySnapshot_{};
	FarmCropSelectionSystem::Snapshot defaultCropSelectionSnapshot_{};
	bool dirty_ = false;
	bool fileExists_ = false;
};
