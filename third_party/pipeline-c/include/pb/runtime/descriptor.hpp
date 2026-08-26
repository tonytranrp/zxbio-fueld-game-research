#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>

#include "pb/core/describe.hpp"
#include "pb/core/pipeline_state.hpp"
#include "pb/core/validate.hpp"

namespace pb::runtime {

// ---------------------------------------------------------------------------
// Schema version
// ---------------------------------------------------------------------------

inline constexpr std::string_view descriptor_schema_version = "pb.descriptor.v1";

// ---------------------------------------------------------------------------
// Topology
// ---------------------------------------------------------------------------

enum class descriptor_topology : std::uint8_t {
  linear = 0,
  branch = 1,
  fan_in = 2,
};

// ---------------------------------------------------------------------------
// Record types
// ---------------------------------------------------------------------------

struct descriptor_stage_record {
  std::size_t index{};
  std::string_view key{};
  std::string_view name{};
  std::string_view input_type_name{};
  std::string_view output_type_name{};
  descriptor_topology topology{descriptor_topology::linear};
};

struct descriptor_branch_case_record {
  std::size_t branch_stage_index{};
  std::size_t case_index{};
  std::string_view case_id{};
  std::string_view case_key{};
  std::string_view case_label{};
  std::string_view predicate_node_id{};
  std::string_view stage_node_id{};
  std::string_view predicate_key;
  std::string_view predicate_name;
  std::string_view stage_key;
  std::string_view stage_name;
  std::string_view input_type_name{};
  std::string_view output_type_name{};
};

struct descriptor_edge_record {
  std::size_t index{};
  std::size_t from_stage_index{};
  std::size_t to_stage_index{};
  std::string_view from_key{};
  std::string_view from_name{};
  std::string_view to_key{};
  std::string_view to_name{};
  std::string_view label{};
};

// ---------------------------------------------------------------------------
// Compile-time type name extraction
// ---------------------------------------------------------------------------

namespace detail {

template <class T>
[[nodiscard]] constexpr auto type_name_impl() noexcept -> std::string_view {
#if defined(__clang__)
  constexpr auto prefix = std::string_view{"[T = "};
  constexpr auto suffix = std::string_view{"]"};
  constexpr auto function = std::string_view{__PRETTY_FUNCTION__};
#elif defined(__GNUC__)
  constexpr auto prefix = std::string_view{"[with T = "};
  constexpr auto suffix = std::string_view{"]"};
  constexpr auto function = std::string_view{__PRETTY_FUNCTION__};
#elif defined(_MSC_VER)
  constexpr auto prefix = std::string_view{"type_name_impl<"};
  constexpr auto suffix = std::string_view{">(void)"};
  constexpr auto function = std::string_view{__FUNCSIG__};
#else
  constexpr auto prefix = std::string_view{};
  constexpr auto suffix = std::string_view{};
  constexpr auto function = std::string_view{"<unknown>"};
#endif
  constexpr auto start = function.find(prefix);
  constexpr auto trimmed =
      start == std::string_view::npos ? function : function.substr(start + prefix.size());
  constexpr auto end = trimmed.rfind(suffix);
  return end == std::string_view::npos ? trimmed : trimmed.substr(0, end);
}

} // namespace detail

template <class T>
[[nodiscard]] constexpr auto type_name() noexcept -> std::string_view {
  return detail::type_name_impl<T>();
}

template <std::size_t Value>
[[nodiscard]] consteval auto decimal_digits() noexcept -> std::size_t {
  std::size_t digits = 1;
  for (std::size_t temp = Value; temp >= 10; temp /= 10) {
    ++digits;
  }
  return digits;
}

template <std::size_t N>
struct consteval_buffer {
  std::array<char, N> data{};
  std::size_t pos = 0;

  constexpr void append_literal(std::string_view text) {
    for (const char ch : text) {
      data[pos++] = ch;
    }
  }

  constexpr void append_decimal(std::size_t value) {
    std::array<char, 20> digits{};
    std::size_t len = 0;
    do {
      digits[len++] = static_cast<char>('0' + (value % 10));
      value /= 10;
    } while (value != 0);
    while (len > 0) {
      data[pos++] = digits[--len];
    }
  }

