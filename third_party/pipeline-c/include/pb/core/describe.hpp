#pragma once

#include <array>
#include <cstddef>
#include <string_view>
#include <utility>

#include "pb/core/meta.hpp"
#include "pb/core/stage_traits.hpp"
#include "pb/core/validate.hpp"

namespace pb::core {

struct stage_record {
  std::size_t index{};
  std::string_view key{};
  std::string_view name{};
};

struct edge_record {
  std::size_t index{};
  std::size_t from_stage_index{};
  std::size_t to_stage_index{};
  std::string_view from_key{};
  std::string_view from_name{};
  std::string_view to_key{};
  std::string_view to_name{};
};

template <std::size_t StageCount>
struct pipeline_descriptor_view {
  static constexpr std::size_t stage_count = StageCount;
  static constexpr std::size_t edge_count = StageCount > 0 ? StageCount - 1 : 0;
  static constexpr bool empty = StageCount == 0;

  std::array<stage_record, StageCount> stages{};
  std::array<edge_record, edge_count> edges{};

  [[nodiscard]] constexpr auto stage_records() const noexcept
      -> const std::array<stage_record, StageCount>& {
    return stages;
  }

  [[nodiscard]] constexpr auto edge_records() const noexcept
      -> const std::array<edge_record, edge_count>& {
    return edges;
  }
};

template <std::size_t Index, class StageType>
struct stage_descriptor {
  using stage_type = StageType;
  using input_type = stage_input_t<StageType>;
  using output_type = stage_output_t<StageType>;
  using error_type = stage_error_t<StageType>;

  static constexpr std::size_t index = Index;

  [[nodiscard]] static constexpr std::string_view name() noexcept {
    return stage_traits<StageType>::name();
  }

  [[nodiscard]] static constexpr std::string_view key() noexcept {
    return stage_traits<StageType>::key();
  }
};

template <std::size_t Index, class FromStageType, class ToStageType>
struct edge_descriptor {
  using from_stage_type = FromStageType;
  using to_stage_type = ToStageType;
  using from_input_type = stage_input_t<FromStageType>;
  using from_output_type = stage_output_t<FromStageType>;
  using to_input_type = stage_input_t<ToStageType>;
  using to_output_type = stage_output_t<ToStageType>;

  static constexpr std::size_t index = Index;
  static constexpr std::size_t from_stage_index = Index;
  static constexpr std::size_t to_stage_index = Index + 1;

  [[nodiscard]] static constexpr std::string_view from_name() noexcept {
    return stage_traits<FromStageType>::name();
  }

  [[nodiscard]] static constexpr std::string_view from_key() noexcept {
    return stage_traits<FromStageType>::key();
  }

  [[nodiscard]] static constexpr std::string_view to_name() noexcept {
    return stage_traits<ToStageType>::name();
  }

  [[nodiscard]] static constexpr std::string_view to_key() noexcept {
    return stage_traits<ToStageType>::key();
  }
};

namespace detail {
template <class StageList, std::size_t Index, std::size_t StageCount, bool InRange = (Index < StageCount)>
struct pipeline_stage_type_at;

template <class StageList, std::size_t Index, std::size_t StageCount>
struct pipeline_stage_type_at<StageList, Index, StageCount, true> {
  using type = meta::at_t<StageList, Index>;
};

template <class StageList, std::size_t Index, std::size_t StageCount>
struct pipeline_stage_type_at<StageList, Index, StageCount, false> {
  static_assert(Index < StageCount,
                "Pipeline stage metadata index out of range: Index must be less than pipeline stage_count");
  using type = void;
};

template <class StageList, std::size_t Index, std::size_t EdgeCount, bool InRange = (Index < EdgeCount)>
struct pipeline_edge_descriptor_at;

template <class... Stages, std::size_t Index, std::size_t EdgeCount>
struct pipeline_edge_descriptor_at<meta::type_list<Stages...>, Index, EdgeCount, true> {
  using from_stage = meta::at_t<meta::type_list<Stages...>, Index>;
  using to_stage = meta::at_t<meta::type_list<Stages...>, Index + 1>;
  using type = edge_descriptor<Index, from_stage, to_stage>;
};

template <class StageList, std::size_t Index, std::size_t EdgeCount>
struct pipeline_edge_descriptor_at<StageList, Index, EdgeCount, false> {
  static_assert(Index < EdgeCount,
                "Pipeline edge metadata index out of range: Index must be less than pipeline edge_count");
  using type = void;
};
} // namespace detail

template <class Pipeline>
struct pipeline_traits;

template <class Input, class Output, class... Stages, class Policies>
struct pipeline_traits<pipeline<Input, Output, meta::type_list<Stages...>, Policies>> {
  using input_type = Input;
  using output_type = Output;
  using stages = meta::type_list<Stages...>;

