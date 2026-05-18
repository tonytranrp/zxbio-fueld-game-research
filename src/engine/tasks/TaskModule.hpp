#pragma once

#include "engine/core/LoadingTask.hpp"
#include "engine/core/Types.hpp"
#include <pb/pipeline.hpp>
#include <concepts>
#include <functional>
#include <string>

namespace biofuel::engine::tasks {

// ---------------------------------------------------------------------------
// Simple token types for init-task pipeline I/O
// ---------------------------------------------------------------------------
struct InitToken {};
struct InitResult {};

// ---------------------------------------------------------------------------
// TaskModule concept
//   - a valid pb::core::ValidPipeline
//   - a task_weight() returning a float
//   - a task_init_fn() returning a std::function<void()> for the real init work
// ---------------------------------------------------------------------------
template <typename M>
concept TaskModule = requires {
    typename M::pipeline;
    requires pb::core::ValidPipeline<typename M::pipeline>;
    { M::task_weight() } -> std::convertible_to<f32>;
    { M::init_work() } -> std::convertible_to<std::function<void()>>;
};

// ---------------------------------------------------------------------------
// TaskModuleList — compile-time list of TaskModules
// ---------------------------------------------------------------------------
template <typename... Modules>
struct TaskModuleList {
    static constexpr std::size_t size() { return sizeof...(Modules); }

    static void populate(LoadingTaskQueue& queue) {
        (populateOne<Modules>(queue), ...);
    }

private:
    template <typename M>
    static void populateOne(LoadingTaskQueue& queue) {
        static_assert(TaskModule<M>, "Every element in TaskModuleList must satisfy TaskModule");

        LoadingTask task{
            .name = std::string{M::task_label()},
            .weight = M::task_weight(),
            .work = M::init_work(),
        };
        queue.add(std::move(task));
    }
};

} // namespace biofuel::engine::tasks
