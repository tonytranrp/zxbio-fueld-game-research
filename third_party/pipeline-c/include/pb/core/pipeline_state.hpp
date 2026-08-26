#pragma once

#include <concepts>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

#include "pb/core/concepts.hpp"
#include "pb/core/fixed_string.hpp"
#include "pb/core/meta.hpp"
namespace pb::policy::errors {
struct throwing;
struct terminating;
struct ignoring;
struct propagating;
struct result;
} // namespace pb::policy::errors

namespace pb::policy::diagnostics {
struct verbose;
struct quiet;
} // namespace pb::policy::diagnostics

namespace pb::policy::copying {
struct value;
struct move_only;
struct shared;
struct clone;
} // namespace pb::policy::copying

namespace pb::core {

template <class Predicate, class BranchStage, fixed_string Label = fixed_string{""}>
struct branch_case;

template <class JoinStage>
struct join_node;

template <std::size_t Index, class T>
struct fan_in_case_result;

template <class... CaseResults>
struct fan_in_results;

template <class... Cases>
struct branch_outputs;

template <class Case>
struct branch_case_output;

template <class Outputs, class Output>
struct branch_output_validation;

template <class Outputs, class Output>
struct branch_unified_output_validation;

template <class Outputs, class JoinStage>
struct join_builder_validation;

template <class... Cases>
struct branch_raw_output_types;

template <class... Cases>
struct branch_unified_output;

template <class... Cases>
struct fan_in_output;

template <class... Cases>
using fan_in_output_t = typename fan_in_output<Cases...>::type;

template <class Policies, class Input, class Current, class... Stages>
struct pipeline_state;

namespace detail {

template <class>
inline constexpr bool always_false_v = false;

template <class Predicate, bool IsPredicateStage = Stage<Predicate>>
struct branch_predicate_output_bool : std::false_type {};

template <class Predicate>
struct branch_predicate_output_bool<Predicate, true>
    : std::bool_constant<std::convertible_to<stage_output_t<Predicate>, bool>> {};

template <class>
struct is_branch_case : std::false_type {};

template <class Predicate, class BranchStage, fixed_string Label>
struct is_branch_case<branch_case<Predicate, BranchStage, Label>> : std::true_type {};

template <class>
struct is_join_node : std::false_type {};

template <class JoinStage>
struct is_join_node<join_node<JoinStage>> : std::true_type {};

template <class>
struct is_branch_outputs : std::false_type {};

template <class... Cases>
struct is_branch_outputs<branch_outputs<Cases...>> : std::true_type {};

template <class Output, class... Cases>
struct branch_outputs_match_output
    : std::bool_constant<(std::same_as<typename branch_case_output<Cases>::output_type, Output> && ...)> {};

template <class T>
struct is_type_list : std::false_type {};

template <class... Ts>
struct is_type_list<meta::type_list<Ts...>> : std::true_type {};

template <class Stage, class OutputTypes>
struct stage_invocable_for_output_types : std::false_type {};

template <class Stage, class... Outputs>
struct stage_invocable_for_output_types<Stage, meta::type_list<Outputs...>>
    : std::bool_constant<(std::is_invocable_v<Stage, Outputs> && ...)> {};

template <class Outputs, class JoinStage, bool IsOutputs = is_branch_outputs<Outputs>::value,
          bool IsJoinStage = Stage<JoinStage>>
struct join_stage_accepts_outputs : std::false_type {};

template <class Outputs, class JoinStage>
struct join_stage_accepts_outputs<Outputs, JoinStage, true, true> {
  using join_input_type = stage_input_t<JoinStage>;
  using raw_output_types = typename Outputs::output_types;
  using unified_output_type = typename Outputs::output_type;

  static constexpr bool accepts_unified_output = std::same_as<join_input_type, unified_output_type>;
  static constexpr bool accepts_raw_type_list =
      std::same_as<join_input_type, raw_output_types> &&
      stage_invocable_for_output_types<JoinStage, raw_output_types>::value;