  static constexpr std::size_t stage_count = sizeof...(Stages);
  static constexpr std::size_t edge_count = stage_count > 0 ? stage_count - 1 : 0;
  static constexpr bool empty = stage_count == 0;

  template <std::size_t Index>
  using stage_type = typename detail::pipeline_stage_type_at<stages, Index, stage_count>::type;

  template <std::size_t Index>
  using stage = stage_descriptor<Index, stage_type<Index>>;

  template <std::size_t Index>
  using edge = typename detail::pipeline_edge_descriptor_at<stages, Index, edge_count>::type;

  template <class Stage>
  static constexpr bool has_stage_v = meta::contains<stages, Stage>::value;
};

namespace detail {
template <class... Stages, std::size_t... Indexes>
[[nodiscard]] constexpr auto stage_names(meta::type_list<Stages...>, std::index_sequence<Indexes...>) noexcept
    -> std::array<std::string_view, sizeof...(Stages)> {
  return {stage_descriptor<Indexes, Stages>::name()...};
}

template <class... Stages, std::size_t... Indexes>
[[nodiscard]] constexpr auto stage_keys(meta::type_list<Stages...>, std::index_sequence<Indexes...>) noexcept
    -> std::array<std::string_view, sizeof...(Stages)> {
  return {stage_descriptor<Indexes, Stages>::key()...};
}

template <class... Stages, std::size_t... Indexes>
[[nodiscard]] constexpr auto stage_records(meta::type_list<Stages...>, std::index_sequence<Indexes...>) noexcept
    -> std::array<stage_record, sizeof...(Stages)> {
  return {stage_record{Indexes, stage_descriptor<Indexes, Stages>::key(),
                       stage_descriptor<Indexes, Stages>::name()}...};
}

template <class StageList, std::size_t Index>
struct edge_record_at;

template <class... Stages, std::size_t Index>
struct edge_record_at<meta::type_list<Stages...>, Index> {
  using from_stage = meta::at_t<meta::type_list<Stages...>, Index>;
  using to_stage = meta::at_t<meta::type_list<Stages...>, Index + 1>;

  [[nodiscard]] static constexpr auto value() noexcept -> edge_record {
    return edge_record{Index,
                       Index,
                       Index + 1,
                       stage_traits<from_stage>::key(),
                       stage_traits<from_stage>::name(),
                       stage_traits<to_stage>::key(),
                       stage_traits<to_stage>::name()};
  }
};

template <class StageList, std::size_t... Indexes>
[[nodiscard]] constexpr auto edge_records(StageList, std::index_sequence<Indexes...>) noexcept
    -> std::array<edge_record, sizeof...(Indexes)> {
  return {edge_record_at<StageList, Indexes>::value()...};
}
} // namespace detail

template <ValidPipeline Pipeline>
struct pipeline_descriptor {
  using pipeline_type = Pipeline;
  using traits = pipeline_traits<Pipeline>;
  using input_type = typename traits::input_type;
  using output_type = typename traits::output_type;
  using stages = typename traits::stages;

  static constexpr std::size_t stage_count = traits::stage_count;
  static constexpr std::size_t edge_count = traits::edge_count;
  static constexpr bool empty = traits::empty;

