#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

#include "pb/core/detail/string_sink.hpp"
#include "pb/core/pipeline_state.hpp"
#include "pb/core/validate.hpp"
#include "pb/runtime/descriptor.hpp"

// pb::to_text<Pipeline>()
//
// Compact, human-readable single-document export of a validated pipeline.
// Designed for compiler-error logs, CI snippets, and inline pb_cli output —
// no quoting overhead, no escaping, just a flat indented dump of the same
// descriptor data that drives DOT and JSON.
//
// Output schema (stable enough to grep, NOT a public interchange contract —
// see docs/graph-export-roadmap.md for the export-helper boundary):
//
//   # pb.core.graph.v1  topology=<linear|branch|fan_in>  stages=<N>  edges=<M>
//   [<i>] <stage_key>  "<stage_name>"
//       case <j>  predicate=<predicate_key>  stage=<stage_key>     (branch/fan_in only)
//   edges:
//     <from_index> -> <to_index>  <from_key> -> <to_key>
//
// All fields use the same string_views the rest of the descriptor pipeline
// uses, so empty values render as empty fields rather than "(null)".

namespace pb::core {

namespace detail {


[[nodiscard]] inline auto topology_label(pb::runtime::descriptor_topology topology) noexcept
    -> std::string_view {
  switch (topology) {
  case pb::runtime::descriptor_topology::fan_in:
    return "fan_in";
  case pb::runtime::descriptor_topology::branch:
    return "branch";
  case pb::runtime::descriptor_topology::linear:
  default:
    return "linear";
  }
}

inline void append_stage_text(string_sink& stream,
                              const pb::runtime::descriptor_stage_record& stage) {
  stream << "[" << stage.index << "] " << stage.key;
  if (!stage.name.empty() && stage.name != stage.key) {
    stream << "  \"" << stage.name << "\"";
  }
  stream << "\n";
}

inline void append_branch_cases_text(string_sink& stream, std::size_t stage_index,
                                     const auto& branch_case_records) {
  for (const auto& record : branch_case_records) {
    if (record.branch_stage_index != stage_index) continue;
    stream << "    case " << record.case_index << "  predicate=" << record.predicate_key
           << "  stage=" << record.stage_key;
    if (!record.case_label.empty()) {
      stream << "  label=" << record.case_label;
    }
    stream << "\n";
  }
}

inline void append_edge_text(string_sink& stream,
                             const pb::runtime::descriptor_edge_record& edge) {
  stream << "  " << edge.from_stage_index << " -> " << edge.to_stage_index << "  "
         << edge.from_key << " -> " << edge.to_key << "\n";
}

} // namespace detail

template <class Pipeline>
[[nodiscard]] auto to_text() -> std::string {
  static_assert(ValidPipeline<Pipeline>,
                "pb::to_text<Pipeline> requires pb::ValidPipeline; export helpers accept only "
                "pb::from<...>::...::to<...> pipeline types");

  if constexpr (!ValidPipeline<Pipeline>) {
    return {};
  } else {
    constexpr auto descriptor = pb::runtime::make_descriptor<Pipeline>();
    constexpr auto topology = decltype(descriptor)::topology;

    detail::string_sink stream;
    stream << "# pb.core.graph.v1  topology=" << detail::topology_label(topology)
           << "  stages=" << descriptor.stage_count << "  edges=" << descriptor.edge_count << "\n";

    const auto& stage_records = descriptor.stage_records();
    const auto& branch_case_records = descriptor.branch_case_records();

    for (const auto& stage : stage_records) {
      detail::append_stage_text(stream, stage);
      if (stage.topology == pb::runtime::descriptor_topology::branch ||
          stage.topology == pb::runtime::descriptor_topology::fan_in) {
        detail::append_branch_cases_text(stream, stage.index, branch_case_records);
      }
    }

    if (descriptor.edge_count > 0) {
      stream << "edges:\n";
      for (const auto& edge : descriptor.edge_records()) {
        detail::append_edge_text(stream, edge);
      }
    }

    return std::move(stream).str();
  }
}



} // namespace pb::core
