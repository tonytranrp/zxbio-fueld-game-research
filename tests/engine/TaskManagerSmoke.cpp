#include "engine/tasks/TaskManager.hpp"

#include <atomic>
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

    std::atomic<bool> cooperativeStarted = false;
    std::atomic<bool> cooperativeObservedStop = false;
    const auto cooperative = manager.schedule("cooperative cancel", [&](std::stop_token token) {
        cooperativeStarted.store(true, std::memory_order_release);
        while (!token.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }
        cooperativeObservedStop.store(true, std::memory_order_release);
    });
    if (!check(waitUntil([&]() { return cooperativeStarted.load(std::memory_order_acquire); }), "cooperative task did not start")) {
        return EXIT_FAILURE;
    }
    manager.cancelAll();
    if (!check(waitUntil([&]() { return manager.finished(cooperative); }), "cooperative task did not finish after cancelAll")) {
        return EXIT_FAILURE;
    }
    if (!check(cooperativeObservedStop.load(std::memory_order_acquire), "cooperative task did not observe cancellation")) {
        return EXIT_FAILURE;
    }
    if (!check(manager.status(cooperative).state == TaskState::Cancelled, "cooperative task did not enter cancelled state")) {
        return EXIT_FAILURE;
    }

    auto postCancel = manager.scheduleValue<int>("post cancel value", []() {
        return 7;
    });
    if (!check(waitUntil([&]() { return postCancel.ready(manager); }), "post-cancel value task did not finish")) {
        return EXIT_FAILURE;
    }
    if (!check(manager.status(postCancel.id()).state == TaskState::Completed, "post-cancel value task did not complete")) {
        return EXIT_FAILURE;
    }
    const auto postCancelValue = postCancel.tryTake(manager);
    if (!check(postCancelValue.has_value() && *postCancelValue == 7, "post-cancel value mismatch")) {
        return EXIT_FAILURE;
    }

    std::atomic<bool> stubbornStarted = false;
    std::atomic<bool> releaseStubborn = false;
    const auto stubborn = manager.schedule("non-cooperative cancel", [&](std::stop_token) {
        stubbornStarted.store(true, std::memory_order_release);
        while (!releaseStubborn.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }
    });
    if (!check(waitUntil([&]() { return manager.status(stubborn).state == TaskState::Running && stubbornStarted.load(std::memory_order_acquire); }), "non-cooperative task did not enter running state")) {
        return EXIT_FAILURE;
    }
    manager.cancelAll();
    if (!check(!manager.finished(stubborn), "non-cooperative task was marked terminal before exiting")) {
        return EXIT_FAILURE;
    }
    releaseStubborn.store(true, std::memory_order_release);
    if (!check(waitUntil([&]() { return manager.finished(stubborn); }), "non-cooperative task did not finish after release")) {
        return EXIT_FAILURE;
    }
    if (!check(manager.status(stubborn).state == TaskState::Cancelled, "non-cooperative task did not settle as cancelled after stop request")) {
        return EXIT_FAILURE;
    }

    std::atomic<bool> isolatedStarted = false;
    std::atomic<bool> isolatedObservedStop = false;
    const auto isolatedCancel = manager.schedule("isolated cancel", [&](std::stop_token token) {
        isolatedStarted.store(true, std::memory_order_release);
        while (!token.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }
        isolatedObservedStop.store(true, std::memory_order_release);
    });
    auto isolatedComplete = manager.scheduleValue<int>("isolated complete", []() {
        return 99;
    });
    if (!check(waitUntil([&]() { return isolatedStarted.load(std::memory_order_acquire); }), "isolated cancel task did not start")) {
        return EXIT_FAILURE;
    }
    manager.cancel(isolatedCancel);
    if (!check(waitUntil([&]() { return manager.finished(isolatedCancel); }), "isolated cancel task did not finish")) {
        return EXIT_FAILURE;
    }
    if (!check(isolatedObservedStop.load(std::memory_order_acquire), "isolated cancel task did not observe stop")) {
        return EXIT_FAILURE;
    }
    if (!check(waitUntil([&]() { return isolatedComplete.ready(manager); }), "isolated complete task did not finish")) {
        return EXIT_FAILURE;
    }
    const auto isolatedValue = isolatedComplete.tryTake(manager);
    if (!check(isolatedValue.has_value() && *isolatedValue == 99, "per-task cancel affected unrelated task")) {
        return EXIT_FAILURE;
    }

    manager.shutdown();
    return EXIT_SUCCESS;
}