  using view_type = pipeline_descriptor_view<stage_count>;

  template <std::size_t Index>
  using stage = typename traits::template stage<Index>;

  template <std::size_t Index>
  using stage_type = typename traits::template stage_type<Index>;

  template <std::size_t Index>
  using edge = typename traits::template edge<Index>;

  template <std::size_t Index>
  [[nodiscard]] static constexpr std::string_view stage_name() noexcept {
    return stage<Index>::name();
  }

  template <std::size_t Index>
  [[nodiscard]] static constexpr std::string_view stage_key() noexcept {
    return stage<Index>::key();
  }

  [[nodiscard]] static constexpr auto stage_names() noexcept -> std::array<std::string_view, stage_count> {
    return detail::stage_names(stages{}, std::make_index_sequence<stage_count>{});
  }

  [[nodiscard]] static constexpr auto stage_keys() noexcept -> std::array<std::string_view, stage_count> {
    return detail::stage_keys(stages{}, std::make_index_sequence<stage_count>{});
  }

  [[nodiscard]] static constexpr auto stage_records() noexcept -> std::array<stage_record, stage_count> {
    return detail::stage_records(stages{}, std::make_index_sequence<stage_count>{});
  }

  [[nodiscard]] static constexpr auto edge_records() noexcept -> std::array<edge_record, edge_count> {
    return detail::edge_records(stages{}, std::make_index_sequence<edge_count>{});
  }