  constexpr void null_terminate() { data[pos] = '\0'; }

  [[nodiscard]] constexpr std::string_view view() const { return {data.data(), pos}; }
};

template <std::size_t BranchStageIdx, std::size_t CaseIdx>
[[nodiscard]] consteval auto make_branch_case_id_storage() noexcept {
  constexpr std::size_t total = (sizeof("branch.") - 1) + decimal_digits<BranchStageIdx>() +
                                (sizeof(".case.") - 1) + decimal_digits<CaseIdx>();
  consteval_buffer<total + 1> buf{};
  buf.append_literal("branch.");
  buf.append_decimal(BranchStageIdx);
  buf.append_literal(".case.");
  buf.append_decimal(CaseIdx);
  buf.null_terminate();
  return buf.data;
}

template <std::size_t BranchStageIdx, std::size_t CaseIdx>
[[nodiscard]] consteval auto make_predicate_node_id_storage() noexcept {
  constexpr std::string_view suffix{".predicate"};
  constexpr std::size_t total = (sizeof("branch.") - 1) + decimal_digits<BranchStageIdx>() +
                                (sizeof(".case.") - 1) + decimal_digits<CaseIdx>() + suffix.size();
  consteval_buffer<total + 1> buf{};
  buf.append_literal("branch.");
  buf.append_decimal(BranchStageIdx);
  buf.append_literal(".case.");
  buf.append_decimal(CaseIdx);
  buf.append_literal(suffix);
  buf.null_terminate();
  return buf.data;
}

template <std::size_t BranchStageIdx, std::size_t CaseIdx>
[[nodiscard]] consteval auto make_stage_node_id_storage() noexcept {
  constexpr std::string_view suffix{".stage"};
  constexpr std::size_t total = (sizeof("branch.") - 1) + decimal_digits<BranchStageIdx>() +
                                (sizeof(".case.") - 1) + decimal_digits<CaseIdx>() + suffix.size();
  consteval_buffer<total + 1> buf{};
  buf.append_literal("branch.");
  buf.append_decimal(BranchStageIdx);
  buf.append_literal(".case.");
  buf.append_decimal(CaseIdx);
  buf.append_literal(suffix);
  buf.null_terminate();
  return buf.data;
}

template <std::size_t BranchStageIdx, std::size_t CaseIdx>
struct branch_case_identity_strings {
  static constexpr auto case_id_storage = make_branch_case_id_storage<BranchStageIdx, CaseIdx>();
  static constexpr std::string_view case_id{case_id_storage.data(), case_id_storage.size() - 1};
  static constexpr std::string_view case_key{case_id};

  static constexpr auto predicate_node_id_storage = make_predicate_node_id_storage<BranchStageIdx, CaseIdx>();
  static constexpr std::string_view predicate_node_id{predicate_node_id_storage.data(),
                                                      predicate_node_id_storage.size() - 1};

  static constexpr auto stage_node_id_storage = make_stage_node_id_storage<BranchStageIdx, CaseIdx>();
  static constexpr std::string_view stage_node_id{stage_node_id_storage.data(), stage_node_id_storage.size() - 1};
};

// ---------------------------------------------------------------------------
// Descriptor view — parameterised by stage count and optional branch-case count
// ---------------------------------------------------------------------------

template <std::size_t StageCount, std::size_t CaseCount = 0,
          descriptor_topology Topology = (CaseCount > 0 ? descriptor_topology::branch
                                                        : descriptor_topology::linear)>
struct descriptor_view {
  static constexpr std::string_view schema_version = descriptor_schema_version;
  static constexpr descriptor_topology topology = Topology;
  static constexpr std::size_t stage_count = StageCount;
  static constexpr std::size_t edge_count = StageCount > 0 ? StageCount - 1 : 0;
  static constexpr std::size_t case_count = CaseCount;
  static constexpr bool empty = StageCount == 0;