  static constexpr bool value = accepts_unified_output || accepts_raw_type_list;
};

template <class Case, bool IsBranchCase = is_branch_case<Case>::value>
struct branch_case_output_impl {
  static_assert(always_false_v<Case>,
                "Branch output marker requires pb::case_<Predicate>::then<Stage>");
};

template <class Case>
struct branch_case_output_impl<Case, true> {
  using case_type = Case;
  using output_type = stage_output_t<typename Case::stage_type>;
};

template <class Join, bool IsJoinNode = is_join_node<Join>::value>
struct join_output_impl {
  static_assert(always_false_v<Join>, "Join output marker requires pb::join_node<Stage>");
};

template <class Join>
struct join_output_impl<Join, true> {
  using join_type = Join;
  using stage_type = typename Join::stage_type;
  using input_type = typename Join::input_type;
  using output_type = typename Join::output_type;
};

template <class Outputs, class Join, bool IsOutputs = is_branch_outputs<Outputs>::value,
          bool IsJoin = is_join_node<Join>::value>
struct join_validation_impl {
  static_assert(always_false_v<Outputs>,
                "Join validation requires pb::branch_outputs<...> and pb::join_node<Stage>");
};

template <class Outputs, class Join>
struct join_validation_impl<Outputs, Join, true, true> {
  using stage_type = typename Join::stage_type;
  using acceptance = join_stage_accepts_outputs<Outputs, stage_type>;

  static_assert(acceptance::value,
                "Join validation mismatch: join stage input_type must match the unified branch output_type "
                "or the raw branch output_types type_list; type-list joins must be invocable for every "
                "branch output_type");

  using branch_outputs_type = Outputs;
  using join_type = Join;
  using raw_output_types = typename Outputs::output_types;
  using input_type = typename Join::input_type;
  using execution_input_type = typename Outputs::output_type;
  using output_type = typename Join::output_type;
  static constexpr bool accepts_unified_output = acceptance::accepts_unified_output;
  static constexpr bool accepts_raw_type_list = acceptance::accepts_raw_type_list;
};

template <class Outputs, class JoinStage, bool IsOutputs = is_branch_outputs<Outputs>::value,
          bool IsJoinStage = Stage<JoinStage>>
struct join_builder_validation_impl {
  static_assert(always_false_v<Outputs>,
                "Join builder validation requires pb::branch_outputs<...> and a valid join stage");
};

template <class Outputs, class JoinStage>
struct join_builder_validation_impl<Outputs, JoinStage, true, true> {
  using join_type = join_node<JoinStage>;
  using acceptance = join_stage_accepts_outputs<Outputs, JoinStage>;

  static_assert(acceptance::value,
                "Join builder source mismatch: join stage input_type must match unified branch output_type "
                "or raw branch output_types before pb::from<...>::branch<...>::join<Stage>; type-list joins "
                "must be invocable for every branch output_type");

  using branch_outputs_type = Outputs;
  using stage_type = JoinStage;
  using raw_output_types = typename Outputs::output_types;
  using input_type = stage_input_t<JoinStage>;
  using execution_input_type = typename Outputs::output_type;
  using output_type = stage_output_t<JoinStage>;
  static constexpr bool accepts_unified_output = acceptance::accepts_unified_output;
  static constexpr bool accepts_raw_type_list = acceptance::accepts_raw_type_list;
};

template <class Outputs, class Output, bool IsOutputs = is_branch_outputs<Outputs>::value>
struct branch_output_validation_impl {
  static_assert(always_false_v<Outputs>,
                "Branch output validation requires pb::branch_outputs<...>");
};

template <class Output, class... Cases>
struct branch_output_validation_impl<branch_outputs<Cases...>, Output, true> {
  static_assert(branch_outputs_match_output<Output, Cases...>::value,
                "Branch output validation mismatch: every branch output_type must match requested output_type");

  using branch_outputs_type = branch_outputs<Cases...>;
  using input_type = typename branch_outputs<Cases...>::input_type;
  using output_type = Output;
  using output_types = typename branch_outputs<Cases...>::output_types;
  static constexpr std::size_t output_count = sizeof...(Cases);
};

template <class Outputs, class Output, bool IsOutputs = is_branch_outputs<Outputs>::value>
struct branch_unified_output_validation_impl {
  static_assert(always_false_v<Outputs>,
                "Branch unified output validation requires pb::branch_outputs<...>");
};

template <class Output, class... Cases>
struct branch_unified_output_validation_impl<branch_outputs<Cases...>, Output, true> {
  static_assert(std::same_as<typename branch_outputs<Cases...>::output_type, Output>,
                "Branch unified output validation mismatch: Output must match branch_outputs::output_type");