  [[nodiscard]] static constexpr auto view() noexcept -> view_type {
    return view_type{stage_records(), edge_records()};
  }
};

template <ValidPipeline Pipeline>
[[nodiscard]] constexpr auto describe() noexcept {
  return pipeline_descriptor<Pipeline>{};
}

template <ValidPipeline Pipeline>
[[nodiscard]] constexpr auto descriptor_view() noexcept {
  return pipeline_descriptor<Pipeline>::view();
}

template <ValidPipeline Pipeline>
inline constexpr std::size_t pipeline_size_v = pipeline_traits<Pipeline>::stage_count;

template <ValidPipeline Pipeline>
inline constexpr std::size_t pipeline_edge_count_v = pipeline_traits<Pipeline>::edge_count;

template <ValidPipeline Pipeline>
inline constexpr bool pipeline_empty_v = pipeline_traits<Pipeline>::empty;

template <ValidPipeline Pipeline>
using pipeline_input_t = typename pipeline_traits<Pipeline>::input_type;

template <ValidPipeline Pipeline>
using pipeline_output_t = typename pipeline_traits<Pipeline>::output_type;

template <ValidPipeline Pipeline>
using pipeline_stages_t = typename pipeline_traits<Pipeline>::stages;

template <ValidPipeline Pipeline, std::size_t Index>
using pipeline_stage_t = typename pipeline_traits<Pipeline>::template stage_type<Index>;

template <ValidPipeline Pipeline, std::size_t Index>
using pipeline_stage_descriptor_t = typename pipeline_traits<Pipeline>::template stage<Index>;

template <ValidPipeline Pipeline, std::size_t Index>
using pipeline_stage_error_t = typename pipeline_stage_descriptor_t<Pipeline, Index>::error_type;

template <ValidPipeline Pipeline, std::size_t Index>
using pipeline_edge_descriptor_t = typename pipeline_traits<Pipeline>::template edge<Index>;

template <ValidPipeline Pipeline, std::size_t Index>
using pipeline_edge_from_stage_t = typename pipeline_edge_descriptor_t<Pipeline, Index>::from_stage_type;

template <ValidPipeline Pipeline, std::size_t Index>
using pipeline_edge_to_stage_t = typename pipeline_edge_descriptor_t<Pipeline, Index>::to_stage_type;

template <ValidPipeline Pipeline, std::size_t Index>
[[nodiscard]] constexpr auto stage_key() noexcept -> std::string_view {
  return pipeline_stage_descriptor_t<Pipeline, Index>::key();
}

template <ValidPipeline Pipeline, std::size_t Index>
[[nodiscard]] constexpr auto stage_name() noexcept -> std::string_view {
  return pipeline_stage_descriptor_t<Pipeline, Index>::name();
}

template <ValidPipeline Pipeline, std::size_t Index>
[[nodiscard]] constexpr auto edge_from_key() noexcept -> std::string_view {
  return pipeline_edge_descriptor_t<Pipeline, Index>::from_key();
}

template <ValidPipeline Pipeline, std::size_t Index>
[[nodiscard]] constexpr auto edge_from_name() noexcept -> std::string_view {
  return pipeline_edge_descriptor_t<Pipeline, Index>::from_name();
}

template <ValidPipeline Pipeline, std::size_t Index>
[[nodiscard]] constexpr auto edge_to_key() noexcept -> std::string_view {
  return pipeline_edge_descriptor_t<Pipeline, Index>::to_key();
}

template <ValidPipeline Pipeline, std::size_t Index>
[[nodiscard]] constexpr auto edge_to_name() noexcept -> std::string_view {
  return pipeline_edge_descriptor_t<Pipeline, Index>::to_name();
}

template <ValidPipeline Pipeline, class Stage>
inline constexpr bool pipeline_has_stage_v = pipeline_traits<Pipeline>::template has_stage_v<Stage>;

// ─────────────────────────────────────────────────────────────────────────────
// Indexed stage I/O aliases with named out-of-range diagnostics.
//
//   pipeline_stage_input_t<P, N>   — input_type of the Nth stage in P
//   pipeline_stage_output_t<P, N>  — output_type of the Nth stage in P
//
// Both fire a static_assert that names the alias when Index >= stage_count,
// instead of a silent void or an opaque deep backtrace.
// ─────────────────────────────────────────────────────────────────────────────

namespace detail {

template <class Pipeline, std::size_t Index,
          std::size_t StageCount = pipeline_traits<Pipeline>::stage_count,
          bool InRange = (Index < StageCount)>
struct pipeline_stage_input_at {
  static_assert(Index < StageCount,
                "pipeline_stage_input_t: pipeline stage index out of range — "
                "Index must be less than pipeline_size_v<Pipeline>");
  using type = void;
};

template <class Pipeline, std::size_t Index, std::size_t StageCount>
struct pipeline_stage_input_at<Pipeline, Index, StageCount, true> {
  using type = typename stage_traits<
      typename pipeline_traits<Pipeline>::template stage_type<Index>>::input_type;
};

template <class Pipeline, std::size_t Index,
          std::size_t StageCount = pipeline_traits<Pipeline>::stage_count,
          bool InRange = (Index < StageCount)>
struct pipeline_stage_output_at {
  static_assert(Index < StageCount,
                "pipeline_stage_output_t: pipeline stage index out of range — "
                "Index must be less than pipeline_size_v<Pipeline>");
  using type = void;
};

template <class Pipeline, std::size_t Index, std::size_t StageCount>
struct pipeline_stage_output_at<Pipeline, Index, StageCount, true> {
  using type = typename stage_traits<
      typename pipeline_traits<Pipeline>::template stage_type<Index>>::output_type;
};

} // namespace detail

/// input_type of the stage at position \p Index within \p Pipeline.
/// Fires a named static_assert when \p Index >= pb::pipeline_size_v<Pipeline>.
template <ValidPipeline Pipeline, std::size_t Index>
using pipeline_stage_input_t =
    typename detail::pipeline_stage_input_at<Pipeline, Index>::type;

/// output_type of the stage at position \p Index within \p Pipeline.
/// Fires a named static_assert when \p Index >= pb::pipeline_size_v<Pipeline>.
template <ValidPipeline Pipeline, std::size_t Index>
using pipeline_stage_output_t =
    typename detail::pipeline_stage_output_at<Pipeline, Index>::type;

} // namespace pb::core
