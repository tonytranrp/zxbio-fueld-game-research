#pragma once

#include "engine/core/Types.hpp"
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace biofuel::engine::tasks {

enum class TaskState : u8 {
    Unknown,
    Pending,
    Running,
    Completed,
    Failed,
    Cancelled,
};

class TaskManager;

template<typename TResult>
class TaskResult {
public:
    TaskResult() = default;

    [[nodiscard]] u64 id() const noexcept { return m_id; }
    [[nodiscard]] bool valid() const noexcept { return m_id != 0U && m_value != nullptr; }
    [[nodiscard]] bool ready(const TaskManager& manager) const;
    [[nodiscard]] std::optional<TResult> tryTake(const TaskManager& manager);
    [[nodiscard]] std::string errorMessage(const TaskManager& manager) const;

private:
    friend class TaskManager;

    TaskResult(const u64 id, std::shared_ptr<std::optional<TResult>> value) noexcept
        : m_id(id), m_value(std::move(value)) {}

    u64 m_id = 0U;
    std::shared_ptr<std::optional<TResult>> m_value;
};

class TaskManager final {
public:
    using TaskId = u64;
    using Work = std::function<void(std::stop_token)>;

    struct Status {
        TaskId id = 0U;
        std::string name;
        TaskState state = TaskState::Unknown;
        std::string error;
    };

    TaskManager();
    ~TaskManager() noexcept;

    TaskManager(const TaskManager&) = delete;
    TaskManager& operator=(const TaskManager&) = delete;
    TaskManager(TaskManager&&) = delete;
    TaskManager& operator=(TaskManager&&) = delete;

    void init();
    void shutdown() noexcept;

    [[nodiscard]] TaskId schedule(std::string name, Work work);

    template<typename TResult, typename TCallable>
    [[nodiscard]] TaskResult<TResult> scheduleValue(std::string name, TCallable&& callable) {
        auto value = std::make_shared<std::optional<TResult>>();
        auto callableCopy = std::forward<TCallable>(callable);
        const TaskId id = schedule(std::move(name), [value, callableCopy = std::move(callableCopy)](std::stop_token token) mutable {
            if (token.stop_requested()) {
                return;
            }
            if constexpr (std::is_invocable_v<TCallable&, std::stop_token>) {
                value->emplace(std::invoke(callableCopy, token));
            } else {
                value->emplace(std::invoke(callableCopy));
            }
        });
        return TaskResult<TResult>{id, std::move(value)};
    }

    [[nodiscard]] Status status(TaskId id) const;
    [[nodiscard]] std::unordered_map<TaskId, Status> snapshot() const;
    [[nodiscard]] bool finished(TaskId id) const;
    [[nodiscard]] bool failed(TaskId id) const;

    void cancel(TaskId id) noexcept;
    void cancelAll() noexcept;

private:
    struct Impl;
    std::shared_ptr<Impl> m_impl;
};

template<typename TResult>
bool TaskResult<TResult>::ready(const TaskManager& manager) const {
    return manager.finished(m_id);
}

template<typename TResult>
std::optional<TResult> TaskResult<TResult>::tryTake(const TaskManager& manager) {
    if (!ready(manager) || m_value == nullptr || !m_value->has_value()) {
        return std::nullopt;
    }

    std::optional<TResult> out{std::move(**m_value)};
    m_value->reset();
    return out;
}

template<typename TResult>
std::string TaskResult<TResult>::errorMessage(const TaskManager& manager) const {
    return manager.status(m_id).error;
}

} // namespace biofuel::engine::tasks
