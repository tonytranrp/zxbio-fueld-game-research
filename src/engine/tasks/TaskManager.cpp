#include "TaskManager.hpp"
#include <taskflow/taskflow.hpp>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <thread>

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
        for (auto& [id, record] : records) {
            (void)id;
            if (record.state == TaskState::Pending || record.state == TaskState::Running) {
                record.state = TaskState::Cancelled;
            }
        }
    }

    tf::Executor executor;
    mutable std::mutex mutex;
    std::unordered_map<TaskId, Record> records;
    std::atomic<TaskId> nextId{1U};
    std::stop_source stopSource;
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
    if (m_impl->stopSource.stop_requested()) {
        m_impl->stopSource = std::stop_source{};
    }
    m_impl->initialized = true;
}

void TaskManager::shutdown() noexcept {
    if (!m_impl) {
        return;
    }

    try {
        m_impl->stopSource.request_stop();
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
    {
        std::scoped_lock lock{m_impl->mutex};
        m_impl->records.emplace(id, Impl::Record{.name = std::move(name)});
    }

    auto impl = m_impl;
    auto token = impl->stopSource.get_token();
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

void TaskManager::cancelAll() noexcept {
    try {
        m_impl->stopSource.request_stop();
        m_impl->markActiveAsCancelled();
    } catch (...) {
    }
}

} // namespace biofuel::engine::tasks