  using branch_outputs_type = branch_outputs<Cases...>;
  using input_type = typename branch_outputs<Cases...>::input_type;
  using output_type = Output;
  using raw_output_types = typename branch_outputs<Cases...>::output_types;
  using unified_output_type = typename branch_outputs<Cases...>::output_type;
  static constexpr std::size_t output_count = sizeof...(Cases);
};

template <class... Cases>
struct branch_cases_valid : std::bool_constant<(is_branch_case<Cases>::value && ...)> {};

template <bool AllCasesValid, class... Cases>
struct branch_cases_same_input_impl : std::false_type {};

template <class... Cases>
struct branch_cases_same_input_impl<true, Cases...> : std::true_type {};

template <class First, class... Rest>
struct branch_cases_same_input_impl<true, First, Rest...>
    : std::bool_constant<(std::same_as<typename First::input_type, typename Rest::input_type> && ...)> {};

template <class... Cases>
struct branch_cases_same_input
    : branch_cases_same_input_impl<branch_cases_valid<Cases...>::value, Cases...> {};

template <bool IsBranchCase, class Case>
struct branch_case_input_or_void {
  using type = void;
};

template <class Case>
struct branch_case_input_or_void<true, Case> {
  using type = typename Case::input_type;
};

template <class... Cases>
struct branch_node_input {
  using type = void;
};

template <class First, class... Rest>
struct branch_node_input<First, Rest...>
    : branch_case_input_or_void<is_branch_case<First>::value, First> {};

template <class T, class... Rest>
struct all_same : std::bool_constant<(std::same_as<T, Rest> && ...)> {};

template <class... Outputs>
struct branch_output_type_or_variant;

template <class Output>
struct branch_output_type_or_variant<Output> {
  using type = Output;
};

template <class Output1, class Output2, class... Rest>
struct branch_output_type_or_variant<Output1, Output2, Rest...> {
private:
  static constexpr bool homogeneous = all_same<Output1, Output2, Rest...>::value;

  template <bool IsHomogeneous, class... Ts>
  struct impl;

  template <class T, class... Ts>
  struct impl<true, T, Ts...> {
    using type = T;
  };

  template <class... Ts>
  struct impl<false, Ts...> {
    using type = std::variant<Ts...>;
  };

public:
  using type = typename impl<homogeneous, Output1, Output2, Rest...>::type;
};

template <class OutputTypes>
struct branch_unified_output_from_type_list;

template <class... Outputs>
struct branch_unified_output_from_type_list<meta::type_list<Outputs...>> {
  using type = typename branch_output_type_or_variant<Outputs...>::type;
};

template <class... Cases>
struct selected_branch_node;

template <class>
struct is_selected_branch_node : std::false_type {};

template <class... Cases>
struct is_selected_branch_node<selected_branch_node<Cases...>> : std::true_type {};

template <class... Cases>
struct fan_in_branch_node;

template <class>
struct is_fan_in_branch_node : std::false_type {};

template <class... Cases>
struct is_fan_in_branch_node<fan_in_branch_node<Cases...>> : std::true_type {};

template <class Stage, bool IsSelectedBranch = is_selected_branch_node<Stage>::value>
struct branch_outputs_for_stage {
  using type = void;
};

template <class Stage>
struct branch_outputs_for_stage<Stage, true> {
  using type = typename Stage::branch_outputs_type;
};

template <class... Stages>
struct last_stage_or_void {
  using type = void;
};

template <class First, class... Rest>
struct last_stage_or_void<First, Rest...> {
  using type = meta::back_t<meta::type_list<First, Rest...>>;
};

template <class Policies, class Input, class Current, class List>
struct pipeline_state_from_list;

template <class Policies, class Input, class Current, class... Stages>
struct pipeline_state_from_list<Policies, Input, Current, meta::type_list<Stages...>> {
  using type = pipeline_state<Policies, Input, Current, Stages...>;
};

template <class IndexSequence, class... Cases>
struct fan_in_results_from_cases;

template <std::size_t... Indexes, class... Cases>
struct fan_in_results_from_cases<std::index_sequence<Indexes...>, Cases...> {
  using type = fan_in_results<fan_in_case_result<Indexes, typename branch_case_output<Cases>::output_type>...>;
};

template <class LastStage, bool IsSelectedBranch, class JoinStage>
struct fan_in_stage_from_last {
  static_assert(always_false_v<LastStage>,
                "Fan-in join requires a preceding branch: use pb::from<...>::branch<...>::fan_in<Stage>");
};

template <class... Cases, class JoinStage>
struct fan_in_stage_from_last<selected_branch_node<Cases...>, true, JoinStage> {
  using type = fan_in_branch_node<Cases...>;
};
} // namespace detail

enum class fan_in_case_state {
  skipped,
  completed,
  failed,
};

template <std::size_t Index, class T>
struct fan_in_case_result {
  static constexpr std::size_t index = Index;
  using value_type = T;

