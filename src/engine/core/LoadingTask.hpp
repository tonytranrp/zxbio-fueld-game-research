#pragma once

#include "engine/core/Types.hpp"
#include "engine/tasks/TaskManager.hpp"
#include <exception>
#include <functional>
#include <optional>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

namespace biofuel {

// ------------------------------------------------------------------------------
// LoadingTask — A single initialization step for the loading screen
// ------------------------------------------------------------------------------
struct LoadingTask {
    using Work = std::function<void()>;
    using AsyncWork = std::function<void(std::stop_token)>;

    std::string name;
    f32 weight = 1.0f;
    Work work;
    AsyncWork asyncWork;
    bool runAsync = false;

    [[nodiscard]] static LoadingTask async(std::string taskName, const f32 taskWeight, AsyncWork taskWork) {
        LoadingTask task{
            .name = std::move(taskName),
            .weight = taskWeight,
            .asyncWork = std::move(taskWork),
            .runAsync = true,
        };
        return task;
    }
};

// ------------------------------------------------------------------------------
// LoadingTaskQueue — Sequential deferred init task processor
// Tracks progress for display on the loading screen.
// ------------------------------------------------------------------------------
class LoadingTaskQueue {
public:
    void clear(::biofuel::engine::tasks::TaskManager& taskManager) noexcept {
        cancelActive(taskManager);
        resetState();
    }

    void cancelActive(::biofuel::engine::tasks::TaskManager& taskManager) noexcept {
        if (m_activeAsyncTask.has_value()) {
            taskManager.cancel(*m_activeAsyncTask);
        }
    }

    void reserve(const size_t taskCount) {
        m_tasks.reserve(taskCount);
    }

    void add(LoadingTask task) {
        m_totalWeight += task.weight;
        m_tasks.push_back(std::move(task));
    }

    void processNext(::biofuel::engine::tasks::TaskManager* taskManager = nullptr) {
        if (m_tasks.empty() || m_failed) {
            return;
        }
        if (isDone()) {
            return;
        }

        if (m_currentIndex < 0) {
            m_currentIndex = 0;
        }

        auto& task = m_tasks[m_currentIndex];
        if (m_activeAsyncTask.has_value()) {
            if (taskManager == nullptr) {
                fail(task.name, "async task manager is unavailable");
                return;
            }

            const auto status = taskManager->status(*m_activeAsyncTask);
            if (status.state == ::biofuel::engine::tasks::TaskState::Failed) {
                fail(task.name, status.error.c_str());
                return;
            }
            if (status.state == ::biofuel::engine::tasks::TaskState::Cancelled) {
                fail(task.name, "async task was cancelled");
                return;
            }
            if (status.state != ::biofuel::engine::tasks::TaskState::Completed) {
                return;
            }

            completeCurrentTask(task.weight);
            m_activeAsyncTask.reset();
            return;
        }

        try {
            if (task.runAsync) {
                if (taskManager == nullptr) {
                    fail(task.name, "async task manager is unavailable");
                    return;
                }
                m_activeAsyncTask = taskManager->schedule(task.name, std::move(task.asyncWork));
                return;
            }

            if (task.work) {
                task.work();
            }
        } catch (const std::exception& ex) {
            fail(task.name, ex.what());
            return;
        } catch (...) {
            fail(task.name, "unknown exception");
            return;
        }
        completeCurrentTask(task.weight);
    }

    [[nodiscard]] f32 progress() const noexcept {
        if (m_totalWeight <= 0.0f) {
            return 0.0f;
        }
        return m_completedWeight / m_totalWeight;
    }

    [[nodiscard]] const std::string& currentName() const noexcept {
        if (m_failed) {
            return m_failureMessage;
        }
        if (isDone() && m_currentIndex > 0) {
            return m_tasks[m_currentIndex - 1].name;
        }
        if (m_currentIndex >= 0 && m_currentIndex < static_cast<i32>(m_tasks.size())) {
            return m_tasks[m_currentIndex].name;
        }
        static const std::string ready = "Ready.";
        return ready;
    }

    [[nodiscard]] bool isDone() const noexcept {
        return !m_failed && m_currentIndex >= static_cast<i32>(m_tasks.size());
    }

    [[nodiscard]] bool isFailed() const noexcept {
        return m_failed;
    }

    [[nodiscard]] const std::string& failureMessage() const noexcept {
        return m_failureMessage;
    }

    [[nodiscard]] i32 totalTasks() const noexcept {
        return static_cast<i32>(m_tasks.size());
    }

    [[nodiscard]] i32 completedTasks() const noexcept {
        if (m_currentIndex < 0) {
            return 0;
        }
        return m_currentIndex;
    }

private:
    void resetState() noexcept {
        m_tasks.clear();
        m_currentIndex = -1;
        m_completedWeight = 0.0f;
        m_totalWeight = 0.0f;
        m_failed = false;
        m_failureMessage.clear();
        m_activeAsyncTask.reset();
    }

    void fail(const std::string& taskName, const char* reason) {
        m_failed = true;
        m_failureMessage = "Failed: ";
        m_failureMessage += taskName.empty() ? "loading task" : taskName;
        if (reason != nullptr && reason[0] != '\0') {
            m_failureMessage += " (";
            m_failureMessage += reason;
            m_failureMessage += ")";
        }
    }

    void completeCurrentTask(const f32 weight) noexcept {
        m_completedWeight += weight;
        ++m_currentIndex;
    }

    std::vector<LoadingTask> m_tasks;
    std::optional<::biofuel::engine::tasks::TaskManager::TaskId> m_activeAsyncTask;
    i32 m_currentIndex = -1;
    f32 m_completedWeight = 0.0f;
    f32 m_totalWeight = 0.0f;
    bool m_failed = false;
    std::string m_failureMessage;
};

} // namespace biofuel