  std::array<descriptor_stage_record, StageCount> stages{};
  std::array<descriptor_edge_record, edge_count> edges{};
  std::array<descriptor_branch_case_record, CaseCount> branch_cases{};

  [[nodiscard]] constexpr auto stage_records() const noexcept
      -> const std::array<descriptor_stage_record, StageCount>& {
    return stages;
  }

  [[nodiscard]] constexpr auto edge_records() const noexcept
      -> const std::array<descriptor_edge_record, edge_count>& {
    return edges;
  }

  [[nodiscard]] constexpr auto branch_case_records() const noexcept
      -> const std::array<descriptor_branch_case_record, CaseCount>& {
    return branch_cases;
  }
};

// ---------------------------------------------------------------------------
// detail: conversion helpers
// ---------------------------------------------------------------------------

namespace detail {

[[nodiscard]] constexpr auto to_runtime_stage_record(pb::core::stage_record source,
                                                     std::string_view input_type_name = {},
                                                     std::string_view output_type_name = {},
                                                     descriptor_topology topo = descriptor_topology::linear) noexcept
    -> descriptor_stage_record {
  return descriptor_stage_record{.index = source.index,
                                 .key = source.key,
                                 .name = source.name,
                                 .input_type_name = input_type_name,
                                 .output_type_name = output_type_name,
                                 .topology = topo};
}

[[nodiscard]] constexpr auto to_runtime_edge_record(pb::core::edge_record source,
                                                    std::string_view lbl = {}) noexcept -> descriptor_edge_record {
  return descriptor_edge_record{.index = source.index,
                                .from_stage_index = source.from_stage_index,
                                .to_stage_index = source.to_stage_index,
                                .from_key = source.from_key,
                                .from_name = source.from_name,
                                .to_key = source.to_key,
                                .to_name = source.to_name,
                                .label = lbl};
}

// ---------------------------------------------------------------------------
// detail: stage-level type-name extraction from pipeline stages
// ---------------------------------------------------------------------------

template <class Stage>
[[nodiscard]] constexpr auto stage_type_names() noexcept {
  if constexpr (pb::core::detail::is_selected_branch_node<Stage>::value ||
                pb::core::detail::is_fan_in_branch_node<Stage>::value) {
    return std::pair{type_name<typename Stage::input_type>(),
                     type_name<typename Stage::output_type>()};
  } else {
    return std::pair{type_name<pb::core::stage_input_t<Stage>>(),
                     type_name<pb::core::stage_output_t<Stage>>()};
  }
}

template <class Stage>
[[nodiscard]] constexpr auto stage_topology() noexcept -> descriptor_topology {
  if constexpr (pb::core::detail::is_fan_in_branch_node<Stage>::value) {
    return descriptor_topology::fan_in;
  } else if constexpr (pb::core::detail::is_selected_branch_node<Stage>::value) {
    return descriptor_topology::branch;
  } else {
    return descriptor_topology::linear;
  }
}

// ---------------------------------------------------------------------------
// detail: total branch-case count across all pipeline stages
//          Uses a helper trait so MSVC does not eagerly evaluate
//          Stage::case_count on non-branch-node stages.
// ---------------------------------------------------------------------------

template <class Stage, bool IsBranchLike = (pb::core::detail::is_selected_branch_node<Stage>::value ||
                                           pb::core::detail::is_fan_in_branch_node<Stage>::value)>
struct branch_case_count_or_zero : std::integral_constant<std::size_t, 0> {};

template <class Stage>
struct branch_case_count_or_zero<Stage, true> : std::integral_constant<std::size_t, Stage::case_count> {};

template <class StageList>
struct total_branch_case_count;

template <class... Stages>
struct total_branch_case_count<pb::meta::type_list<Stages...>>
    : std::integral_constant<std::size_t, (0 + ... + branch_case_count_or_zero<Stages>::value)> {};

template <class StageList>
inline constexpr std::size_t total_branch_case_count_v = total_branch_case_count<StageList>::value;

// ---------------------------------------------------------------------------
// detail: make descriptor_view with type-name enrichment
// ---------------------------------------------------------------------------

template <class Pipeline, class... Stages, std::size_t... StageIndexes, std::size_t... EdgeIndexes>
[[nodiscard]] constexpr auto make_enriched_descriptor(
    pb::core::pipeline_descriptor_view<sizeof...(Stages)> source,
    pb::meta::type_list<Stages...>,
    std::index_sequence<StageIndexes...>,
    std::index_sequence<EdgeIndexes...>) noexcept
    -> descriptor_view<sizeof...(Stages)> {
  return descriptor_view<sizeof...(Stages)>{
      .stages = {to_runtime_stage_record(source.stage_records()[StageIndexes],
                                         stage_type_names<Stages>().first,
                                         stage_type_names<Stages>().second,
                                         stage_topology<Stages>())...},
      .edges = {to_runtime_edge_record(source.edge_records()[EdgeIndexes])...},
  };
}

template <std::size_t StageCount, std::size_t... StageIndexes, std::size_t... EdgeIndexes>
[[nodiscard]] constexpr auto make_runtime_descriptor(pb::core::pipeline_descriptor_view<StageCount> source,
                                                     std::index_sequence<StageIndexes...>,
                                                     std::index_sequence<EdgeIndexes...>) noexcept
    -> descriptor_view<StageCount> {
  return descriptor_view<StageCount>{
      .stages = {to_runtime_stage_record(source.stage_records()[StageIndexes])...},
      .edges = {to_runtime_edge_record(source.edge_records()[EdgeIndexes])...},
  };
}

// ---------------------------------------------------------------------------
// detail: branch case record generation
// ---------------------------------------------------------------------------

template <std::size_t BranchStageIdx, std::size_t CaseIdx, class Case>
[[nodiscard]] constexpr auto to_branch_case_record() noexcept
    -> descriptor_branch_case_record {
  using Predicate = typename Case::predicate_type;
  using BranchStage = typename Case::stage_type;
  using identity = branch_case_identity_strings<BranchStageIdx, CaseIdx>;

  return descriptor_branch_case_record{
      .branch_stage_index = BranchStageIdx,
      .case_index = CaseIdx,
      .case_id = identity::case_id,
      .case_key = identity::case_key,
      .case_label = Case::case_label(),
      .predicate_node_id = identity::predicate_node_id,
      .stage_node_id = identity::stage_node_id,
      .predicate_key = pb::core::stage_traits<Predicate>::key(),
      .predicate_name = pb::core::stage_traits<Predicate>::name(),
      .stage_key = pb::core::stage_traits<BranchStage>::key(),
      .stage_name = pb::core::stage_traits<BranchStage>::name(),
      .input_type_name = type_name<typename Case::input_type>(),
      .output_type_name = type_name<pb::core::stage_output_t<BranchStage>>(),
  };
}

template <class BranchNode, std::size_t BranchStageIdx, std::size_t... CaseIndexes>
[[nodiscard]] constexpr auto branch_case_records_from_node(std::index_sequence<CaseIndexes...>) noexcept
    -> std::array<descriptor_branch_case_record, sizeof...(CaseIndexes)> {
  using Cases = typename BranchNode::cases;
  return {to_branch_case_record<BranchStageIdx, CaseIndexes, pb::meta::at_t<Cases, CaseIndexes>>()...};
}

// ---------------------------------------------------------------------------
// detail: fill stage records (non-lambda helper for MSVC compatibility)
// ---------------------------------------------------------------------------

template <class SourceView, class... Stages, std::size_t... Idx>
[[nodiscard]] constexpr auto fill_stage_records(const SourceView& source,
                                                pb::meta::type_list<Stages...>,
                                                std::index_sequence<Idx...>) noexcept
    -> std::array<descriptor_stage_record, sizeof...(Stages)> {
  return {to_runtime_stage_record(source.stage_records()[Idx],
                                  stage_type_names<Stages>().first,
                                  stage_type_names<Stages>().second,
                                  stage_topology<Stages>())...};
}

// ---------------------------------------------------------------------------
// detail: fill edge records (non-lambda helper for MSVC compatibility)
// ---------------------------------------------------------------------------

template <class SourceView, std::size_t EdgeCount, std::size_t... Idx>
[[nodiscard]] constexpr auto fill_edge_records(const SourceView& source,
                                               std::index_sequence<Idx...>) noexcept
    -> std::array<descriptor_edge_record, EdgeCount> {
  return {to_runtime_edge_record(source.edge_records()[Idx])...};
}

// ---------------------------------------------------------------------------
// detail: fill branch case records by iterating stages and collecting from
//          branch nodes (non-lambda helper for MSVC compatibility)
// ---------------------------------------------------------------------------

template <std::size_t TotalCases, class... Stages, std::size_t... StageIndexes>
[[nodiscard]] constexpr auto fill_branch_case_records(pb::meta::type_list<Stages...>,
                                                      std::index_sequence<StageIndexes...>) noexcept
    -> std::array<descriptor_branch_case_record, TotalCases> {
  auto cases_arr = std::array<descriptor_branch_case_record, TotalCases>{};

  if constexpr (TotalCases > 0) {
    std::size_t case_offset = 0;
    auto collect_one = [&]<class Stage, std::size_t StageIndex>() {
      if constexpr (pb::core::detail::is_selected_branch_node<Stage>::value ||
                    pb::core::detail::is_fan_in_branch_node<Stage>::value) {
        constexpr auto node_cases = branch_case_count_or_zero<Stage>::value;
        auto node_records =
            branch_case_records_from_node<Stage, StageIndex>(std::make_index_sequence<node_cases>{});
        for (std::size_t i = 0; i < node_cases; ++i) {
          cases_arr[case_offset + i] = node_records[i];
        }
        case_offset += node_cases;
      }
    };
    (collect_one.template operator()<Stages, StageIndexes>(), ...);
  }

  return cases_arr;
}

// ---------------------------------------------------------------------------
// detail: build branch descriptor — detect branch nodes, extract cases,
//          produce descriptor_view<StageCount, TotalCaseCount>
// ---------------------------------------------------------------------------

template <class Pipeline, class... Stages>
[[nodiscard]] constexpr auto build_branch_descriptor(
    pb::core::pipeline_descriptor_view<sizeof...(Stages)> source,
    pb::meta::type_list<Stages...> stages_tl) noexcept {
  constexpr auto stage_count = sizeof...(Stages);
  constexpr auto edge_count = stage_count > 0 ? stage_count - 1 : 0;
  constexpr auto total_cases = total_branch_case_count_v<pb::meta::type_list<Stages...>>;

  auto stages_arr = fill_stage_records(source, stages_tl, std::make_index_sequence<stage_count>{});
  auto edges_arr = fill_edge_records<decltype(source), edge_count>(
      source, std::make_index_sequence<edge_count>{});
  auto cases_arr = fill_branch_case_records<total_cases>(stages_tl, std::make_index_sequence<stage_count>{});

  constexpr auto topology = (pb::core::detail::is_fan_in_branch_node<Stages>::value || ...)
                                ? descriptor_topology::fan_in
                                : ((pb::core::detail::is_selected_branch_node<Stages>::value || ...)
                                       ? descriptor_topology::branch
                                       : descriptor_topology::linear);

  return descriptor_view<stage_count, total_cases, topology>{
      .stages = stages_arr,
      .edges = edges_arr,
      .branch_cases = cases_arr,
  };
}

} // namespace detail

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

template <pb::core::ValidPipeline Pipeline>
[[nodiscard]] constexpr auto make_descriptor() noexcept {
  constexpr auto core_view = pb::core::descriptor_view<Pipeline>();
  using stages_t = pb::core::pipeline_stages_t<Pipeline>;
  return detail::build_branch_descriptor<Pipeline>(core_view, stages_t{});
}

template <pb::core::ValidPipeline Pipeline>
[[nodiscard]] constexpr auto make_branch_descriptor() noexcept {
  return make_descriptor<Pipeline>();
}

} // namespace pb::runtime
