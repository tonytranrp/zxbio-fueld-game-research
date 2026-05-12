#pragma once

#include "engine/core/Types.hpp"
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace biofuel {

// ------------------------------------------------------------------------------
// LoadingTask — A single initialization step for the loading screen
// ------------------------------------------------------------------------------
struct LoadingTask {
    std::string name;
    f32 weight = 1.0f;
    std::function<void()> work;
};

// ------------------------------------------------------------------------------
// LoadingTaskQueue — Sequential deferred init task processor
// Tracks progress for display on the loading screen.
// ------------------------------------------------------------------------------
class LoadingTaskQueue {
public:
    void clear() noexcept {
        m_tasks.clear();
        m_currentIndex = -1;
        m_completedWeight = 0.0f;
        m_totalWeight = 0.0f;
    }

    void add(LoadingTask task) {
        m_totalWeight += task.weight;
        m_tasks.push_back(std::move(task));
    }

    void processNext() {
        if (m_tasks.empty()) {
            return;
        }
        if (isDone()) {
            return;
        }

        if (m_currentIndex < 0) {
            m_currentIndex = 0;
        }

        if (m_tasks[m_currentIndex].work) {
            m_tasks[m_currentIndex].work();
        }
        m_completedWeight += m_tasks[m_currentIndex].weight;
        ++m_currentIndex;
    }

    [[nodiscard]] f32 progress() const noexcept {
        if (m_totalWeight <= 0.0f) {
            return 0.0f;
        }
        return m_completedWeight / m_totalWeight;
    }

    [[nodiscard]] const std::string& currentName() const noexcept {
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
        return m_currentIndex >= static_cast<i32>(m_tasks.size());
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
    std::vector<LoadingTask> m_tasks;
    i32 m_currentIndex = -1;
    f32 m_completedWeight = 0.0f;
    f32 m_totalWeight = 0.0f;
};

} // namespace biofuel