  fan_in_case_state state{fan_in_case_state::skipped};
  std::optional<T> value{};
  std::optional<std::string> diagnostic{};

  [[nodiscard]] constexpr bool selected() const noexcept { return state == fan_in_case_state::completed; }
  [[nodiscard]] constexpr bool completed() const noexcept { return state == fan_in_case_state::completed; }
  [[nodiscard]] constexpr bool skipped() const noexcept { return state == fan_in_case_state::skipped; }
  [[nodiscard]] constexpr bool failed() const noexcept { return state == fan_in_case_state::failed; }
  [[nodiscard]] constexpr bool has_value() const noexcept { return value.has_value(); }
  [[nodiscard]] constexpr T& get() & { return *value; }
  [[nodiscard]] constexpr const T& get() const& { return *value; }
  [[nodiscard]] constexpr T&& get() && { return std::move(*value); }
  [[nodiscard]] constexpr std::string_view diagnostic_message() const noexcept {
    return diagnostic.has_value() ? std::string_view{*diagnostic} : std::string_view{};
  }

  template <class Value>
  void mark_completed(Value&& next_value) {
    state = fan_in_case_state::completed;
    value.emplace(std::forward<Value>(next_value));
    diagnostic.reset();
  }

  void mark_failed(std::string message) {
    state = fan_in_case_state::failed;
    value.reset();
    diagnostic.emplace(std::move(message));
  }
};

template <std::size_t Index>
struct fan_in_case_result<Index, void> {
  static constexpr std::size_t index = Index;
  using value_type = void;

  fan_in_case_state state{fan_in_case_state::skipped};
  std::optional<std::string> diagnostic{};

  [[nodiscard]] constexpr bool selected() const noexcept { return state == fan_in_case_state::completed; }
  [[nodiscard]] constexpr bool completed() const noexcept { return state == fan_in_case_state::completed; }
  [[nodiscard]] constexpr bool skipped() const noexcept { return state == fan_in_case_state::skipped; }
  [[nodiscard]] constexpr bool failed() const noexcept { return state == fan_in_case_state::failed; }
  [[nodiscard]] constexpr bool has_value() const noexcept { return state == fan_in_case_state::completed; }
  [[nodiscard]] constexpr std::string_view diagnostic_message() const noexcept {
    return diagnostic.has_value() ? std::string_view{*diagnostic} : std::string_view{};
  }

  void mark_completed() noexcept {
    state = fan_in_case_state::completed;
    diagnostic.reset();
  }

  void mark_failed(std::string message) {
    state = fan_in_case_state::failed;
    diagnostic.emplace(std::move(message));
  }
};

template <class... CaseResults>
struct fan_in_results {
  using cases_type = std::tuple<CaseResults...>;
  static constexpr std::size_t case_count = sizeof...(CaseResults);

  cases_type cases{};

  template <std::size_t Index>
  [[nodiscard]] constexpr auto& get() & {
    return std::get<Index>(cases);
  }

  template <std::size_t Index>
  [[nodiscard]] constexpr const auto& get() const& {
    return std::get<Index>(cases);
  }

  template <std::size_t Index>
  [[nodiscard]] constexpr auto&& get() && {
    return std::get<Index>(std::move(cases));
  }
};

template <class... Cases>
struct fan_in_output {
  using type = typename detail::fan_in_results_from_cases<std::index_sequence_for<Cases...>, Cases...>::type;
};

template <class Predicate, class BranchStage, fixed_string Label>
struct branch_case {
  static_assert(Stage<Predicate>,
                "Branch predicate is invalid: define input_type and bool-like output_type");
  static_assert(detail::branch_predicate_output_bool<Predicate>::value,
                "Branch predicate stage must produce a bool-like output_type");
  static_assert(Stage<BranchStage>, "Branch case target stage is invalid: define input_type and output_type");
  static_assert(std::same_as<stage_input_t<Predicate>, stage_input_t<BranchStage>>,
                "Branch case source mismatch: predicate input_type must match branch target stage input_type");

