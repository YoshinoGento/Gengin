#pragma once

#include <cstddef>
#include <memory>
#include <string_view>
#include <vector>

class IUndoableCommand {
public:
	virtual ~IUndoableCommand() = default;
	virtual bool Execute() = 0;
	virtual bool Undo() = 0;
	[[nodiscard]] virtual std::string_view GetName() const noexcept = 0;
};

class CommandHistory final {
public:
	explicit CommandHistory(std::size_t capacity = 128);
	// Stored commands may borrow other members of the history's owner.
	CommandHistory(const CommandHistory&) = delete;
	CommandHistory& operator=(const CommandHistory&) = delete;
	CommandHistory(CommandHistory&&) = delete;
	CommandHistory& operator=(CommandHistory&&) = delete;

	bool Execute(std::unique_ptr<IUndoableCommand> command);
	bool Undo();
	bool Redo();
	void Clear() noexcept;

	[[nodiscard]] bool CanUndo() const noexcept { return cursor_ > 0; }
	[[nodiscard]] bool CanRedo() const noexcept { return cursor_ < commands_.size(); }
	[[nodiscard]] std::size_t GetUndoCount() const noexcept { return cursor_; }
	[[nodiscard]] std::size_t GetRedoCount() const noexcept { return commands_.size() - cursor_; }
	[[nodiscard]] std::string_view GetUndoName() const noexcept;
	[[nodiscard]] std::string_view GetRedoName() const noexcept;

private:
	std::vector<std::unique_ptr<IUndoableCommand>> commands_;
	std::size_t cursor_ = 0;
	std::size_t capacity_ = 128;
};
