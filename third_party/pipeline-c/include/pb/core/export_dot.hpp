#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

#include "pb/core/detail/string_sink.hpp"
#include "pb/core/pipeline_state.hpp"
#include "pb/runtime/descriptor.hpp"

namespace pb::core {

namespace detail {


[[nodiscard]] inline auto sanitize_identifier(std::string_view value) -> std::string {
  std::string output(value.begin(), value.end());
  for (auto& ch : output) {
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_') {
      continue;
    }
    ch = '_';
  }
  if (output.empty()) {
    return "pipeline";
  }
  if ((output.front() >= '0' && output.front() <= '9')) {
    output.front() = '_';
  }
  return output;
}

[[nodiscard]] inline auto escape_dot_label(std::string_view value) -> std::string {
  std::string output;
  output.reserve(value.size());
  for (const auto ch : value) {
    switch (ch) {
    case '"':
      output += "\\\"";
      break;
    case '\\':
      output += "\\\\";
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
      output.push_back(ch);
      break;
    }
  }
  return output;
}

inline void append_dot_label(string_sink& stream, std::string_view value) {
  stream << '"' << escape_dot_label(value) << '"';
}

// ── Compile-time branch detection ───────────────────────────────────────────

template <class StageList>
struct has_branch_stage_impl;

template <class... Stages>
struct has_branch_stage_impl<meta::type_list<Stages...>>
    : std::bool_constant<((is_selected_branch_node<Stages>::value || is_fan_in_branch_node<Stages>::value) || ...)> {};

template <class Pipeline>
inline constexpr bool pipeline_has_branch_v =
    has_branch_stage_impl<typename pipeline_traits<Pipeline>::stages>::value;

// ── Branch case DOT rendering ───────────────────────────────────────────────

inline void append_branch_case_subgraph(string_sink& stream,
                                        const pb::runtime::descriptor_branch_case_record& branch_case) {
  const auto branch_index = branch_case.branch_stage_index;
  const auto case_index = branch_case.case_index;
  const auto pred_name = branch_case.predicate_name;
  const auto stage_name = branch_case.stage_name;
  const auto case_name = branch_case.case_label.empty() ? std::to_string(case_index)
                                                       : std::string{branch_case.case_label};
  const auto case_label = std::string{"Case "} + case_name + ": " + std::string{pred_name};
  const auto pred_label = std::string{"pred: "} + std::string{pred_name};

  stream << "  subgraph cluster_case_" << branch_index << "_" << case_index << " {\n";
  stream << "    label=";
  append_dot_label(stream, case_label);
  stream << ";\n";
  stream << "    pred_" << branch_index << "_" << case_index << " [label=";
  append_dot_label(stream, pred_label);
  stream << "];\n";
  stream << "    case_" << branch_index << "_" << case_index << " [label=";
  append_dot_label(stream, stage_name);
  stream << "];\n";
  stream << "    branch_" << branch_index << " -> pred_" << branch_index << "_" << case_index
         << " [style=dashed, label=";
  append_dot_label(stream, "test");
  stream << "];\n";
  stream << "    pred_" << branch_index << "_" << case_index << " -> case_" << branch_index << "_"
         << case_index << ";\n";
  stream << "  }\n";
}

template <class Descriptor>
void append_branch_stage_dot(string_sink& stream, const Descriptor& descriptor, std::size_t stage_index) {
  const auto& branch_stage = descriptor.stage_records()[stage_index];
  stream << "  branch_" << stage_index << " [shape=diamond, label=";
  append_dot_label(stream, branch_stage.topology == pb::runtime::descriptor_topology::fan_in ? "fan_in" : "branch");
  stream << "];\n\n";

  for (const auto& branch_case : descriptor.branch_case_records()) {
    if (branch_case.branch_stage_index == stage_index) {
      append_branch_case_subgraph(stream, branch_case);
    }
  }
  stream << "\n";
}

// ── Branch-aware DOT emitter ────────────────────────────────────────────────

template <class Pipeline, bool HasBranch>
struct dot_emitter;

// Linear pipeline emitter
template <class Pipeline>
struct dot_emitter<Pipeline, false> {
  using descriptor_type = decltype(pb::runtime::make_descriptor<Pipeline>());

  [[nodiscard]] static auto emit(std::string_view graph_name) -> std::string {
    constexpr auto descriptor = pb::runtime::make_descriptor<Pipeline>();

    string_sink stream;
    stream << "digraph " << detail::sanitize_identifier(graph_name) << " {\n";
    stream << "  rankdir=LR;\n";
    stream << "  node [shape=box];\n\n";

    for (const auto& stage : descriptor.stage_records()) {
      const auto key = stage.key;
      const auto name = stage.name;
      auto label = std::string{name};
      if (name != key) {
        label += "\n(";
        label += key;
        label += ")";
      }
      stream << "  stage_" << stage.index << " [label=";
      append_dot_label(stream, label);
      stream << "];\n";
    }

    stream << "\n";

    for (const auto& edge : descriptor.edge_records()) {
      stream << "  stage_" << edge.from_stage_index << " -> stage_" << edge.to_stage_index;
      const auto label = std::string{edge.from_key} + " -> " + std::string{edge.to_key};
      stream << " [label=";
      append_dot_label(stream, label);
      stream << "];\n";
    }

    stream << "}\n";
    return std::move(stream).str();
  }
};

// Branch pipeline emitter
template <class Pipeline>
struct dot_emitter<Pipeline, true> {
  using descriptor_type = decltype(pb::runtime::make_descriptor<Pipeline>());
  static constexpr std::size_t stage_count = descriptor_type::stage_count;

