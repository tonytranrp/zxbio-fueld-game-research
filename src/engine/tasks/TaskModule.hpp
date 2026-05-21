#pragma once

#include "engine/core/LoadingTask.hpp"
#include "engine/core/Types.hpp"
#include <pb/pipeline.hpp>
#include <concepts>
#include <functional>
#include <string>
#include <string_view>

namespace biofuel::engine::tasks {

// ---------------------------------------------------------------------------
// Simple token types for init-task pipeline I/O
// ---------------------------------------------------------------------------
struct InitToken {};
struct InitResult {};

// ---------------------------------------------------------------------------
// TaskModule concept
//   - a valid pb::core::ValidPipeline
//   - a task_label() returning the user-visible loading label
//   - a task_weight() returning a float
//   - an init_work() returning a std::function<void()> for the real init work
// ---------------------------------------------------------------------------
template <typename M>
concept TaskModule = requires {
    typename M::pipeline;
    requires pb::core::ValidPipeline<typename M::pipeline>;
    { M::task_label() } -> std::convertible_to<std::string_view>;
    { M::task_weight() } -> std::convertible_to<f32>;
    { M::init_work() } -> std::convertible_to<std::function<void()>>;
};

template <typename M>
consteval bool validateTaskModule() {
    static_assert(TaskModule<M>, "Every engine startup task module must satisfy TaskModule");
    if constexpr (TaskModule<M>) {
        static_assert(!std::string_view{M::task_label()}.empty(),
            "Engine startup task modules must provide a non-empty task_label().");
        static_assert(M::task_weight() > 0.0f,
            "Engine startup task modules must provide a positive compile-time task_weight().");
    }
    return true;
}

// ---------------------------------------------------------------------------
// TaskModuleList — compile-time list of TaskModules
// ---------------------------------------------------------------------------
template <typename... Modules>
struct TaskModuleList {
    static_assert((validateTaskModule<Modules>() && ...),
        "Every element in TaskModuleList must satisfy the engine startup task contract.");

    static constexpr std::size_t size() { return sizeof...(Modules); }

    static consteval bool valid() {
        return (validateTaskModule<Modules>() && ...);
    }

    static void populate(LoadingTaskQueue& queue) {
        (populateOne<Modules>(queue), ...);
    }

private:
    template <typename M>
    static void populateOne(LoadingTaskQueue& queue) {
        LoadingTask task{
            .name = std::string{M::task_label()},
            .weight = M::task_weight(),
            .work = M::init_work(),
        };
        queue.add(std::move(task));
    }
};

} // namespace biofuel::engine::tasks