  using predicate_type = Predicate;
  using stage_type = BranchStage;
  using input_type = stage_input_t<Predicate>;
  static constexpr auto label = Label;

  [[nodiscard]] static constexpr std::string_view case_label() noexcept { return Label.view(); }
};

template <class Case>
struct branch_case_output : detail::branch_case_output_impl<Case> {};

template <class Predicate>
struct case_ {
  template <class Stage>
  using then = branch_case<Predicate, Stage>;

  template <fixed_string Label>
  struct label {
    template <class Stage>
    using then = branch_case<Predicate, Stage, Label>;
  };
};

template <class... Cases>
struct branch_node {
  static_assert(sizeof...(Cases) > 0, "Branch node requires at least one pb::case_<Predicate>::then<Stage>");
  static_assert(detail::branch_cases_valid<Cases...>::value,
                "Branch node requires pb::case_<Predicate>::then<Stage> cases");
  static_assert(detail::branch_cases_same_input<Cases...>::value,
                "Branch node source mismatch: all branch cases must share input_type");

  using cases = meta::type_list<Cases...>;
  using input_type = typename detail::branch_node_input<Cases...>::type;
  using output_types = meta::type_list<typename branch_case_output<Cases>::output_type...>;
  static constexpr std::size_t case_count = sizeof...(Cases);
};

template <class... Cases>
struct branch_outputs {
  static_assert(sizeof...(Cases) > 0, "Branch outputs require at least one pb::case_<Predicate>::then<Stage>");
  static_assert(detail::branch_cases_valid<Cases...>::value,
                "Branch outputs require pb::case_<Predicate>::then<Stage> cases");
  static_assert(detail::branch_cases_same_input<Cases...>::value,
                "Branch outputs source mismatch: all branch cases must share input_type");

  using cases = meta::type_list<Cases...>;
  using input_type = typename detail::branch_node_input<Cases...>::type;
  using output_types = meta::type_list<typename branch_case_output<Cases>::output_type...>;
  using output_type = typename detail::branch_unified_output_from_type_list<output_types>::type;
  static constexpr std::size_t output_count = sizeof...(Cases);
};

template <class... Cases>
struct branch_raw_output_types {
  using type = typename branch_outputs<Cases...>::output_types;
};

template <class... Cases>
using branch_raw_output_types_t = typename branch_raw_output_types<Cases...>::type;

template <class... Cases>
struct branch_unified_output {
  using type = typename branch_outputs<Cases...>::output_type;
};

template <class... Cases>
using branch_unified_output_t = typename branch_unified_output<Cases...>::type;

namespace detail {
template <class... Cases>
struct selected_branch_node {
  // Extract all branch output types
  using cases = meta::type_list<Cases...>;
  using input_type = typename branch_node_input<Cases...>::type;

  // Collect all output types via branch_outputs (validates cases are valid branch_case types)
  using branch_outputs_type = branch_outputs<Cases...>;
  using output_types = typename branch_outputs_type::output_types;

  // Variant-or-single type:
  // - If all branch outputs are the same type, use it directly (homogeneous, backward compatible).
  // - Otherwise, wrap in std::variant (heterogeneous).
  using output_type = typename branch_outputs_type::output_type;

  static constexpr std::size_t case_count = sizeof...(Cases);
  static constexpr bool homogeneous = all_same<
      typename branch_case_output<Cases>::output_type...>::value;

  // For move-only inputs, predicates must accept const& to inspect the input
  // before it gets moved into the selected branch stage.
  // For copy-constructible inputs, no additional predicate constraint is required.
  static_assert(std::copy_constructible<input_type> ||
                    (std::is_invocable_v<typename Cases::predicate_type, const input_type&> && ...),
                "Move-only branch inputs require predicates callable with const input_type& — "
                "predicates inspect the input by const reference while the selected branch stage "
                "receives it by move; define operator()(const input_type&) const on each predicate stage");

  // Storage for stateful branch execution: preserves predicate and branch stage
  // instances across multiple pipeline runs so they can carry mutable state.
  using predicate_tuple = std::tuple<typename Cases::predicate_type...>;
  using branch_stage_tuple = std::tuple<typename Cases::stage_type...>;

  predicate_tuple predicates_{};
  branch_stage_tuple branch_stages_{};

