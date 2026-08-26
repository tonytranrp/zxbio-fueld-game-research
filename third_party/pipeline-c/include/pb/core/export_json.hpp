#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include "pb/core/pipeline_state.hpp"
#include "pb/runtime/descriptor.hpp"

namespace pb::core {

namespace detail {


inline void append_json_string(std::string& output, std::string_view value) {
  constexpr auto hex_digits = std::string_view{"0123456789abcdef"};

  output.push_back('"');
  for (const auto ch : value) {
    const auto byte = static_cast<unsigned char>(ch);
    switch (ch) {
    case '"':
      output += "\\\"";
      break;
    case '\\':
      output += "\\\\";
      break;
    case '\b':
      output += "\\b";
      break;
    case '\f':
      output += "\\f";
      break;
    case '\n':
      output += "\\n";
      break;
    case '\r':
      output += "\\r";
      break;
    case '\t':
      output += "\\t";
      break;
    default:
      if (byte < 0x20U) {
        output += "\\u00";
        output.push_back(hex_digits[(byte >> 4U) & 0x0FU]);
        output.push_back(hex_digits[byte & 0x0FU]);
      } else {
        output.push_back(ch);
      }
      break;
    }
  }
  output.push_back('"');
}

[[nodiscard]] inline auto branch_case_label_or_index(
    const pb::runtime::descriptor_branch_case_record& branch_case) -> std::string {
  if (!branch_case.case_label.empty()) {
    return std::string{branch_case.case_label};
  }
  return std::to_string(branch_case.case_index);
}

inline void append_branch_case_json(std::string& output,
                                    const pb::runtime::descriptor_branch_case_record& branch_case) {
  output += "{\"index\":";
  output += std::to_string(branch_case.case_index);
  output += ",\"case_id\":";
  append_json_string(output, branch_case.case_id);
  output += ",\"case_key\":";
  append_json_string(output, branch_case.case_key);
  output += ",\"case_label\":";
  append_json_string(output, branch_case_label_or_index(branch_case));
  output += ",\"predicate_node_id\":";
  append_json_string(output, branch_case.predicate_node_id);
  output += ",\"stage_node_id\":";
  append_json_string(output, branch_case.stage_node_id);
  output += ",\"predicate_key\":";
  append_json_string(output, branch_case.predicate_key);
  output += ",\"predicate_name\":";
  append_json_string(output, branch_case.predicate_name);
  output += ",\"stage_key\":";
  append_json_string(output, branch_case.stage_key);
  output += ",\"stage_name\":";
  append_json_string(output, branch_case.stage_name);
  output += ",\"predicate_edge\":{\"from\":\"branch\",\"to\":\"predicate\",\"style\":\"dashed\",\"label\":\"test\"}";
  output += ",\"stage_edge\":{\"from\":\"predicate\",\"to\":\"case_stage\"}";
  output += "}";
}

template <class Descriptor>
void append_branch_cases_json(std::string& output, const Descriptor& descriptor, std::size_t branch_stage_index) {
  output += ",\"branch_cases\":[";
  bool first_case = true;
  for (const auto& branch_case : descriptor.branch_case_records()) {
    if (branch_case.branch_stage_index != branch_stage_index) {
      continue;
    }
    if (!first_case) {
      output.push_back(',');
    }
    first_case = false;
    append_branch_case_json(output, branch_case);
  }
  output.push_back(']');
}

template <class Descriptor>
void append_stage_kind_json(std::string& output, const Descriptor& descriptor,
                            const pb::runtime::descriptor_stage_record& stage) {
  if (stage.topology == pb::runtime::descriptor_topology::branch ||
      stage.topology == pb::runtime::descriptor_topology::fan_in) {
    output += ",\"kind\":";
    append_json_string(output, stage.topology == pb::runtime::descriptor_topology::fan_in ? "fan_in" : "branch");
    append_branch_cases_json(output, descriptor, stage.index);
  } else {
    output += ",\"kind\":\"stage\"";
  }
}

template <class Descriptor>
void append_graph_stage_json(std::string& output, const Descriptor& descriptor,
                             const pb::runtime::descriptor_stage_record& stage) {
  output += "{\"index\":";
  output += std::to_string(stage.index);
  output += ",\"key\":";
  append_json_string(output, stage.key);
  output += ",\"name\":";
  append_json_string(output, stage.name);
  append_stage_kind_json(output, descriptor, stage);
  output += "}";
}

template <class Descriptor>
void append_graph_stages_json(std::string& output, const Descriptor& descriptor) {
  bool first_stage = true;
  for (const auto& stage : descriptor.stage_records()) {
    if (!first_stage) {
      output.push_back(',');
    }
    first_stage = false;
    append_graph_stage_json(output, descriptor, stage);
  }
}

inline void append_graph_edge_json(std::string& output, const pb::runtime::descriptor_edge_record& edge) {
  output += "{\"index\":";
  output += std::to_string(edge.index);
  output += ",\"from_stage_index\":";
  output += std::to_string(edge.from_stage_index);
  output += ",\"to_stage_index\":";
  output += std::to_string(edge.to_stage_index);
  output += ",\"from_key\":";
  append_json_string(output, edge.from_key);
  output += ",\"from_name\":";
  append_json_string(output, edge.from_name);
  output += ",\"to_key\":";
  append_json_string(output, edge.to_key);
  output += ",\"to_name\":";
  append_json_string(output, edge.to_name);
  output += "}";
}

} // namespace detail

template <class Pipeline>
[[nodiscard]] auto to_json() -> std::string {
  static_assert(ValidPipeline<Pipeline>,
                "pb::to_json<Pipeline> requires pb::ValidPipeline; export helpers accept only "
                "pb::from<...>::...::to<...> pipeline types");

  if constexpr (!ValidPipeline<Pipeline>) {
    return {};
  } else {
    constexpr auto descriptor = pb::runtime::make_descriptor<Pipeline>();
    constexpr auto topology = decltype(descriptor)::topology == pb::runtime::descriptor_topology::fan_in
                                ? std::string_view{"fan_in"}
                                : (decltype(descriptor)::topology == pb::runtime::descriptor_topology::branch
                                       ? std::string_view{"branch"}
                                       : std::string_view{"linear"});

    std::string output;
    output.reserve(128U + (descriptor.stage_count * 96U) + (descriptor.edge_count * 96U));
    output += "{\"schema_version\":\"pb.core.graph.v1\",\"topology\":";
    detail::append_json_string(output, topology);
    output += ",\"stage_count\":";
    output += std::to_string(descriptor.stage_count);
    output += ",\"edge_count\":";
    output += std::to_string(descriptor.edge_count);
    output += ",\"stages\":[";
    detail::append_graph_stages_json(output, descriptor);
    output += "],\"edges\":[";

    bool first_edge = true;
    for (const auto& edge : descriptor.edge_records()) {
      if (!first_edge) {
        output.push_back(',');
      }
      first_edge = false;
      detail::append_graph_edge_json(output, edge);
    }

    output += "]}";
    return output;
  }
}



} // namespace pb::core
