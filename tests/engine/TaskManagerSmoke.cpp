#include "engine/tasks/TaskManager.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {

bool check(const bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        return false;
    }
    return true;
}

template<typename TPredicate>
bool waitUntil(TPredicate&& predicate) {
    using namespace std::chrono_literals;
    for (int i = 0; i < 100; ++i) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(10ms);
    }
    return false;
}

} // namespace

int main() {
    using ::biofuel::engine::tasks::TaskManager;
    using ::biofuel::engine::tasks::TaskState;

    TaskManager manager;
    manager.init();

    auto value = manager.scheduleValue<int>("compute value", []() {
        return 42;
    });

    if (!check(waitUntil([&]() { return value.ready(manager); }), "value task did not finish")) {
        return EXIT_FAILURE;
    }
    if (!check(manager.status(value.id()).state == TaskState::Completed, "value task did not complete")) {
        return EXIT_FAILURE;
    }
    const auto taken = value.tryTake(manager);
    if (!check(taken.has_value() && *taken == 42, "value task result mismatch")) {
        return EXIT_FAILURE;
    }

    const auto failed = manager.schedule("failing task", [](std::stop_token) {
        throw std::runtime_error{"synthetic task failure"};
    });
    if (!check(waitUntil([&]() { return manager.finished(failed); }), "failing task did not finish")) {
        return EXIT_FAILURE;
    }
    const auto failureStatus = manager.status(failed);
    if (!check(failureStatus.state == TaskState::Failed, "failing task did not enter failed state")) {
        return EXIT_FAILURE;
    }
    if (!check(failureStatus.error.find("synthetic task failure") != std::string::npos, "failure message missing exception text")) {
        return EXIT_FAILURE;
    }

    manager.shutdown();
    return EXIT_SUCCESS;
}