  [[nodiscard]] static constexpr auto stage_name() noexcept { return "branch"; }
  [[nodiscard]] static constexpr auto stage_key() noexcept { return "branch"; }
};

template <class... Cases>
struct fan_in_branch_node {
  using cases = meta::type_list<Cases...>;
  using input_type = typename branch_node_input<Cases...>::type;
  using branch_outputs_type = branch_outputs<Cases...>;
  using output_types = typename branch_outputs_type::output_types;

  using output_type = fan_in_output_t<Cases...>;

  static constexpr std::size_t case_count = sizeof...(Cases);

  static_assert(std::copy_constructible<input_type> ||
                    (std::is_invocable_v<typename Cases::stage_type&, const input_type&> && ...),
                "Fan-in branch inputs must be copy-constructible, or every fan-in branch stage must be callable "
                "with const input_type& for borrowed-input fan-in execution");
  static_assert((std::is_invocable_v<typename Cases::predicate_type, const input_type&> && ...),
                "Fan-in branch predicates must be callable with const input_type& so every predicate can inspect "
                "the input before any passing branch stage receives a copy");

  using predicate_tuple = std::tuple<typename Cases::predicate_type...>;
  using branch_stage_tuple = std::tuple<typename Cases::stage_type...>;

  predicate_tuple predicates_{};
  branch_stage_tuple branch_stages_{};

  [[nodiscard]] static constexpr auto stage_name() noexcept { return "fan_in"; }
  [[nodiscard]] static constexpr auto stage_key() noexcept { return "fan_in"; }
};
} // namespace detail

template <class Outputs, class Output>
struct branch_output_validation : detail::branch_output_validation_impl<Outputs, Output> {};

template <class Outputs, class Output>
struct branch_unified_output_validation : detail::branch_unified_output_validation_impl<Outputs, Output> {};

template <class JoinStage>
struct join_node {
  static_assert(Stage<JoinStage>, "Join stage is invalid: define input_type and output_type");

  using stage_type = JoinStage;
  using input_type = stage_input_t<JoinStage>;
  using output_type = stage_output_t<JoinStage>;
};

template <class Join>
struct join_output : detail::join_output_impl<Join> {};

template <class Outputs, class Join>
struct join_validation : detail::join_validation_impl<Outputs, Join> {};

template <class Outputs, class JoinStage>
struct join_builder_validation : detail::join_builder_validation_impl<Outputs, JoinStage> {};

template <class Input, class Output, class StageList, class Policies = pb::meta::type_list<>>
struct pipeline {
  using input_type = Input;
  using output_type = Output;
  using stages = StageList;
  using policies = Policies;
  static constexpr bool valid = true;
};

template <class Policies, class Input, class Current, class... Stages>
struct pipeline_state;

namespace detail {

template <class State, class Stage>
struct append_stage;

template <class Policies, class Input, class Current, class... Stages, class StageType>
struct append_stage<pipeline_state<Policies, Input, Current, Stages...>, StageType> {
  static_assert(has_stage_input_type_v<StageType>,
                "Pipeline stage is invalid: missing input_type member");
  static_assert(has_stage_output_type_v<StageType>,
                "Pipeline stage is invalid: missing output_type member");
  static_assert(Stage<StageType>, "Pipeline stage is invalid: define input_type and output_type");
  static_assert(Connectable<Current, StageType>,
                "Pipeline edge mismatch: previous output_type (or pipeline input) must exactly match next "
                "stage input_type; inspect pb::connectable_v<PreviousOutput, NextStage> or "
                "pb::AdjacentStages<PreviousStage, NextStage>");
  using type = pipeline_state<Policies, Input, stage_output_t<StageType>, Stages..., StageType>;
};

template <class State, class... StageTypes>
struct append_stages;

template <class State>
struct append_stages<State> {
  using type = State;
};

template <class State, class StageType, class... Rest>
struct append_stages<State, StageType, Rest...> {
  using next = typename append_stage<State, StageType>::type;
  using type = typename append_stages<next, Rest...>::type;
};

template <class State, class JoinStage>
struct append_join;

template <class Policies, class Input, class Current, class... Stages, class JoinStage>
struct append_join<pipeline_state<Policies, Input, Current, Stages...>, JoinStage> {
  using last_stage = typename detail::last_stage_or_void<Stages...>::type;

  static_assert(!std::is_same_v<last_stage, void>,
                "Join requires a preceding branch: use pb::from<...>::branch<...>::join<Stage>");

  static_assert(detail::is_selected_branch_node<last_stage>::value,
                "Join must follow a branch node: pipeline_state::join is only valid after ::branch<...>");

