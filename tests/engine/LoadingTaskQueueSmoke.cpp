#include "engine/core/LoadingTask.hpp"

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

int require(const bool condition, const int code) {
    return condition ? 0 : code;
}

} // namespace

int main() {
    biofuel::engine::tasks::TaskManager taskManager;
    biofuel::LoadingTaskQueue tasks;
    tasks.add({"first", 1.0f, []() {}});
    tasks.add({"throws", 2.0f, []() {
        throw std::runtime_error{"synthetic failure"};
    }});
    tasks.add({"must not run", 4.0f, []() {
        throw std::runtime_error{"processed after failure"};
    }});

    tasks.processNext();
    if (const int code = require(!tasks.isFailed(), 1); code != 0) {
        return code;
    }
    if (const int code = require(tasks.completedTasks() == 1, 2); code != 0) {
        return code;
    }

    tasks.processNext();
    if (const int code = require(tasks.isFailed(), 3); code != 0) {
        return code;
    }
    if (const int code = require(tasks.completedTasks() == 1, 4); code != 0) {
        return code;
    }
    const std::string failure{tasks.failureMessage()};
    if (const int code = require(failure.find("throws") != std::string::npos, 5); code != 0) {
        return code;
    }
    if (const int code = require(failure.find("synthetic failure") != std::string::npos, 6); code != 0) {
        return code;
    }

    tasks.processNext();
    if (const int code = require(tasks.completedTasks() == 1, 7); code != 0) {
        return code;
    }

    tasks.clear(taskManager);
    if (const int code = require(!tasks.isFailed(), 8); code != 0) {
        return code;
    }
    if (const int code = require(tasks.totalTasks() == 0, 9); code != 0) {
        return code;
    }
    if (const int code = require(tasks.completedTasks() == 0, 10); code != 0) {
        return code;
    }

    tasks.add(biofuel::LoadingTask::async("async preflight", 1.0f, [](std::stop_token) {}));
    tasks.processNext(&taskManager);
    for (int i = 0; i < 100 && !tasks.isDone() && !tasks.isFailed(); ++i) {
        tasks.processNext(&taskManager);
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    if (const int code = require(tasks.isDone(), 11); code != 0) {
        return code;
    }
    if (const int code = require(!tasks.isFailed(), 12); code != 0) {
        return code;
    }

    tasks.clear(taskManager);
    std::atomic<bool> asyncStarted = false;
    std::atomic<bool> asyncObservedStop = false;
    tasks.add(biofuel::LoadingTask::async("async cancel", 1.0f, [&](std::stop_token token) {
        asyncStarted.store(true, std::memory_order_release);
        while (!token.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }
        asyncObservedStop.store(true, std::memory_order_release);
    }));
    tasks.processNext(&taskManager);
    for (int i = 0; i < 100 && !asyncStarted.load(std::memory_order_acquire); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    if (const int code = require(asyncStarted.load(std::memory_order_acquire), 13); code != 0) {
        return code;
    }
    auto unrelated = taskManager.scheduleValue<int>("unrelated task", []() {
        return 123;
    });
    tasks.clear(taskManager);
    for (int i = 0; i < 100 && !asyncObservedStop.load(std::memory_order_acquire); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    if (const int code = require(asyncObservedStop.load(std::memory_order_acquire), 14); code != 0) {
        return code;
    }
    if (const int code = require(!tasks.isFailed(), 15); code != 0) {
        return code;
    }
    if (const int code = require(tasks.totalTasks() == 0, 16); code != 0) {
        return code;
    }
    if (const int code = require(tasks.completedTasks() == 0, 17); code != 0) {
        return code;
    }
    for (int i = 0; i < 100 && !unrelated.ready(taskManager); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    const auto unrelatedValue = unrelated.tryTake(taskManager);
    if (const int code = require(unrelatedValue.has_value() && *unrelatedValue == 123, 18); code != 0) {
        return code;
    }

    tasks.add(biofuel::LoadingTask::async("externally cancelled", 1.0f, [](std::stop_token token) {
        while (!token.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }
    }));
    tasks.processNext(&taskManager);
    taskManager.cancelAll();
    for (int i = 0; i < 100 && !tasks.isFailed(); ++i) {
        tasks.processNext(&taskManager);
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    if (const int code = require(tasks.isFailed(), 19); code != 0) {
        return code;
    }
    if (const int code = require(tasks.failureMessage().find("cancelled") != std::string::npos, 20); code != 0) {
        return code;
    }

    return 0;
}
