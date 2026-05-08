#pragma once

#include <taskflow/taskflow.hpp>
#include <future>
#include <functional>

namespace biofuel::utils::task {

// ------------------------------------------------------------------------------
// TaskSystem - taskflow parallel execution wrapper
// ------------------------------------------------------------------------------
class TaskSystem {
public:
    using Executor = tf::Executor;
    using Taskflow = tf::Taskflow;

    [[nodiscard]] static Executor& getExecutor();

    template<typename Func>
    static auto async(Func&& func);

    static void waitForAll();

private:
    static Executor s_executor;
};

// ------------------------------------------------------------------------------
// Inline implementations (header-only)
// ------------------------------------------------------------------------------

template<typename Func>
auto TaskSystem::async(Func&& func) {
    return getExecutor().async(std::forward<Func>(func));
}

} // namespace biofuel::utils::task