  static_assert(Stage<JoinStage>, "Join stage is invalid: define input_type and output_type");

  using branch_outputs_type = typename branch_outputs_for_stage<last_stage>::type;
  using acceptance = join_stage_accepts_outputs<branch_outputs_type, JoinStage>;

  static_assert(acceptance::value,
                "Pipeline edge mismatch: join stage input_type must match the unified branch output_type "
                "or raw branch output_types type_list; type-list joins must be invocable for every "
                "branch output_type");

  using type = pipeline_state<Policies, Input, stage_output_t<JoinStage>, Stages..., JoinStage>;
};

template <class State, class JoinStage>
struct append_fan_in;

template <class Policies, class Input, class Current, class... Stages, class JoinStage>
struct append_fan_in<pipeline_state<Policies, Input, Current, Stages...>, JoinStage> {
  using last_stage = typename detail::last_stage_or_void<Stages...>::type;

  static_assert(!std::is_same_v<last_stage, void>,
                "Fan-in join requires a preceding branch: use pb::from<...>::branch<...>::fan_in<Stage>");

  static_assert(detail::is_selected_branch_node<last_stage>::value,
                "Fan-in join must follow a branch node: pipeline_state::fan_in is only valid after ::branch<...>");

  static_assert(Stage<JoinStage>, "Fan-in join stage is invalid: define input_type and output_type");

  using fan_in_stage =
      typename detail::fan_in_stage_from_last<last_stage, detail::is_selected_branch_node<last_stage>::value,
                                              JoinStage>::type;

  static_assert(std::same_as<stage_input_t<JoinStage>, typename fan_in_stage::output_type>,
                "Fan-in join input mismatch: join stage input_type must match pb::fan_in_results<...> for the "
                "preceding branch cases");

  using replaced_stages = meta::replace_back_t<meta::type_list<Stages...>, fan_in_stage>;
  using with_join = meta::push_back_t<replaced_stages, JoinStage>;
  using type = typename detail::pipeline_state_from_list<Policies, Input, stage_output_t<JoinStage>, with_join>::type;
};

template <class State, class Output>
struct finalize_pipeline;

template <class Policies, class Input, class Current, class... Stages, class Output>
struct finalize_pipeline<pipeline_state<Policies, Input, Current, Stages...>, Output> {
  static_assert(std::same_as<Current, Output>,
                "Pipeline sink mismatch: actual final output type does not match requested sink type");
  using type = pipeline<Input, Output, meta::type_list<Stages...>, Policies>;
};

template <class State, class... Cases>
struct append_branch;

template <class T>
struct is_error_policy_marker : std::false_type {};

template <>
struct is_error_policy_marker<::pb::policy::errors::throwing> : std::true_type {};
template <>
struct is_error_policy_marker<::pb::policy::errors::terminating> : std::true_type {};
template <>
struct is_error_policy_marker<::pb::policy::errors::ignoring> : std::true_type {};
template <>
struct is_error_policy_marker<::pb::policy::errors::propagating> : std::true_type {};
template <>
struct is_error_policy_marker<::pb::policy::errors::result> : std::true_type {};

template <class T>
struct is_diagnostics_policy_marker : std::false_type {};

template <>
struct is_diagnostics_policy_marker<::pb::policy::diagnostics::verbose> : std::true_type {};
template <>
struct is_diagnostics_policy_marker<::pb::policy::diagnostics::quiet> : std::true_type {};

template <class T>
struct is_copying_policy_marker : std::false_type {};

template <>
struct is_copying_policy_marker<::pb::policy::copying::value> : std::true_type {};
template <>
struct is_copying_policy_marker<::pb::policy::copying::move_only> : std::true_type {};
template <>
struct is_copying_policy_marker<::pb::policy::copying::shared> : std::true_type {};
template <>
struct is_copying_policy_marker<::pb::policy::copying::clone> : std::true_type {};

template <class ExistingPolicies, class... NewPolicies>
struct append_policies;

template <template <class> class Predicate, class... Policies>
inline constexpr auto policy_axis_count_v =
    (std::size_t{0} + ... + (Predicate<Policies>::value ? std::size_t{1} : std::size_t{0}));

template <class... ExistingPolicies, class... NewPolicies>
struct append_policies<meta::type_list<ExistingPolicies...>, NewPolicies...> {
  static constexpr auto error_policy_count =
      policy_axis_count_v<is_error_policy_marker, ExistingPolicies..., NewPolicies...>;
  static constexpr auto diagnostics_policy_count =
      policy_axis_count_v<is_diagnostics_policy_marker, ExistingPolicies..., NewPolicies...>;
  static constexpr auto copying_policy_count =
      policy_axis_count_v<is_copying_policy_marker, ExistingPolicies..., NewPolicies...>;

