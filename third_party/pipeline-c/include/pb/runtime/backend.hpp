#pragma once

#include <array>
#include <span>
#include <string_view>

namespace pb::runtime {

enum class backend_support {
  supported,
  roadmap,
  experimental,
};

enum class backend_execution_model {
  sequential,
  thread_pool,
  task_graph,
  filter_pipeline,
  sender_receiver,
};

namespace detail {

inline constexpr bool taskflow_backend_compile_time_available =
#if defined(PB_HAS_TASKFLOW)
    true;
#else
    false;
#endif

inline constexpr bool tbb_backend_compile_time_available =
#if defined(PB_HAS_TBB)
    true;
#else
    false;
#endif

inline constexpr bool stdexec_backend_compile_time_available =
#if defined(PB_HAS_STDEXEC)
    true;
#else
    false;
#endif

} // namespace detail

[[nodiscard]] constexpr auto backend_support_name(backend_support support) noexcept -> std::string_view {
  switch (support) {
    case backend_support::supported:    return "supported";
    case backend_support::roadmap:      return "roadmap";
    case backend_support::experimental: return "experimental";
  }
  return "unknown";
}

[[nodiscard]] constexpr auto backend_execution_model_name(backend_execution_model model) noexcept
    -> std::string_view {
  switch (model) {
    case backend_execution_model::sequential:       return "sequential";
    case backend_execution_model::thread_pool:      return "thread_pool";
    case backend_execution_model::task_graph:       return "task_graph";
    case backend_execution_model::filter_pipeline:  return "filter_pipeline";
    case backend_execution_model::sender_receiver:  return "sender_receiver";
  }
  return "unknown";
}

struct backend_feature {
  std::string_view name;
  backend_execution_model execution_model;
  backend_support support;
  bool external_dependency;
  bool default_build;
  bool compile_time_available;
  std::string_view availability_macro;
  std::string_view note;
};

inline constexpr auto backend_feature_matrix = std::array{
    backend_feature{.name = "sequential",
                    .execution_model = backend_execution_model::sequential,
                    .support = backend_support::supported,
                    .external_dependency = false,
                    .default_build = true,
                    .compile_time_available = true,
                    .availability_macro = "",
                    .note = "standard-library deterministic linear runtime"},
    backend_feature{.name = "thread_pool",
                    .execution_model = backend_execution_model::thread_pool,
                    .support = backend_support::supported,
                    .external_dependency = false,
                    .default_build = false,
                    .compile_time_available = true,
                    .availability_macro = "",
                    .note = "standard-library thread-pool backend for supported fan-in pipeline execution"},
    backend_feature{.name = "taskflow",
                    .execution_model = backend_execution_model::task_graph,
                    .support = backend_support::roadmap,
                    .external_dependency = true,
                    .default_build = false,
                    .compile_time_available = detail::taskflow_backend_compile_time_available,
                    .availability_macro = "PB_HAS_TASKFLOW",
                    .note = "planned optional adapter target; dependency is not required by core/runtime"},
    backend_feature{.name = "oneTBB",
                    .execution_model = backend_execution_model::filter_pipeline,
                    .support = backend_support::roadmap,
                    .external_dependency = true,
                    .default_build = false,
                    .compile_time_available = detail::tbb_backend_compile_time_available,
                    .availability_macro = "PB_HAS_TBB",
                    .note = "planned optional adapter target; dependency is not required by core/runtime"},
    backend_feature{.name = "stdexec",
                    .execution_model = backend_execution_model::sender_receiver,
                    .support = backend_support::experimental,
                    .external_dependency = true,
                    .default_build = false,
                    .compile_time_available = detail::stdexec_backend_compile_time_available,
                    .availability_macro = "PB_HAS_STDEXEC",
                    .note = "future sender/receiver adapter gated on compiler and library maturity"},
};

[[nodiscard]] constexpr auto backend_features() noexcept -> std::span<const backend_feature> {
  return {backend_feature_matrix.data(), backend_feature_matrix.size()};
}

[[nodiscard]] constexpr auto backend_supported(std::string_view name) noexcept -> bool {
  for (const auto& feature : backend_feature_matrix) {
    if (feature.name == name) {
      return feature.support == backend_support::supported;
    }
  }
  return false;
}

[[nodiscard]] constexpr auto backend_available(std::string_view name) noexcept -> bool {
  for (const auto& feature : backend_feature_matrix) {
    if (feature.name == name) {
      return feature.compile_time_available;
    }
  }
  return false;
}

} // namespace pb::runtime