  [[nodiscard]] static auto emit(std::string_view graph_name) -> std::string {
    constexpr auto descriptor = pb::runtime::make_descriptor<Pipeline>();
    string_sink stream;
    stream << "digraph " << detail::sanitize_identifier(graph_name) << " {\n";
    stream << "  rankdir=LR;\n";
    stream << "  node [shape=box];\n\n";

    // ── Input node ──────────────────────────────────────────────
    stream << "  from_input [label=";
    append_dot_label(stream, "Input");
    stream << "];\n\n";

    // ── Stage nodes ─────────────────────────────────────────────
    emit_stages(stream, descriptor);

    // ── Output node ─────────────────────────────────────────────
    stream << "  to_output [shape=doublecircle, label=";
    append_dot_label(stream, "Output");
    stream << "];\n\n";

    // ── Edges ───────────────────────────────────────────────────
    emit_all_edges(stream, descriptor);

    stream << "}\n";
    return std::move(stream).str();
  }

private:
  // ── Stage node emission ──────────────────────────────────────

  static auto is_branch_stage(const pb::runtime::descriptor_stage_record& stage) noexcept -> bool {
    return stage.topology == pb::runtime::descriptor_topology::branch ||
           stage.topology == pb::runtime::descriptor_topology::fan_in;
  }

  static void emit_stages(string_sink& stream, const descriptor_type& descriptor) {
    for (const auto& stage : descriptor.stage_records()) {
      emit_single_stage(stream, descriptor, stage);
    }
  }

  static void emit_single_stage(string_sink& stream, const descriptor_type& descriptor,
                                const pb::runtime::descriptor_stage_record& stage) {
    if (is_branch_stage(stage)) {
      detail::append_branch_stage_dot(stream, descriptor, stage.index);
    } else {
      const auto key = stage.key;
      const auto name = stage.name;
      auto label = std::string{name};
      if (name != key) {
        label += "\n(";
        label += key;
        label += ")";
      }
      stream << "  stage_" << stage.index << " [label=";
      append_dot_label(stream, label);
      stream << "];\n";
    }
  }

  // ── Edge emission ────────────────────────────────────────────

  static void emit_all_edges(string_sink& stream, const descriptor_type& descriptor) {
    // Input -> first stage
    emit_input_to_first_edge(stream, descriptor);

    // Stage-to-stage edges (for each adjacent pair)
    emit_adjacent_edges(stream, descriptor);

    // Last stage -> output
    emit_last_to_output_edge(stream, descriptor);
  }

  static void emit_input_to_first_edge(string_sink& stream, const descriptor_type& descriptor) {
    if constexpr (stage_count > 0) {
      if (is_branch_stage(descriptor.stage_records()[0])) {
        stream << "  from_input -> branch_0;\n";
      } else {
        stream << "  from_input -> stage_0;\n";
      }
    } else {
      stream << "  from_input -> to_output;\n";
    }
  }

  static void emit_adjacent_edges(string_sink& stream, const descriptor_type& descriptor) {
    for (std::size_t index = 0; index + 1 < stage_count; ++index) {
      emit_adjacent_edge(stream, descriptor, index);
    }
  }

  static void emit_adjacent_edge(string_sink& stream, const descriptor_type& descriptor, std::size_t index) {
    if (index + 1 < stage_count) {
      const auto& current_stage = descriptor.stage_records()[index];
      const auto& next_stage = descriptor.stage_records()[index + 1];

      if (is_branch_stage(current_stage)) {
        // Branch -> next: connect all case output stages to next stage
        emit_branch_to_next_edges(stream, descriptor, index, is_branch_stage(next_stage));
      } else if (is_branch_stage(next_stage)) {
        // Previous -> branch diamond
        stream << "  stage_" << index << " -> branch_" << (index + 1) << ";\n";
      } else {
        // Regular -> regular
        stream << "  stage_" << index << " -> stage_" << (index + 1) << ";\n";
      }
    }
  }

  static void emit_branch_to_next_edges(string_sink& stream, const descriptor_type& descriptor,
                                        std::size_t branch_index, bool next_is_branch) {
    for (const auto& branch_case : descriptor.branch_case_records()) {
      if (branch_case.branch_stage_index != branch_index) {
        continue;
      }
      stream << "  case_" << branch_index << "_" << branch_case.case_index;
      if (next_is_branch) {
        stream << " -> branch_" << (branch_index + 1) << ";\n";
      } else {
        stream << " -> stage_" << (branch_index + 1) << ";\n";
      }
    }
  }

  static void emit_last_to_output_edge(string_sink& stream, const descriptor_type& descriptor) {
    if constexpr (stage_count > 0) {
      if (is_branch_stage(descriptor.stage_records()[stage_count - 1])) {
        emit_branch_to_output_edges(stream, descriptor);
      } else {
        stream << "  stage_" << (stage_count - 1) << " -> to_output;\n";
      }
    }
  }

  static void emit_branch_to_output_edges(string_sink& stream, const descriptor_type& descriptor) {
    constexpr auto branch_index = stage_count - 1;
    for (const auto& branch_case : descriptor.branch_case_records()) {
      if (branch_case.branch_stage_index == branch_index) {
        stream << "  case_" << branch_index << "_" << branch_case.case_index << " -> to_output;\n";
      }
    }
  }
};

} // namespace detail

template <class Pipeline>
[[nodiscard]] auto to_dot(std::string_view graph_name = "pipeline") -> std::string {
  static_assert(ValidPipeline<Pipeline>,
                "pb::to_dot<Pipeline> requires pb::ValidPipeline; export helpers accept only "
                "pb::from<...>::...::to<...> pipeline types");

  if constexpr (!ValidPipeline<Pipeline>) {
    return {};
  } else {
    return detail::dot_emitter<Pipeline, detail::pipeline_has_branch_v<Pipeline>>::emit(graph_name);
  }
}


} // namespace pb::core
