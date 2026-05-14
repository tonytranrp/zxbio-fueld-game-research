#include "TaskManager.hpp"
#include <taskflow/taskflow.hpp>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

namespace biofuel::engine::tasks {

namespace {

[[nodiscard]] u32 defaultWorkerCount() noexcept {
    const u32 hardware = std::thread::hardware_concurrency();
    if (hardware <= 2U) {
        return 1U;
    }
    return std::min<u32>(hardware - 1U, 4U);
}

[[nodiscard]] bool isTerminal(const TaskState state) noexcept {
    return state == TaskState::Completed
        || state == TaskState::Failed
        || state == TaskState::Cancelled;
}

} // namespace

struct TaskManager::Impl final {
    explicit Impl(const u32 workerCount)
        : executor(workerCount) {}

    struct Record {
        std::string name;
        TaskState state = TaskState::Pending;
        std::string error;
        std::shared_ptr<std::stop_source> stopSource;
    };

    void setState(const TaskId id, const TaskState state) {
        std::scoped_lock lock{mutex};
        if (auto it = records.find(id); it != records.end()) {
            it->second.state = state;
        }
    }

    void setFailure(const TaskId id, std::string message) {
        std::scoped_lock lock{mutex};
        if (auto it = records.find(id); it != records.end()) {
            it->second.state = TaskState::Failed;
            it->second.error = std::move(message);
        }
    }

    void markActiveAsCancelled() {
        std::scoped_lock lock{mutex};
        for (auto& [_, record] : records) {
            if (record.state == TaskState::Pending || record.state == TaskState::Running) {
                record.state = TaskState::Cancelled;
            }
        }
    }

    [[nodiscard]] std::shared_ptr<std::stop_source> sourceFor(const TaskId id) const {
        std::scoped_lock lock{mutex};
        const auto it = records.find(id);
        if (it == records.end()) {
            return {};
        }
        return it->second.stopSource;
    }

    [[nodiscard]] std::vector<std::shared_ptr<std::stop_source>> activeSources() const {
        std::vector<std::shared_ptr<std::stop_source>> out;
        std::scoped_lock lock{mutex};
        out.reserve(records.size());
        for (const auto& [_, record] : records) {
            if (!isTerminal(record.state) && record.stopSource != nullptr) {
                out.push_back(record.stopSource);
            }
        }
        return out;
    }

    tf::Executor executor;
    mutable std::mutex mutex;
    std::unordered_map<TaskId, Record> records;
    std::atomic<TaskId> nextId{1U};
    bool initialized = false;
};

TaskManager::TaskManager()
    : m_impl(std::make_shared<Impl>(defaultWorkerCount())) {}

TaskManager::~TaskManager() noexcept {
    shutdown();
}

void TaskManager::init() {
    if (m_impl->initialized) {
        return;
    }
    m_impl->initialized = true;
}

void TaskManager::shutdown() noexcept {
    if (!m_impl) {
        return;
    }

    try {
        cancelAll();
        m_impl->executor.wait_for_all();
        m_impl->markActiveAsCancelled();
        m_impl->initialized = false;
    } catch (...) {
        // TaskManager shutdown must remain noexcept during application teardown.
    }
}

TaskManager::TaskId TaskManager::schedule(std::string name, Work work) {
    init();

    const TaskId id = m_impl->nextId.fetch_add(1U, std::memory_order_relaxed);
    auto stopSource = std::make_shared<std::stop_source>();
    {
        std::scoped_lock lock{m_impl->mutex};
        m_impl->records.emplace(id, Impl::Record{
            .name = std::move(name),
            .stopSource = stopSource,
        });
    }

    auto impl = m_impl;
    auto token = stopSource->get_token();
    impl->executor.silent_async([impl, id, token, work = std::move(work)]() mutable {
        if (token.stop_requested()) {
            impl->setState(id, TaskState::Cancelled);
            return;
        }

        impl->setState(id, TaskState::Running);
        try {
            if (work) {
                work(token);
            }
            impl->setState(id, token.stop_requested() ? TaskState::Cancelled : TaskState::Completed);
        } catch (const std::exception& ex) {
            impl->setFailure(id, ex.what());
        } catch (...) {
            impl->setFailure(id, "unknown exception");
        }
    });

    return id;
}

TaskManager::Status TaskManager::status(const TaskId id) const {
    std::scoped_lock lock{m_impl->mutex};
    const auto it = m_impl->records.find(id);
    if (it == m_impl->records.end()) {
        return Status{.id = id};
    }

    return Status{
        .id = id,
        .name = it->second.name,
        .state = it->second.state,
        .error = it->second.error,
    };
}

std::unordered_map<TaskManager::TaskId, TaskManager::Status> TaskManager::snapshot() const {
    std::unordered_map<TaskId, Status> out;
    std::scoped_lock lock{m_impl->mutex};
    out.reserve(m_impl->records.size());
    for (const auto& [id, record] : m_impl->records) {
        out.emplace(id, Status{
            .id = id,
            .name = record.name,
            .state = record.state,
            .error = record.error,
        });
    }
    return out;
}

bool TaskManager::finished(const TaskId id) const {
    return isTerminal(status(id).state);
}

bool TaskManager::failed(const TaskId id) const {
    return status(id).state == TaskState::Failed;
}

void TaskManager::cancel(const TaskId id) noexcept {
    try {
        if (auto source = m_impl->sourceFor(id); source != nullptr) {
            source->request_stop();
        }
    } catch (...) {
    }
}

void TaskManager::cancelAll() noexcept {
    try {
        const auto sources = m_impl->activeSources();
        for (const auto& source : sources) {
            if (source != nullptr) {
                source->request_stop();
            }
        }
    } catch (...) {
    }
}

} // namespace biofuel::engine::tasks
