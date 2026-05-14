#include "engine/core/LoadingTask.hpp"

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

    tasks.clear();
    if (const int code = require(!tasks.isFailed(), 8); code != 0) {
        return code;
    }
    if (const int code = require(tasks.totalTasks() == 0, 9); code != 0) {
        return code;
    }
    if (const int code = require(tasks.completedTasks() == 0, 10); code != 0) {
        return code;
    }

    biofuel::engine::tasks::TaskManager taskManager;
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

    return 0;
}