  static_assert(error_policy_count <= 1,
                "pb::pipeline_state::with accepts at most one pb::policy::errors::* marker; "
                "duplicate or conflicting error policies are rejected");
  static_assert(diagnostics_policy_count <= 1,
                "pb::pipeline_state::with accepts at most one pb::policy::diagnostics::* marker; "
                "duplicate or conflicting diagnostics policies are rejected");
  static_assert(copying_policy_count <= 1,
                "pb::pipeline_state::with accepts at most one pb::policy::copying::* marker; "
                "duplicate or conflicting copying policies are rejected");

  using type = meta::type_list<ExistingPolicies..., NewPolicies...>;
};

template <class Policies, class Input, class Current, class... Stages>
struct append_branch<pipeline_state<Policies, Input, Current, Stages...>> {
  static_assert(always_false_v<pipeline_state<Policies, Input, Current, Stages...>>,
                "Branch node requires at least one pb::case_<Predicate>::then<Stage>");
};

template <class Policies, class Input, class Current, class... Stages, class... Cases>
struct append_branch<pipeline_state<Policies, Input, Current, Stages...>, Cases...> {
  using branch_input = typename branch_node_input<Cases...>::type;

  static_assert(std::same_as<Current, branch_input>,
                "Branch builder source mismatch: current pipeline output_type must match branch case input_type "
                "before pb::from<...>::branch<...>");

  using branch_stage = selected_branch_node<Cases...>;
  using type = pipeline_state<Policies, Input, typename branch_stage::output_type, Stages..., branch_stage>;
};

} // namespace detail

template <class Policies, class Input, class Current, class... Stages>
struct pipeline_state {
  using input_type = Input;
  using current_type = Current;
  using stages = meta::type_list<Stages...>;
  using stage_list = stages;

  static constexpr std::size_t stage_count = sizeof...(Stages);
  static constexpr bool empty = stage_count == 0;

  /// Accumulated policy markers carried by ::with<...>.  Threaded into the
  /// finalized pipeline's 4th template parameter at ::to<Output>.
  using policies = Policies;

  template <class StageType>
  using then = typename detail::append_stage<pipeline_state, StageType>::type;

  template <class... StageTypes>
  using then_all = typename detail::append_stages<pipeline_state, StageTypes...>::type;

  template <class... StageTypes>
  using pipe = then_all<StageTypes...>;

  template <class... StageTypes>
  using through = then_all<StageTypes...>;

  template <class StageType>
  using join = typename detail::append_join<pipeline_state, StageType>::type;

  template <class StageType>
  using fan_in = typename detail::append_fan_in<pipeline_state, StageType>::type;

  template <class StageType>
  using join_all = fan_in<StageType>;

  template <class Output>
  using to = typename detail::finalize_pipeline<pipeline_state, Output>::type;

  template <class Output>
  using as = to<Output>;

  template <class Output>
  using returns = to<Output>;

  using done = typename detail::finalize_pipeline<pipeline_state, Current>::type;
  using pipeline_type = done;

  template <class... Cases>
  using branch = typename detail::append_branch<pipeline_state, Cases...>::type;

  /// Policy marker accumulator — appends the supplied markers to the carried
  /// `Policies` type-list and returns the updated pipeline_state.  The markers
  /// are pure type-level annotations (zero runtime cost) until inspected by a
  /// backend such as pb::compile<P>(pb::runtime::sequential{}), which selects
  /// an error-policy engine wrapper from any pb::policy::errors marker present.
  /// The copying axis is intentionally single-valued: once one
  /// pb::policy::copying::* marker is carried, adding a duplicate or conflicting
  /// copying marker is rejected at compile time instead of silently letting the
  /// first marker win.
  template <class... NewPolicies>
  using with = pipeline_state<typename detail::append_policies<Policies, NewPolicies...>::type,
                              Input, Current, Stages...>;
};

template <class Input>
using from = pipeline_state<meta::type_list<>, Input, Input>;

} // namespace pb::core
