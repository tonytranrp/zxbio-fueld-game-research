#pragma once

#include <array>
#include <exception>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <type_traits>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include "pb/core/meta.hpp"
#include "pb/core/policy.hpp"
#include "pb/core/validate.hpp"
#include "pb/runtime/descriptor.hpp"
#include "pb/runtime/error.hpp"
#include "pb/runtime/error_policy.hpp"
#include "pb/runtime/observer.hpp"
#include "pb/runtime/result.hpp"

namespace pb::runtime {

struct construct_stages_per_run {};
struct store_stages_in_engine {};

template <class StageStoragePolicy>
struct sequential_policy {
  using stage_storage_policy = StageStoragePolicy;
};

struct sequential : sequential_policy<construct_stages_per_run> {};

using stateful_sequential = sequential_policy<store_stages_in_engine>;

namespace policy {
namespace storage {
using construct_per_run = construct_stages_per_run;
using store_in_engine = store_stages_in_engine;
} // namespace storage

template <class StageStoragePolicy>
using sequential = sequential_policy<StageStoragePolicy>;

using sequential_per_run = sequential<storage::construct_per_run>;
using sequential_stateful = sequential<storage::store_in_engine>;
} // namespace policy

namespace detail {
struct no_stage_storage {};

template <class StageStoragePolicy>
inline constexpr bool stores_stages_in_engine_v = std::same_as<StageStoragePolicy, store_stages_in_engine>;

template <class T>
struct is_variant : std::false_type {};

template <class... Ts>
struct is_variant<std::variant<Ts...>> : std::true_type {};

template <class Stage>
inline constexpr bool stage_declares_type_list_input_v =
    pb::core::detail::is_type_list<pb::core::stage_input_t<Stage>>::value;

template <class Stage, class StageObject, class Input>
[[nodiscard]] decltype(auto) invoke_type_list_join_stage(StageObject& stage_object, Input&& input) {
  if constexpr (is_variant<std::remove_cvref_t<Input>>::value) {
    return std::visit(
        [&stage_object](auto&& selected_output) -> decltype(auto) {
          return stage_object(std::forward<decltype(selected_output)>(selected_output));
        },
        std::forward<Input>(input));
  } else {
    return stage_object(std::forward<Input>(input));
  }
}

template <class BranchNode, std::size_t StageIndex, class Input>
[[nodiscard]] auto run_selected_branch(observer* sink, Input&& input) -> result<typename BranchNode::output_type>;

template <class BranchNode, std::size_t StageIndex, class Input>
[[nodiscard]] auto run_selected_branch(BranchNode* branch_storage, observer* sink, Input&& input)
    -> result<typename BranchNode::output_type>;

template <class BranchNode, std::size_t StageIndex, class Input>
[[nodiscard]] auto run_fan_in_branch(observer* sink, Input&& input) -> result<typename BranchNode::output_type>;

template <class BranchNode, std::size_t StageIndex, class Input>
[[nodiscard]] auto run_fan_in_branch(BranchNode* branch_storage, observer* sink, Input&& input)
    -> result<typename BranchNode::output_type>;

template <class Stage, std::size_t StageIndex, class StageStorage, class Input>
[[nodiscard]] decltype(auto) invoke_stage(StageStorage* storage, observer* sink, Input&& input) {
  if constexpr (pb::core::detail::is_fan_in_branch_node<Stage>::value) {
    if constexpr (std::same_as<std::remove_cvref_t<StageStorage>, no_stage_storage>) {
      return run_fan_in_branch<Stage, StageIndex>(sink, std::forward<Input>(input));
    } else {
      auto& branch_node = std::get<StageIndex>(*storage);
      return run_fan_in_branch<Stage, StageIndex>(&branch_node, sink, std::forward<Input>(input));
    }
  } else if constexpr (pb::core::detail::is_selected_branch_node<Stage>::value) {
    if constexpr (std::same_as<std::remove_cvref_t<StageStorage>, no_stage_storage>) {
      return run_selected_branch<Stage, StageIndex>(sink, std::forward<Input>(input));
    } else {
      auto& branch_node = std::get<StageIndex>(*storage);
      return run_selected_branch<Stage, StageIndex>(&branch_node, sink, std::forward<Input>(input));
    }
  } else if constexpr (stage_declares_type_list_input_v<Stage>) {
    if constexpr (std::same_as<std::remove_cvref_t<StageStorage>, no_stage_storage>) {
      auto stage = Stage{};
      return invoke_type_list_join_stage<Stage>(stage, std::forward<Input>(input));
    } else {
      auto& stage = std::get<StageIndex>(*storage);
      return invoke_type_list_join_stage<Stage>(stage, std::forward<Input>(input));
    }
  } else if constexpr (std::same_as<std::remove_cvref_t<StageStorage>, no_stage_storage>) {
    return Stage{}(std::forward<Input>(input));
  } else {
    return std::get<StageIndex>(*storage)(std::forward<Input>(input));
  }
}

template <class T>
struct is_result_type : std::false_type {};

template <class T, class E>
struct is_result_type<result<T, E>> : std::true_type {
  using error_type = E;
};

template <class Stage>
[[nodiscard]] auto stage_id_for(std::size_t stage_index = 0) -> stage_id {
  const auto stage_key = pb::core::stage_traits<Stage>::key();
  if (stage_key != "<unnamed>") {
    return stage_id{.key = std::string{stage_key}, .name = std::string{pb::core::stage_traits<Stage>::name()}};
  }
  return stage_id{.key = std::to_string(stage_index), .name = std::string{pb::core::stage_traits<Stage>::name()}};
}

template <class Stage>
[[nodiscard]] auto stage_id_for_fallback(std::string fallback_key) -> stage_id {
  const auto stage_key = pb::core::stage_traits<Stage>::key();
  if (stage_key != "<unnamed>") {
    return stage_id{.key = std::string{stage_key}, .name = std::string{pb::core::stage_traits<Stage>::name()}};
  }
  return stage_id{.key = std::move(fallback_key), .name = std::string{pb::core::stage_traits<Stage>::name()}};
}

template <class BranchNode, class Stage>
[[nodiscard]] auto branch_child_stage_id(std::size_t stage_index, std::size_t case_index,
                                         std::string_view role) -> stage_id {
  auto branch = stage_id_for<BranchNode>(stage_index);
  auto fallback_key = branch.key + ".case." + std::to_string(case_index) + "." + std::string{role};
  return stage_id_for_fallback<Stage>(std::move(fallback_key));
}

template <class Stage>
[[nodiscard]] auto stage_identity(std::size_t stage_index = 0) -> stage_id {
  return stage_id_for<Stage>(stage_index);
}

template <class... Stages, std::size_t... Indexes>
[[nodiscard]] auto stage_descriptors(std::index_sequence<Indexes...>) -> std::array<stage_id, sizeof...(Stages)> {
  return {stage_identity<Stages>(Indexes)...};
}

template <class... Stages>
[[nodiscard]] auto stage_descriptors() -> std::array<stage_id, sizeof...(Stages)> {
  return stage_descriptors<Stages...>(std::index_sequence_for<Stages...>{});
}

template <class Stage, class Error>
[[nodiscard]] auto observer_error_for(std::size_t stage_index, const Error& value, error_category fallback_category)
    -> error {
  using ErrorType = std::remove_cvref_t<Error>;
  if constexpr (std::same_as<ErrorType, error>) {
    auto annotated = value;
    if (!has_stage(annotated.stage)) {
      annotated.stage = stage_id_for<Stage>(stage_index);
    }
    return annotated;
  } else if constexpr (requires(const ErrorType& source) {
                         { source.diagnostic } -> std::convertible_to<error>;
                       }) {
    auto annotated = error{value.diagnostic};
    if (!has_stage(annotated.stage)) {
      annotated.stage = stage_id_for<Stage>(stage_index);
    }
    if (!has_message(annotated)) {
      annotated.message = detail::error_message_from(value);
    }
    return annotated;
  } else {
    return error{.stage = stage_id_for<Stage>(stage_index),
                 .category = fallback_category,
                 .message = detail::error_message_from(value)};
  }
}

template <class Error>
[[nodiscard]] auto observer_error_for(const stage_id& id, const Error& value, error_category fallback_category)
    -> error {
  using ErrorType = std::remove_cvref_t<Error>;
  if constexpr (std::same_as<ErrorType, error>) {
    auto annotated = value;
    if (!has_stage(annotated.stage)) {
      annotated.stage = id;
    }
    return annotated;
  } else if constexpr (requires(const ErrorType& source) {
                         { source.diagnostic } -> std::convertible_to<error>;
                       }) {
    auto annotated = error{value.diagnostic};
    if (!has_stage(annotated.stage)) {
      annotated.stage = id;
    }
    if (!has_message(annotated)) {
      annotated.message = detail::error_message_from(value);
    }
    return annotated;
  } else {
    return error{.stage = id, .category = fallback_category, .message = detail::error_message_from(value)};
  }
}

template <class Stage>
void notify_stage_start(observer* sink, std::size_t stage_index) {
  if (sink != nullptr) {
    sink->on_stage_start(stage_identity<Stage>(stage_index));
  }
}

inline void notify_stage_start(observer* sink, const stage_id& id) {
  if (sink != nullptr) {
    sink->on_stage_start(id);
  }
}

template <class Stage>
void notify_stage_success(observer* sink, std::size_t stage_index) {
  if (sink != nullptr) {
    sink->on_stage_success(stage_identity<Stage>(stage_index));
  }
}

inline void notify_stage_success(observer* sink, const stage_id& id) {
  if (sink != nullptr) {
    sink->on_stage_success(id);
  }
}

template <class Stage, class Error>
void notify_stage_failure(observer* sink, std::size_t stage_index, const Error& value) {
  if (sink != nullptr) {
    sink->on_stage_failure(stage_identity<Stage>(stage_index),
                           observer_error_for<Stage>(stage_index, value, error_category::stage_failure));
  }
}

template <class Error>
void notify_stage_failure(observer* sink, const stage_id& id, const Error& value) {
  if (sink != nullptr) {
    sink->on_stage_failure(id, observer_error_for(id, value, error_category::stage_failure));
  }
}

template <class Stage, class Error>
void notify_stage_exception(observer* sink, std::size_t stage_index, const Error& value) {
  if (sink != nullptr) {
    sink->on_stage_exception(stage_identity<Stage>(stage_index),
                             observer_error_for<Stage>(stage_index, value, error_category::exception));
  }
}

template <class Error>
void notify_stage_exception(observer* sink, const stage_id& id, const Error& value) {
  if (sink != nullptr) {
    sink->on_stage_exception(id, observer_error_for(id, value, error_category::exception));
  }
}

template <class Stage, class Error>
[[nodiscard]] auto annotate_stage_error(std::size_t stage_index, Error&& source) {
  using ErrorType = std::remove_cvref_t<Error>;
  if constexpr (std::same_as<ErrorType, error>) {
    auto annotated = std::forward<Error>(source);
    if (!has_stage(annotated.stage)) {
      annotated.stage = stage_id_for<Stage>(stage_index);
    }
    return annotated;
  } else if constexpr (requires(ErrorType value, error diagnostic) {
                         { value.diagnostic } -> std::convertible_to<error>;
                         value.diagnostic = std::move(diagnostic);
                       }) {
    auto annotated = std::forward<Error>(source);
    auto diagnostic = error{annotated.diagnostic};
    if (!has_stage(diagnostic.stage)) {
      diagnostic.stage = stage_id_for<Stage>(stage_index);
    }
    annotated.diagnostic = std::move(diagnostic);
    return annotated;
  } else {
    return std::forward<Error>(source);
  }
}

template <class Error>
[[nodiscard]] auto annotate_stage_error(const stage_id& id, Error&& source) {
  using ErrorType = std::remove_cvref_t<Error>;
  if constexpr (std::same_as<ErrorType, error>) {
    auto annotated = std::forward<Error>(source);
    if (!has_stage(annotated.stage)) {
      annotated.stage = id;
    }
    return annotated;
  } else if constexpr (requires(ErrorType value, error diagnostic) {
                         { value.diagnostic } -> std::convertible_to<error>;
                         value.diagnostic = std::move(diagnostic);
                       }) {
    auto annotated = std::forward<Error>(source);
    auto diagnostic = error{annotated.diagnostic};
    if (!has_stage(diagnostic.stage)) {
      diagnostic.stage = id;
    }
    annotated.diagnostic = std::move(diagnostic);
    return annotated;
  } else {
    return std::forward<Error>(source);
  }
}

template <class Stage, class Error>
[[nodiscard]] auto annotate_stage_error(Error&& source) {
  return annotate_stage_error<Stage>(0, std::forward<Error>(source));
}

template <class TargetError, class SourceError>
[[nodiscard]] auto convert_error(SourceError&& source) -> TargetError {
  if constexpr (std::constructible_from<TargetError, SourceError>) {
    return TargetError{std::forward<SourceError>(source)};
  } else {
    static_assert(std::same_as<TargetError, std::remove_cvref_t<SourceError>>,
                  "Pipeline stages that return result<T, E> must use a compatible error type");
  }
}

template <class FinalOutput, class Error, class Input>
[[nodiscard]] auto run_after_result(Input&& input) -> result<FinalOutput, Error> {
  return result<FinalOutput, Error>{std::forward<Input>(input)};
}

template <class Stage>
[[nodiscard]] auto exception_error(std::size_t stage_index, const std::exception& exception) -> error {
  return error{.stage = stage_id_for<Stage>(stage_index),
               .category = error_category::exception,
               .message = exception.what()};
}

[[nodiscard]] inline auto exception_error(const stage_id& id, const std::exception& exception) -> error {
  return error{.stage = id, .category = error_category::exception, .message = exception.what()};
}

template <class Stage>
[[nodiscard]] auto unknown_exception_error(std::size_t stage_index) -> error {
  return error{.stage = stage_id_for<Stage>(stage_index),
               .category = error_category::exception,
               .message = "stage threw an unknown exception"};
}

[[nodiscard]] inline auto unknown_exception_error(const stage_id& id) -> error {
  return error{.stage = id, .category = error_category::exception, .message = "stage threw an unknown exception"};
}

template <class Stage, class StageResult>
[[nodiscard]] auto normalize_stage_error(std::size_t stage_index, StageResult&& stage_result) -> error {
  auto normalized_error = detail::normalize_expected_error(std::forward<StageResult>(stage_result).error());
  return annotate_stage_error<Stage>(stage_index, std::move(normalized_error));
}

template <class StageResult>
[[nodiscard]] auto normalize_stage_error(const stage_id& id, StageResult&& stage_result) -> error {
  auto normalized_error = detail::normalize_expected_error(std::forward<StageResult>(stage_result).error());
  return annotate_stage_error(id, std::move(normalized_error));
}

template <class Stage, class StageResult>
[[nodiscard]] auto normalize_stage_error(StageResult&& stage_result) -> error {
  return normalize_stage_error<Stage>(0, std::move(stage_result));
}

template <class BranchNode, std::size_t CaseIndex, class Value>
[[nodiscard]] auto selected_branch_success(Value&& value) -> result<typename BranchNode::output_type> {
  using BranchOutput = typename BranchNode::output_type;
  if constexpr (BranchNode::homogeneous) {
    return result<BranchOutput>{std::forward<Value>(value)};
  } else {
    return result<BranchOutput>{BranchOutput{std::in_place_index<CaseIndex>, std::forward<Value>(value)}};
  }
}

template <class BranchNode, class Predicate, std::size_t StageIndex, class Input>
[[nodiscard]] auto evaluate_branch_predicate(observer* sink, const Input& input, std::size_t case_index)
    -> result<bool> {
  auto predicate_id = branch_child_stage_id<BranchNode, Predicate>(StageIndex, case_index, "predicate");
  try {
    notify_stage_start(sink, predicate_id);
    auto predicate_result = Predicate{}(input);
    if constexpr (expected_like<decltype(predicate_result)>) {
      auto normalized_result = to_result(std::move(predicate_result));
      if (!normalized_result.has_value()) {
        auto predicate_error = normalize_stage_error(predicate_id, std::move(normalized_result));
        notify_stage_failure(sink, predicate_id, predicate_error);
        return result<bool>{std::move(predicate_error)};
      }
      notify_stage_success(sink, predicate_id);
      return result<bool>{static_cast<bool>(std::move(normalized_result).value())};
    } else {
      notify_stage_success(sink, predicate_id);
      return result<bool>{static_cast<bool>(predicate_result)};
    }
  } catch (const std::exception& exception) {
    auto predicate_error = exception_error(predicate_id, exception);
    notify_stage_exception(sink, predicate_id, predicate_error);
    return result<bool>{std::move(predicate_error)};
  } catch (...) {
    auto predicate_error = unknown_exception_error(predicate_id);
    notify_stage_exception(sink, predicate_id, predicate_error);
    return result<bool>{std::move(predicate_error)};
  }
}

template <class BranchNode, class BranchStage, std::size_t StageIndex, std::size_t CaseIndex, class Input>
[[nodiscard]] auto run_branch_stage(observer* sink, Input&& input, std::size_t case_index)
    -> result<typename BranchNode::output_type> {
  auto branch_stage_id = branch_child_stage_id<BranchNode, BranchStage>(StageIndex, case_index, "stage");
  try {
    notify_stage_start(sink, branch_stage_id);
    auto branch_result = BranchStage{}(std::forward<Input>(input));
    if constexpr (expected_like<decltype(branch_result)>) {
      auto normalized_result = to_result(std::move(branch_result));
      if (!normalized_result.has_value()) {
        auto branch_error = normalize_stage_error(branch_stage_id, std::move(normalized_result));
        notify_stage_failure(sink, branch_stage_id, branch_error);
        return result<typename BranchNode::output_type>{std::move(branch_error)};
      }
      notify_stage_success(sink, branch_stage_id);
      return selected_branch_success<BranchNode, CaseIndex>(std::move(normalized_result).value());
    } else {
      notify_stage_success(sink, branch_stage_id);
      return selected_branch_success<BranchNode, CaseIndex>(std::move(branch_result));
    }
  } catch (const std::exception& exception) {
    auto branch_error = exception_error(branch_stage_id, exception);
    notify_stage_exception(sink, branch_stage_id, branch_error);
    return result<typename BranchNode::output_type>{std::move(branch_error)};
  } catch (...) {
    auto branch_error = unknown_exception_error(branch_stage_id);
    notify_stage_exception(sink, branch_stage_id, branch_error);
    return result<typename BranchNode::output_type>{std::move(branch_error)};
  }
}

template <class BranchNode, std::size_t StageIndex, class Input>
[[nodiscard]] auto unselected_branch_error() -> result<typename BranchNode::output_type> {
  return result<typename BranchNode::output_type>{
      error{.stage = stage_id_for<BranchNode>(StageIndex),
            .category = error_category::contract_violation,
            .message = "no branch predicate selected a case"}};
}

template <class BranchNode, std::size_t StageIndex, class Input, std::size_t CaseIndex, class Case, class... Rest>
[[nodiscard]] auto run_selected_branch_cases_with_storage(
    BranchNode* branch_storage, observer* sink, Input& input)
    -> result<typename BranchNode::output_type> {
  using Predicate = typename Case::predicate_type;
  using BranchStage = typename Case::stage_type;

  auto branch_id = stage_id_for<BranchNode>(StageIndex);
  auto predicate_id = branch_child_stage_id<BranchNode, Predicate>(StageIndex, CaseIndex, "predicate");
  auto branch_stage_id = branch_child_stage_id<BranchNode, BranchStage>(StageIndex, CaseIndex, "stage");

  auto& stored_predicate = std::get<CaseIndex>(branch_storage->predicates_);

  try {
    notify_stage_start(sink, predicate_id);
    auto predicate_result = stored_predicate(static_cast<const Input&>(input));
    if constexpr (expected_like<decltype(predicate_result)>) {
      auto normalized_result = to_result(std::move(predicate_result));
      if (!normalized_result.has_value()) {
        auto predicate_error = normalize_stage_error(predicate_id, std::move(normalized_result));
        notify_stage_failure(sink, predicate_id, predicate_error);
        return result<typename BranchNode::output_type>{std::move(predicate_error)};
      }
      notify_stage_success(sink, predicate_id);
      if (static_cast<bool>(std::move(normalized_result).value())) {
        if (sink) sink->on_case_selected(branch_id, CaseIndex, predicate_id);
        auto& stored_stage = std::get<CaseIndex>(branch_storage->branch_stages_);
        try {
          notify_stage_start(sink, branch_stage_id);
          auto branch_result = stored_stage(std::move(input));
          if constexpr (expected_like<decltype(branch_result)>) {
            auto normalized = to_result(std::move(branch_result));
            if (!normalized.has_value()) {
              auto branch_error = normalize_stage_error(branch_stage_id, std::move(normalized));
              notify_stage_failure(sink, branch_stage_id, branch_error);
              return result<typename BranchNode::output_type>{std::move(branch_error)};
            }
            notify_stage_success(sink, branch_stage_id);
            return selected_branch_success<BranchNode, CaseIndex>(std::move(normalized).value());
          } else {
            notify_stage_success(sink, branch_stage_id);
            return selected_branch_success<BranchNode, CaseIndex>(std::move(branch_result));
          }
        } catch (const std::exception& exception) {
          auto branch_error = exception_error(branch_stage_id, exception);
          notify_stage_exception(sink, branch_stage_id, branch_error);
          return result<typename BranchNode::output_type>{std::move(branch_error)};
        } catch (...) {
          auto branch_error = unknown_exception_error(branch_stage_id);
          notify_stage_exception(sink, branch_stage_id, branch_error);
          return result<typename BranchNode::output_type>{std::move(branch_error)};
        }
      }
    } else {
      notify_stage_success(sink, predicate_id);
      if (static_cast<bool>(predicate_result)) {
        if (sink) sink->on_case_selected(branch_id, CaseIndex, predicate_id);
        auto& stored_stage = std::get<CaseIndex>(branch_storage->branch_stages_);
        try {
          notify_stage_start(sink, branch_stage_id);
          auto branch_result = stored_stage(std::move(input));
          if constexpr (expected_like<decltype(branch_result)>) {
            auto normalized = to_result(std::move(branch_result));
            if (!normalized.has_value()) {
              auto branch_error = normalize_stage_error(branch_stage_id, std::move(normalized));
              notify_stage_failure(sink, branch_stage_id, branch_error);
              return result<typename BranchNode::output_type>{std::move(branch_error)};
            }
            notify_stage_success(sink, branch_stage_id);
            return selected_branch_success<BranchNode, CaseIndex>(std::move(normalized).value());
          } else {
            notify_stage_success(sink, branch_stage_id);
            return selected_branch_success<BranchNode, CaseIndex>(std::move(branch_result));
          }
        } catch (const std::exception& exception) {
          auto branch_error = exception_error(branch_stage_id, exception);
          notify_stage_exception(sink, branch_stage_id, branch_error);
          return result<typename BranchNode::output_type>{std::move(branch_error)};
        } catch (...) {
          auto branch_error = unknown_exception_error(branch_stage_id);
          notify_stage_exception(sink, branch_stage_id, branch_error);
          return result<typename BranchNode::output_type>{std::move(branch_error)};
        }
      }
    }
    if (sink) sink->on_case_skipped(branch_id, CaseIndex, predicate_id);
    if constexpr (sizeof...(Rest) == 0) {
      return unselected_branch_error<BranchNode, StageIndex, Input>();
    } else {
      return run_selected_branch_cases_with_storage<BranchNode, StageIndex, Input, CaseIndex + 1, Rest...>(
          branch_storage, sink, input);
    }
  } catch (const std::exception& exception) {
    auto predicate_error = exception_error(predicate_id, exception);
    notify_stage_exception(sink, predicate_id, predicate_error);
    return result<typename BranchNode::output_type>{std::move(predicate_error)};
  } catch (...) {
    auto predicate_error = unknown_exception_error(predicate_id);
    notify_stage_exception(sink, predicate_id, predicate_error);
    return result<typename BranchNode::output_type>{std::move(predicate_error)};
  }
}

template <class BranchNode, std::size_t StageIndex, class Input, class... Cases>
[[nodiscard]] auto run_selected_branch(observer* sink, Input& input, pb::meta::type_list<Cases...>)
    -> result<typename BranchNode::output_type> {
  using OutputType = typename BranchNode::output_type;
  bool matched = false;
  bool has_error = false;
  result<OutputType> output = unselected_branch_error<BranchNode, StageIndex, Input>();

  auto process = [&]<std::size_t CaseIndex, class Case>() {
    if (matched || has_error) return;
    using Predicate = typename Case::predicate_type;
    using BranchStage = typename Case::stage_type;

    auto branch_id = stage_id_for<BranchNode>(StageIndex);
    auto predicate_id = branch_child_stage_id<BranchNode, Predicate>(StageIndex, CaseIndex, "predicate");

    auto predicate_result = evaluate_branch_predicate<BranchNode, Predicate, StageIndex>(sink, input, CaseIndex);
    if (!predicate_result.has_value()) {
      has_error = true;
      output = result<OutputType>{std::move(predicate_result).error()};
      return;
    }
    if (predicate_result.value()) {
      matched = true;
      if (sink) sink->on_case_selected(branch_id, CaseIndex, predicate_id);
      output = run_branch_stage<BranchNode, BranchStage, StageIndex, CaseIndex>(sink, std::move(input), CaseIndex);
      return;
    }
    if (sink) sink->on_case_skipped(branch_id, CaseIndex, predicate_id);
  };

  [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    (process.template operator()<Is, Cases>(), ...);
  }(std::index_sequence_for<Cases...>{});

  return output;
}

template <class BranchNode, std::size_t StageIndex, class Input>
[[nodiscard]] auto run_selected_branch(observer* sink, Input&& input) -> result<typename BranchNode::output_type> {
  auto result = run_selected_branch<BranchNode, StageIndex>(sink, input, typename BranchNode::cases{});
  if (!result.has_value() && has_message(result.error())) {
    auto& err = result.error();
    err.message = std::string{"["} + std::string{BranchNode::stage_name()} + "] " + err.message;
  }
  return result;
}

template <class BranchNode, std::size_t StageIndex, class Input, class... Cases>
[[nodiscard]] auto run_selected_branch_cases_with_storage(
    BranchNode* branch_storage, observer* sink, Input& input, pb::meta::type_list<Cases...>)
    -> result<typename BranchNode::output_type> {
  return run_selected_branch_cases_with_storage<BranchNode, StageIndex, Input, 0, Cases...>(branch_storage, sink, input);
}

template <class BranchNode, std::size_t StageIndex, class Input>
[[nodiscard]] auto run_selected_branch(BranchNode* branch_storage, observer* sink, Input&& input)
    -> result<typename BranchNode::output_type> {
  auto result = run_selected_branch_cases_with_storage<BranchNode, StageIndex, Input>(
      branch_storage, sink, input, typename BranchNode::cases{});
  if (!result.has_value() && has_message(result.error())) {
    auto& err = result.error();
    err.message = std::string{"["} + std::string{BranchNode::stage_name()} + "] " + err.message;
  }
  return result;
}

template <class BranchNode, class Predicate, class PredicateObject, std::size_t StageIndex, class Input>
[[nodiscard]] auto evaluate_fan_in_predicate(PredicateObject& predicate, observer* sink, const Input& input,
                                             std::size_t case_index) -> result<bool> {
  auto predicate_id = branch_child_stage_id<BranchNode, Predicate>(StageIndex, case_index, "predicate");
  try {
    notify_stage_start(sink, predicate_id);
    auto predicate_result = predicate(input);
    if constexpr (expected_like<decltype(predicate_result)>) {
      auto normalized_result = to_result(std::move(predicate_result));
      if (!normalized_result.has_value()) {
        auto predicate_error = normalize_stage_error(predicate_id, std::move(normalized_result));
        notify_stage_failure(sink, predicate_id, predicate_error);
        return result<bool>{std::move(predicate_error)};
      }
      notify_stage_success(sink, predicate_id);
      return result<bool>{static_cast<bool>(std::move(normalized_result).value())};
    } else {
      notify_stage_success(sink, predicate_id);
      return result<bool>{static_cast<bool>(predicate_result)};
    }
  } catch (const std::exception& exception) {
    auto predicate_error = exception_error(predicate_id, exception);
    notify_stage_exception(sink, predicate_id, predicate_error);
    return result<bool>{std::move(predicate_error)};
  } catch (...) {
    auto predicate_error = unknown_exception_error(predicate_id);
    notify_stage_exception(sink, predicate_id, predicate_error);
    return result<bool>{std::move(predicate_error)};
  }
}

template <class BranchNode, class BranchStage, class BranchStageObject, std::size_t StageIndex,
          std::size_t CaseIndex, class Arg, class Aggregate>
[[nodiscard]] auto run_fan_in_case_stage_with_arg(BranchStageObject& branch_stage, observer* sink,
                                                  const stage_id& branch_id, Arg&& arg, Aggregate& aggregate)
    -> result<void> {
  auto branch_stage_id = branch_child_stage_id<BranchNode, BranchStage>(StageIndex, CaseIndex, "stage");
  try {
    notify_stage_start(sink, branch_stage_id);
    using InvokeResult = std::invoke_result_t<BranchStageObject&, Arg>;
    if constexpr (std::is_void_v<InvokeResult>) {
      branch_stage(std::forward<Arg>(arg));
      auto& slot = aggregate.template get<CaseIndex>();
      slot.mark_completed();
      notify_stage_success(sink, branch_stage_id);
      return {};
    } else {
      auto branch_result = branch_stage(std::forward<Arg>(arg));
      if constexpr (expected_like<decltype(branch_result)>) {
        auto normalized_result = to_result(std::move(branch_result));
        using NormalizedResult = std::remove_cvref_t<decltype(normalized_result)>;
        using ValueType = typename NormalizedResult::value_type;
        if (!normalized_result.has_value()) {
          auto branch_error = normalize_stage_error(branch_stage_id, std::move(normalized_result));
          auto& slot = aggregate.template get<CaseIndex>();
          slot.mark_failed(branch_error.message);
          notify_stage_failure(sink, branch_stage_id, branch_error);
          if (sink) sink->on_case_failed(branch_id, CaseIndex, branch_stage_id, branch_error);
          return {};
        }
        auto& slot = aggregate.template get<CaseIndex>();
        if constexpr (std::is_void_v<ValueType>) {
          slot.mark_completed();
        } else {
          slot.mark_completed(std::move(normalized_result).value());
        }
        notify_stage_success(sink, branch_stage_id);
        return {};
      } else {
        auto& slot = aggregate.template get<CaseIndex>();
        slot.mark_completed(std::move(branch_result));
        notify_stage_success(sink, branch_stage_id);
        return {};
      }
    }
  } catch (const std::exception& exception) {
    auto branch_error = exception_error(branch_stage_id, exception);
    auto& slot = aggregate.template get<CaseIndex>();
    slot.mark_failed(branch_error.message);
    notify_stage_exception(sink, branch_stage_id, branch_error);
    if (sink) sink->on_case_failed(branch_id, CaseIndex, branch_stage_id, branch_error);
    return {};
  } catch (...) {
    auto branch_error = unknown_exception_error(branch_stage_id);
    auto& slot = aggregate.template get<CaseIndex>();
    slot.mark_failed(branch_error.message);
    notify_stage_exception(sink, branch_stage_id, branch_error);
    if (sink) sink->on_case_failed(branch_id, CaseIndex, branch_stage_id, branch_error);
    return {};
  }
}

template <class BranchNode, class BranchStage, class BranchStageObject, std::size_t StageIndex,
          std::size_t CaseIndex, class Input, class Aggregate>
[[nodiscard]] auto run_fan_in_case_stage(BranchStageObject& branch_stage, observer* sink,
                                         const stage_id& branch_id, const Input& input, Aggregate& aggregate)
    -> result<void> {
  if constexpr (std::copy_constructible<Input>) {
    auto branch_input = Input{input};
    return run_fan_in_case_stage_with_arg<BranchNode, BranchStage, BranchStageObject, StageIndex, CaseIndex>(
        branch_stage, sink, branch_id, std::move(branch_input), aggregate);
  } else {
    return run_fan_in_case_stage_with_arg<BranchNode, BranchStage, BranchStageObject, StageIndex, CaseIndex>(
        branch_stage, sink, branch_id, input, aggregate);
  }
}

template <class Aggregate, std::size_t CaseIndex>
void mark_fan_in_case_failed(Aggregate& aggregate, const error& diagnostic) {
  auto& slot = aggregate.template get<CaseIndex>();
  slot.mark_failed(diagnostic.message);
}

/// Running tally of fan-in case outcomes, used to populate the
/// `on_fan_in_completed` observer event.  Shared by both backends.
struct fan_in_case_tally {
  std::size_t selected{};
  std::size_t completed{};
  std::size_t failed{};
};

template <bool UseStorage, class BranchNode, std::size_t StageIndex, class Input, class Aggregate,
          std::size_t CaseIndex, class Case, class... Rest>
[[nodiscard]] auto run_fan_in_cases(BranchNode* branch_storage, observer* sink, const Input& input,
                                    Aggregate& aggregate, fan_in_case_tally& tally) -> result<void> {
  using Predicate = typename Case::predicate_type;
  using BranchStage = typename Case::stage_type;

  auto branch_id = stage_id_for<BranchNode>(StageIndex);
  auto predicate_id = branch_child_stage_id<BranchNode, Predicate>(StageIndex, CaseIndex, "predicate");
  auto branch_stage_id = branch_child_stage_id<BranchNode, BranchStage>(StageIndex, CaseIndex, "stage");

  auto predicate_result = [&] {
    if constexpr (UseStorage) {
      auto& stored_predicate = std::get<CaseIndex>(branch_storage->predicates_);
      return evaluate_fan_in_predicate<BranchNode, Predicate, decltype(stored_predicate), StageIndex>(
          stored_predicate, sink, input, CaseIndex);
    } else {
      auto predicate = Predicate{};
      return evaluate_fan_in_predicate<BranchNode, Predicate, decltype(predicate), StageIndex>(
          predicate, sink, input, CaseIndex);
    }
  }();
  if (!predicate_result.has_value()) {
    auto predicate_error = std::move(predicate_result).error();
    mark_fan_in_case_failed<Aggregate, CaseIndex>(aggregate, predicate_error);
    ++tally.failed;
    if (sink) sink->on_case_failed(branch_id, CaseIndex, predicate_id, predicate_error);
  } else if (predicate_result.value()) {
    ++tally.selected;
    if (sink) sink->on_case_selected(branch_id, CaseIndex, predicate_id);
    if (sink) sink->on_fan_in_case_scheduled(branch_id, CaseIndex, branch_stage_id);
    auto stage_result = [&] {
      if constexpr (UseStorage) {
        auto& stored_stage = std::get<CaseIndex>(branch_storage->branch_stages_);
        return run_fan_in_case_stage<BranchNode, BranchStage, decltype(stored_stage), StageIndex, CaseIndex>(
            stored_stage, sink, branch_id, input, aggregate);
      } else {
        auto branch_stage = BranchStage{};
        return run_fan_in_case_stage<BranchNode, BranchStage, decltype(branch_stage), StageIndex, CaseIndex>(
            branch_stage, sink, branch_id, input, aggregate);
      }
    }();
    const bool case_succeeded = !aggregate.template get<CaseIndex>().failed();
    if (case_succeeded) {
      ++tally.completed;
    } else {
      ++tally.failed;
    }
    if (sink) sink->on_fan_in_case_completed(branch_id, CaseIndex, branch_stage_id, case_succeeded);
    if (!stage_result.has_value()) {
      return stage_result;
    }
  } else {
    if (sink) sink->on_case_skipped(branch_id, CaseIndex, predicate_id);
  }

  if constexpr (sizeof...(Rest) == 0) {
    return {};
  } else {
    return run_fan_in_cases<UseStorage, BranchNode, StageIndex, Input, Aggregate, CaseIndex + 1, Rest...>(
        branch_storage, sink, input, aggregate, tally);
  }
}

template <bool UseStorage, class BranchNode, std::size_t StageIndex, class Input, class... Cases>
[[nodiscard]] auto run_fan_in_branch_impl(BranchNode* branch_storage, observer* sink, const Input& input,
                                          pb::meta::type_list<Cases...>)
    -> result<typename BranchNode::output_type> {
  typename BranchNode::output_type aggregate{};
  auto branch_id = stage_id_for<BranchNode>(StageIndex);
  if (sink) sink->on_fan_in_started(branch_id, sizeof...(Cases));
  fan_in_case_tally tally{};
  if constexpr (sizeof...(Cases) == 0) {
    if (sink) sink->on_fan_in_completed(branch_id, tally.selected, tally.completed, tally.failed);
    return result<typename BranchNode::output_type>{std::move(aggregate)};
  } else {
    auto cases_result = run_fan_in_cases<UseStorage, BranchNode, StageIndex, Input, typename BranchNode::output_type, 0,
                                         Cases...>(branch_storage, sink, input, aggregate, tally);
    if (sink) sink->on_fan_in_completed(branch_id, tally.selected, tally.completed, tally.failed);
    if (!cases_result.has_value()) {
      return result<typename BranchNode::output_type>{std::move(cases_result).error()};
    }
    return result<typename BranchNode::output_type>{std::move(aggregate)};
  }
}

template <class BranchNode, std::size_t StageIndex, class Input>
[[nodiscard]] auto run_fan_in_branch(observer* sink, Input&& input) -> result<typename BranchNode::output_type> {
  auto result =
      run_fan_in_branch_impl<false, BranchNode, StageIndex>(nullptr, sink, input, typename BranchNode::cases{});
  if (!result.has_value() && has_message(result.error())) {
    auto& err = result.error();
    err.message = std::string{"["} + std::string{BranchNode::stage_name()} + "] " + err.message;
  }
  return result;
}

template <class BranchNode, std::size_t StageIndex, class Input>
[[nodiscard]] auto run_fan_in_branch(BranchNode* branch_storage, observer* sink, Input&& input)
    -> result<typename BranchNode::output_type> {
  auto result = run_fan_in_branch_impl<true, BranchNode, StageIndex>(branch_storage, sink, input,
                                                                     typename BranchNode::cases{});
  if (!result.has_value() && has_message(result.error())) {
    auto& err = result.error();
    err.message = std::string{"["} + std::string{BranchNode::stage_name()} + "] " + err.message;
  }
  return result;
}

template <std::size_t StageIndex, class FinalOutput, class StageStorage, class Input, class Stage, class... Rest>
[[nodiscard]] auto try_run_after_result(observer* sink, StageStorage* storage, Input&& input) -> result<FinalOutput> {
  try {
    notify_stage_start<Stage>(sink, StageIndex);
    auto stage_result = invoke_stage<Stage, StageIndex>(storage, sink, std::forward<Input>(input));
    if constexpr (expected_like<decltype(stage_result)>) {
      auto normalized_result = to_result(std::move(stage_result));
      if (!normalized_result.has_value()) {
        auto normalized_error = normalize_stage_error<Stage>(StageIndex, std::move(normalized_result));
        notify_stage_failure<Stage>(sink, StageIndex, normalized_error);
        return result<FinalOutput>{std::move(normalized_error)};
      }
      notify_stage_success<Stage>(sink, StageIndex);
      if constexpr (sizeof...(Rest) == 0) {
        if constexpr (std::is_void_v<FinalOutput>) {
          return result<FinalOutput>{};
        } else {
          return result<FinalOutput>{std::move(normalized_result).value()};
        }
      } else {
        return try_run_after_result<StageIndex + 1, FinalOutput, StageStorage,
                                    decltype(std::move(normalized_result).value()), Rest...>(
            sink, storage, std::move(normalized_result).value());
      }
    } else if constexpr (sizeof...(Rest) == 0) {
      notify_stage_success<Stage>(sink, StageIndex);
      return result<FinalOutput>{std::move(stage_result)};
    } else {
      notify_stage_success<Stage>(sink, StageIndex);
      return try_run_after_result<StageIndex + 1, FinalOutput, StageStorage, decltype(stage_result), Rest...>(
          sink, storage, std::move(stage_result));
    }
  } catch (const std::exception& exception) {
    auto stage_error = exception_error<Stage>(StageIndex, exception);
    notify_stage_exception<Stage>(sink, StageIndex, stage_error);
    return result<FinalOutput>{std::move(stage_error)};
  } catch (...) {
    auto stage_error = unknown_exception_error<Stage>(StageIndex);
    notify_stage_exception<Stage>(sink, StageIndex, stage_error);
    return result<FinalOutput>{std::move(stage_error)};
  }
}

template <class FinalOutput, class StageStorage, class Input, class Stage, class... Rest>
[[nodiscard]] auto try_run_after_result(observer* sink, StageStorage* storage, Input&& input) -> result<FinalOutput> {
  return try_run_after_result<0, FinalOutput, StageStorage, Input, Stage, Rest...>(sink, storage,
                                                                                  std::forward<Input>(input));
}

template <std::size_t StageIndex, class FinalOutput, class Error, class StageStorage, class Input, class Stage, class... Rest>
[[nodiscard]] auto run_after_result(observer* sink, StageStorage* storage, Input&& input) -> result<FinalOutput, Error> {
  try {
    notify_stage_start<Stage>(sink, StageIndex);
    auto stage_result = invoke_stage<Stage, StageIndex>(storage, sink, std::forward<Input>(input));
    if constexpr (expected_like<decltype(stage_result)>) {
      auto normalized_result = to_result(std::move(stage_result));
      if (!normalized_result.has_value()) {
        auto normalized_error =
            convert_error<Error>(annotate_stage_error<Stage>(StageIndex, std::move(normalized_result).error()));
        notify_stage_failure<Stage>(sink, StageIndex, normalized_error);
        return result<FinalOutput, Error>{std::move(normalized_error)};
      }
      notify_stage_success<Stage>(sink, StageIndex);
      if constexpr (sizeof...(Rest) == 0) {
        if constexpr (std::is_void_v<FinalOutput>) {
          return result<FinalOutput, Error>{};
        } else {
          return result<FinalOutput, Error>{std::move(normalized_result).value()};
        }
      } else {
        return run_after_result<StageIndex + 1, FinalOutput, Error, StageStorage,
                                decltype(std::move(normalized_result).value()), Rest...>(
            sink, storage, std::move(normalized_result).value());
      }
    } else if constexpr (sizeof...(Rest) == 0) {
      notify_stage_success<Stage>(sink, StageIndex);
      return result<FinalOutput, Error>{std::move(stage_result)};
    } else {
      notify_stage_success<Stage>(sink, StageIndex);
      return run_after_result<StageIndex + 1, FinalOutput, Error, StageStorage, decltype(stage_result), Rest...>(
          sink, storage, std::move(stage_result));
    }
  } catch (const std::exception& exception) {
    auto stage_error = exception_error<Stage>(StageIndex, exception);
    notify_stage_exception<Stage>(sink, StageIndex, stage_error);
    if constexpr (std::constructible_from<Error, decltype(stage_error)>) {
      return result<FinalOutput, Error>{Error{std::move(stage_error)}};
    } else {
      throw;
    }
  } catch (...) {
    auto stage_error = unknown_exception_error<Stage>(StageIndex);
    notify_stage_exception<Stage>(sink, StageIndex, stage_error);
    if constexpr (std::constructible_from<Error, decltype(stage_error)>) {
      return result<FinalOutput, Error>{Error{std::move(stage_error)}};
    } else {
      throw;
    }
  }
}

template <class FinalOutput, class Error, class StageStorage, class Input, class Stage, class... Rest>
[[nodiscard]] auto run_after_result(observer* sink, StageStorage* storage, Input&& input) -> result<FinalOutput, Error> {
  return run_after_result<0, FinalOutput, Error, StageStorage, Input, Stage, Rest...>(sink, storage,
                                                                                     std::forward<Input>(input));
}

template <std::size_t StageIndex, class FinalOutput, class StageStorage, class Input, class Stage, class... Rest>
[[nodiscard]] auto run_stages(observer* sink, StageStorage* storage, Input&& input) {
  auto stage_result = [&] {
    try {
      notify_stage_start<Stage>(sink, StageIndex);
      return invoke_stage<Stage, StageIndex>(storage, sink, std::forward<Input>(input));
    } catch (const std::exception& exception) {
      auto stage_error = exception_error<Stage>(StageIndex, exception);
      notify_stage_exception<Stage>(sink, StageIndex, stage_error);
      throw;
    } catch (...) {
      auto stage_error = unknown_exception_error<Stage>(StageIndex);
      notify_stage_exception<Stage>(sink, StageIndex, stage_error);
      throw;
    }
  }();

  if constexpr (expected_like<decltype(stage_result)>) {
    auto normalized_result = to_result(std::move(stage_result));
    using StageResult = std::remove_cvref_t<decltype(normalized_result)>;
    using Error = typename StageResult::error_type;
    if (!normalized_result.has_value()) {
      auto stage_error = annotate_stage_error<Stage>(StageIndex, std::move(normalized_result).error());
      notify_stage_failure<Stage>(sink, StageIndex, stage_error);
      return result<FinalOutput, Error>{std::move(stage_error)};
    }
    notify_stage_success<Stage>(sink, StageIndex);
    if constexpr (sizeof...(Rest) == 0) {
      if constexpr (std::is_void_v<FinalOutput>) {
        return result<FinalOutput, Error>{};
      } else {
        return result<FinalOutput, Error>{std::move(normalized_result).value()};
      }
    } else {
      return run_after_result<StageIndex + 1, FinalOutput, Error, StageStorage,
                              decltype(std::move(normalized_result).value()), Rest...>(
          sink, storage, std::move(normalized_result).value());
    }
  } else if constexpr (sizeof...(Rest) == 0) {
    notify_stage_success<Stage>(sink, StageIndex);
    return stage_result;
  } else {
    notify_stage_success<Stage>(sink, StageIndex);
    return run_stages<StageIndex + 1, FinalOutput, StageStorage, decltype(stage_result), Rest...>(
        sink, storage, std::move(stage_result));
  }
}

template <class FinalOutput, class StageStorage, class Input, class Stage, class... Rest>
[[nodiscard]] auto run_stages(observer* sink, StageStorage* storage, Input&& input) -> decltype(auto) {
  return run_stages<0, FinalOutput, StageStorage, Input, Stage, Rest...>(sink, storage, std::forward<Input>(input));
}

template <class Pipeline, class StageStoragePolicy>
class sequential_engine;

template <class Input, class Output, class... Stages, class Policies, class StageStoragePolicy>
class sequential_engine<pb::core::pipeline<Input, Output, pb::meta::type_list<Stages...>, Policies>,
                        StageStoragePolicy> {
public:
  using pipeline_type = pb::core::pipeline<Input, Output, pb::meta::type_list<Stages...>, Policies>;
  using input_type = Input;
  using output_type = Output;
  using stages = pb::meta::type_list<Stages...>;
  using stage_storage_policy = StageStoragePolicy;
  using try_result_type = result<Output>;
  using descriptor_type = std::array<stage_id, sizeof...(Stages)>;
  using stage_storage_type =
      std::conditional_t<stores_stages_in_engine_v<StageStoragePolicy>, std::tuple<Stages...>, no_stage_storage>;
  using runtime_descriptor_type = decltype(make_descriptor<pipeline_type>());

  static constexpr auto stage_count = sizeof...(Stages);

  void set_observer(observer* value) noexcept { observer_ = value; }

  [[nodiscard]] observer* get_observer() const noexcept { return observer_; }

  [[nodiscard]] auto describe() const -> descriptor_type { return detail::stage_descriptors<Stages...>(); }

  [[nodiscard]] constexpr auto descriptor() const noexcept -> runtime_descriptor_type {
    return make_descriptor<pipeline_type>();
  }

  [[nodiscard]] decltype(auto) run(Input input) const {
    if constexpr (sizeof...(Stages) == 0) {
      return input;
    } else if constexpr (
        [] {
          using RunStagesResult = decltype(detail::run_stages<Output, stage_storage_type, Input, Stages...>(
              nullptr, std::declval<stage_storage_type*>(), std::declval<Input>()));
          return detail::is_result_type<RunStagesResult>::value;
        }()) {
      using RunStagesResult = decltype(detail::run_stages<Output, stage_storage_type, Input, Stages...>(
          std::declval<observer*>(), std::declval<stage_storage_type*>(), std::declval<Input>()));
      return detail::run_after_result<Output, typename detail::is_result_type<RunStagesResult>::error_type,
                                      stage_storage_type, Input, Stages...>(observer_, &stages_, std::move(input));
    } else {
      return run_stages<Output, stage_storage_type, Input, Stages...>(observer_, &stages_, std::move(input));
    }
  }

  [[nodiscard]] auto try_run(Input input) const -> result<Output> {
    if constexpr (sizeof...(Stages) == 0) {
      return result<Output>{std::move(input)};
    } else {
      return try_run_after_result<Output, stage_storage_type, Input, Stages...>(observer_, &stages_, std::move(input));
    }
  }

  template <class Iterator, class Sentinel>
  [[nodiscard]] auto try_run_range(Iterator first, Sentinel last) const -> std::vector<result<Output>> {
    std::vector<result<Output>> results;
    if constexpr (std::sized_sentinel_for<Sentinel, Iterator>) {
      const auto count = last - first;
      if (count > 0) {
        results.reserve(static_cast<std::size_t>(count));
      }
    }
    for (; first != last; ++first) {
      results.push_back(try_run(Input{*first}));
    }
    return results;
  }

  template <class Range>
  [[nodiscard]] auto try_run_each(Range&& inputs) const -> std::vector<result<Output>>
    requires requires(Range&& range) {
      std::begin(range);
      std::end(range);
    }
  {
    std::vector<result<Output>> results;
    if constexpr (requires { std::size(inputs); }) {
      results.reserve(static_cast<std::size_t>(std::size(inputs)));
    }
    auto first = std::begin(inputs);
    auto last = std::end(inputs);
    for (; first != last; ++first) {
      if constexpr (!std::is_lvalue_reference_v<Range&&> &&
                    !std::is_const_v<std::remove_reference_t<decltype(*first)>>) {
        results.push_back(try_run(Input{std::move(*first)}));
      } else {
        results.push_back(try_run(Input{*first}));
      }
    }
    return results;
  }

private:
  observer* observer_{nullptr};
  mutable stage_storage_type stages_{};
};

} // namespace detail

// ---------------------------------------------------------------------------
// Error-policy extraction from a finalized pipeline's carried policy list.
//
// A pipeline produced via `pb::from<In>::with<pb::policy::errors::...>::...`
// records the marker in `Pipeline::policies` (a `pb::meta::type_list<...>`).
// The helpers below locate the first `pb::policy::errors` marker for which
// `pb::policy::is_error_policy_v` is true, and `compile<P>(sequential{})`
// uses it to select an engine wrapper from `pb/runtime/error_policy.hpp`.
// Pipelines without such a marker compile to the bare engine, byte-for-byte
// identical to the pre-feature behaviour.
// ---------------------------------------------------------------------------

namespace detail {

/// Scan a policy `type_list` for the first marker satisfying the predicate
/// trait `Pred` (e.g. `pb::policy::is_error_policy`).  Resolves to `void` when
/// no element matches.  This single template backs the error / diagnostics /
/// copying policy lookups below.
template <template <class> class Pred, class List>
struct first_policy_matching {
  using type = void;
};

template <template <class> class Pred, class Head, class... Tail>
struct first_policy_matching<Pred, pb::meta::type_list<Head, Tail...>> {
  using type = std::conditional_t<
      Pred<Head>::value, Head,
      typename first_policy_matching<Pred, pb::meta::type_list<Tail...>>::type>;
};

template <class List>
using first_error_policy_t = typename first_policy_matching<pb::policy::is_error_policy, List>::type;
template <class List>
using first_diagnostics_policy_t = typename first_policy_matching<pb::policy::is_diagnostics_policy, List>::type;
template <class List>
using first_copying_policy_t = typename first_policy_matching<pb::policy::is_copying_policy, List>::type;

template <class Pipeline, class = void>
struct pipeline_policies {
  using type = pb::meta::type_list<>;
};

template <class Pipeline>
struct pipeline_policies<Pipeline, std::void_t<typename Pipeline::policies>> {
  using type = typename Pipeline::policies;
};

} // namespace detail

/// The error-policy marker carried by `Pipeline` (one of the five
/// `pb::policy::errors` runtime markers), or `void` when none is present.
template <class Pipeline>
using pipeline_error_policy_t =
    detail::first_error_policy_t<typename detail::pipeline_policies<Pipeline>::type>;

/// True when `Pipeline` carries one of the runtime-enforced
/// `pb::policy::errors` markers in its policy list.
template <class Pipeline>
inline constexpr bool has_error_policy_v = !std::is_same_v<pipeline_error_policy_t<Pipeline>, void>;

/// The diagnostics-policy marker carried by `Pipeline` (one of the
/// `pb::policy::diagnostics` runtime markers — `verbose` or `quiet`), or
/// `void` when none is present.
template <class Pipeline>
using pipeline_diagnostics_policy_t =
    detail::first_diagnostics_policy_t<typename detail::pipeline_policies<Pipeline>::type>;

/// True when `Pipeline` carries one of the runtime-enforced
/// `pb::policy::diagnostics` markers (`verbose` / `quiet`) in its policy list.
template <class Pipeline>
inline constexpr bool has_diagnostics_policy_v =
    !std::is_same_v<pipeline_diagnostics_policy_t<Pipeline>, void>;

/// The copying-policy marker carried by `Pipeline` (one of the
/// `pb::policy::copying` markers — `value` / `move_only` / `shared` / `clone`),
/// or `void` when none is present.
///
/// The copying axis is carried + queryable but only partially enforced: this
/// extracts the pinned intent so tooling / user `static_assert`s can read it.
/// `compile<>` does not change runtime copy/move behaviour based on it.
template <class Pipeline>
using pipeline_copying_policy_t =
    detail::first_copying_policy_t<typename detail::pipeline_policies<Pipeline>::type>;

/// True when `Pipeline` carries one of the `pb::policy::copying` markers in its
/// policy list.
template <class Pipeline>
inline constexpr bool has_copying_policy_v =
    !std::is_same_v<pipeline_copying_policy_t<Pipeline>, void>;

namespace detail {

template <pb::core::ValidPipeline Pipeline, class StageStoragePolicy>
[[nodiscard]] constexpr auto apply_error_policy(sequential_engine<Pipeline, StageStoragePolicy> engine) {
  using ErrorPolicy = pipeline_error_policy_t<Pipeline>;
  using Output = typename sequential_engine<Pipeline, StageStoragePolicy>::output_type;
  if constexpr (std::is_same_v<ErrorPolicy, pb::policy::errors::throwing>) {
    return pb::with_throw_on_error(std::move(engine));
  } else if constexpr (std::is_same_v<ErrorPolicy, pb::policy::errors::terminating>) {
    return pb::with_terminate_on_error(std::move(engine));
  } else if constexpr (std::is_same_v<ErrorPolicy, pb::policy::errors::ignoring>) {
    static_assert(!std::is_void_v<Output>,
                  "pb::policy::errors::ignoring requires a non-void pipeline output_type — the ignore "
                  "policy substitutes a fallback value, which is meaningless for void-returning pipelines");
    // The DSL ignore policy has no place to attach a user-provided fallback,
    // so it uses a value-initialized output as the fallback.  Callers who need
    // a custom fallback should wrap explicitly with pb::with_ignore_errors.
    return pb::with_ignore_errors(std::move(engine), Output{});
  } else {
    // propagating / result / no marker -> bare engine, unchanged.
    return engine;
  }
}

/// Compose the diagnostics axis on top of the (possibly error-policy-wrapped)
/// engine.  When `Pipeline` carries `pb::policy::diagnostics::verbose`, the
/// engine is additionally wrapped in `pb::with_verbose_diagnostics(...)` so the
/// verbose wrapper sits *around* any error-policy wrapper (throwing inside,
/// verbose outside).  The default sink is `&std::clog`; callers retarget it at
/// runtime via the wrapper's `set_sink(...)`.  `quiet` / no marker leave the
/// engine unwrapped, byte-for-byte identical to the pre-feature behaviour.
template <pb::core::ValidPipeline Pipeline, class Engine>
[[nodiscard]] auto apply_diagnostics_policy(Engine&& engine) {
  using DiagnosticsPolicy = pipeline_diagnostics_policy_t<Pipeline>;
  if constexpr (std::is_same_v<DiagnosticsPolicy, pb::policy::diagnostics::verbose>) {
    return pb::with_verbose_diagnostics(std::forward<Engine>(engine), &std::clog);
  } else {
    // quiet / no marker -> engine unchanged.
    return std::forward<Engine>(engine);
  }
}

} // namespace detail

template <pb::core::ValidPipeline Pipeline, class SequentialPolicy>
  requires requires { typename SequentialPolicy::stage_storage_policy; }
[[nodiscard]] constexpr auto compile(SequentialPolicy) {
  using StageStoragePolicy = typename SequentialPolicy::stage_storage_policy;
  return detail::apply_diagnostics_policy<Pipeline>(
      detail::apply_error_policy(detail::sequential_engine<Pipeline, StageStoragePolicy>{}));
}

} // namespace pb::runtime

namespace pb {
using runtime::compile;
using runtime::has_error_policy_v;
using runtime::pipeline_error_policy_t;
using runtime::has_diagnostics_policy_v;
using runtime::pipeline_diagnostics_policy_t;
using runtime::has_copying_policy_v;
using runtime::pipeline_copying_policy_t;
} // namespace pb
