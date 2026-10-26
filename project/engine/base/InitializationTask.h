#pragma once
#include <coroutine>
#include <exception>
#include <source_location>
#include <stdexcept>
#include <utility>

// Single-threaded, move-only initialization. Owners must outlive the task;
// destroying a suspended parent also destroys its suspended child.
class InitializationTask final {
public:
    struct promise_type;
    using Handle = std::coroutine_handle<promise_type>;
    struct promise_type {
        Handle child{}; // Borrowed from the currently suspended Awaiter.
        std::exception_ptr error{};
        bool result = false;
        std::source_location location{};
        std::suspend_always await_transform(std::suspend_always value,
            std::source_location at = std::source_location::current()) noexcept {
            location = at;
            return value;
        }
        InitializationTask&& await_transform(InitializationTask&& value,
            std::source_location at = std::source_location::current()) noexcept {
            location = at;
            return std::move(value);
        }
        InitializationTask get_return_object() noexcept { return InitializationTask{Handle::from_promise(*this)}; }
        std::suspend_always initial_suspend() const noexcept { return {}; }
        std::suspend_always final_suspend() const noexcept { return {}; }
        void return_value(bool value) noexcept { result = value; }
        void unhandled_exception() noexcept { error = std::current_exception(); }
    };

    InitializationTask() noexcept = default;
    InitializationTask(InitializationTask&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}
    InitializationTask& operator=(InitializationTask&& other) noexcept {
        if (this != &other) { Reset(); handle_ = std::exchange(other.handle_, {}); }
        return *this;
    }
    InitializationTask(const InitializationTask&) = delete;
    InitializationTask& operator=(const InitializationTask&) = delete;
    ~InitializationTask() { Reset(); }

    void Reset() noexcept { if (handle_) handle_.destroy(); handle_ = {}; }
    [[nodiscard]] bool Done() const noexcept { return !handle_ || handle_.done(); }
    std::source_location ResumeOne() {
        if (Done()) return {};
        auto leaf = handle_;
        while (leaf.promise().child) {
            const auto child = leaf.promise().child;
            if (child.done()) { leaf.promise().child = {}; break; }
            leaf = child;
        }
        leaf.resume();
        return leaf.promise().location;
    }
    [[nodiscard]] bool Result() const {
        if (!handle_ || !handle_.done()) throw std::logic_error("Initialization result is not ready");
        if (handle_.promise().error) std::rethrow_exception(handle_.promise().error);
        return handle_.promise().result;
    }
    bool Complete() { while (!Done()) ResumeOne(); return Result(); }

    struct Awaiter final {
        explicit Awaiter(Handle value) noexcept : child(value) {}
        Awaiter(const Awaiter&) = delete;
        Awaiter(Awaiter&& other) noexcept : child(std::exchange(other.child, {})) {}
        ~Awaiter() { if (child) child.destroy(); }
        bool await_ready() const noexcept { return !child || child.done(); }
        void await_suspend(Handle parent) const noexcept { parent.promise().child = child; }
        bool await_resume() const {
            if (!child) throw std::logic_error("Empty initialization task");
            if (child.promise().error) std::rethrow_exception(child.promise().error);
            return child.promise().result;
        }
    private:
        Handle child;
    };
    Awaiter operator co_await() && noexcept { return Awaiter{std::exchange(handle_, {})}; }

private:
    explicit InitializationTask(Handle handle) noexcept : handle_(handle) {}
    Handle handle_{};
};
