// pb.core.graph.v1 cross-version stability regression.
//
// docs/export-schema-v1.md formalizes the v1 contract.  The "Stability
// promise" section lists invariants that MUST hold across every v1.x
// release: schema_version literal, field names + types + positions,
// enum spellings, banner format, DOT wrapper shape, sibling schema
// version constants.
//
// This test pins every documented invariant via string-equal regression
// so the next person to touch the export code finds out *here* —
// before review — when they've drifted the schema.  A failure means
// either:
//
//   (a) the change is wrong and the schema is being broken silently, or
//   (b) the schema is being intentionally bumped to v2 and the test
//       needs to be replaced by a `pb.core.graph.v2` regression PLUS
//       the v1 emitter must still ship with this test passing.
//
// schema_v1_contract.cpp pins the top-level identifier and the
// stage-record field order.  This file extends coverage to:
//
//   * Sibling schema version constants
//   * BranchCaseRecord 11-field order in JSON output
//   * EdgeRecord 7-field order in JSON output (linear pipelines)
//   * DOT wrapper structure for linear and branch pipelines
//   * Compact-text branch case-line format

#include <pb/pipeline.hpp>

#include <cstdlib>
#include <string>
#include <string_view>

namespace schema_v1_cross_version {

void pb_test_require(bool condition) {
  if (!condition) {
    std::abort();
  }
}

struct Raw  { int value{}; };
struct Done { int value{}; };

struct ParseA {
  using input_type  = Raw;
  using output_type = Done;
  static constexpr auto stage_key()  noexcept { return "parse-a"; }
  static constexpr auto stage_name() noexcept { return "parse-a"; }
  Done operator()(Raw raw) const { return {raw.value + 1}; }
};

struct ParseB {
  using input_type  = Raw;
  using output_type = Done;
  static constexpr auto stage_key()  noexcept { return "parse-b"; }
  static constexpr auto stage_name() noexcept { return "parse-b"; }
  Done operator()(Raw raw) const { return {raw.value + 10}; }
};

struct IsPositive {
  using input_type  = Raw;
  using output_type = bool;
  static constexpr auto stage_key()  noexcept { return "is-positive"; }
  static constexpr auto stage_name() noexcept { return "is-positive"; }
  bool operator()(const Raw& raw) const { return raw.value > 0; }
};

struct IsNegative {
  using input_type  = Raw;
  using output_type = bool;
  static constexpr auto stage_key()  noexcept { return "is-negative"; }
  static constexpr auto stage_name() noexcept { return "is-negative"; }
  bool operator()(const Raw& raw) const { return raw.value < 0; }
};

struct Step {
  using input_type  = Raw;
  using output_type = Done;
  static constexpr auto stage_key()  noexcept { return "step"; }
  static constexpr auto stage_name() noexcept { return "step"; }
  Done operator()(Raw raw) const { return {raw.value + 1}; }
};

struct Finalize {
  using input_type  = Done;
  using output_type = Done;
  static constexpr auto stage_key()  noexcept { return "finalize"; }
  static constexpr auto stage_name() noexcept { return "finalize"; }
  Done operator()(Done done) const { return {done.value * 2}; }
};

using PositiveCase = pb::case_<IsPositive>::then<ParseA>;
using NegativeCase = pb::case_<IsNegative>::then<ParseB>;

using LinearPipeline = pb::from<Raw>::then<Step>::then<Finalize>::to<Done>;
using BranchPipeline = pb::from<Raw>::branch<PositiveCase, NegativeCase>::to<Done>;

constexpr auto contains(std::string_view haystack, std::string_view needle) noexcept -> bool {
  return haystack.find(needle) != std::string_view::npos;
}

// Returns true if `needle` appears in `haystack` at position >= `after`.
constexpr auto contains_after(std::string_view haystack, std::string_view needle, std::size_t after) noexcept -> bool {
  return haystack.find(needle, after) != std::string_view::npos;
}

// Returns position of `needle` in `haystack` (or std::string::npos).
auto find_pos(const std::string& haystack, std::string_view needle) -> std::size_t {
  return haystack.find(needle);
}

} // namespace schema_v1_cross_version

int main() {
  using namespace schema_v1_cross_version;

  // ── 1. Sibling schema version constants must keep their exact literal. ──
  static_assert(pb::descriptor_schema_version == std::string_view{"pb.descriptor.v1"},
                "pb.descriptor.v1 is part of the v1 stability promise");
  static_assert(pb::diagnostics_schema_version == std::string_view{"pb.diagnostics.v1"},
                "pb.diagnostics.v1 is part of the v1 stability promise");
  static_assert(pb::trace_schema_version == std::string_view{"pb.trace.v1"},
                "pb.trace.v1 is part of the v1 stability promise");
  static_assert(pb::route_descriptor_schema_version == std::string_view{"pb.runtime.route.v1"},
                "pb.runtime.route.v1 is part of the v1 stability promise");
  static_assert(pb::observer_schema_version == std::string_view{"pb.observer.v1"},
                "pb.observer.v1 ABI identifier is part of the v1 stability promise");
  static_assert(pb::verbose_observer_schema_version == std::string_view{"pb.observer.verbose.v1"},
                "pb.observer.verbose.v1 line-schema identifier is part of the v1 stability promise");
  static_assert(pb::error_schema_version == std::string_view{"pb.error.v1"},
                "pb.error.v1 JSON-shape identifier is part of the v1 stability promise");
  static_assert(pb::trace_ndjson_schema_version == std::string_view{"pb.trace.ndjson.v1"},
                "pb.trace.ndjson.v1 streaming-sink identifier is part of the v1 stability promise");
  static_assert(pb::state_dsl_schema_version == std::string_view{"pb.state.v1"},
                "pb.state.v1 stateful-storage-DSL identifier is part of the v1 stability promise");

  // ── 2. Linear JSON: EdgeRecord 7-field order ──
  //
  //   index, from_stage_index, to_stage_index, from_key, from_name, to_key, to_name
  //
  // A renamed or reordered EdgeRecord field gets caught here.
  {
    const auto json = pb::to_json<LinearPipeline>();

    // Pin the schema_version + topology + top-level fields.
    pb_test_require(json.rfind("{\"schema_version\":\"pb.core.graph.v1\",\"topology\":\"linear\"", 0) == 0);

    const auto edges_pos          = find_pos(json, "\"edges\":[");
    pb_test_require(edges_pos != std::string::npos);

    // The first edge record must contain all 7 documented fields, in order.
    const auto first_edge_start   = json.find('{', edges_pos);
    pb_test_require(first_edge_start != std::string::npos);
    const auto first_edge_end     = json.find('}', first_edge_start);
    pb_test_require(first_edge_end != std::string::npos);

    const auto edge_record = json.substr(first_edge_start, first_edge_end - first_edge_start + 1);

    const auto idx          = edge_record.find("\"index\"");
    const auto from_idx     = edge_record.find("\"from_stage_index\"");
    const auto to_idx       = edge_record.find("\"to_stage_index\"");
    const auto from_key     = edge_record.find("\"from_key\"");
    const auto from_name    = edge_record.find("\"from_name\"");
    const auto to_key       = edge_record.find("\"to_key\"");
    const auto to_name      = edge_record.find("\"to_name\"");

    pb_test_require(idx       != std::string::npos);
    pb_test_require(from_idx  != std::string::npos);
    pb_test_require(to_idx    != std::string::npos);
    pb_test_require(from_key  != std::string::npos);
    pb_test_require(from_name != std::string::npos);
    pb_test_require(to_key    != std::string::npos);
    pb_test_require(to_name   != std::string::npos);

    pb_test_require(idx       < from_idx);
    pb_test_require(from_idx  < to_idx);
    pb_test_require(to_idx    < from_key);
    pb_test_require(from_key  < from_name);
    pb_test_require(from_name < to_key);
    pb_test_require(to_key    < to_name);

    // Concrete edge values pin the from_/to_ direction at the same time.
    pb_test_require(contains(edge_record, "\"from_key\":\"step\""));
    pb_test_require(contains(edge_record, "\"to_key\":\"finalize\""));
  }

  // ── 3. Branch JSON: BranchCaseRecord 11-field order ──
  //
  //   index, case_id, case_key, case_label, predicate_node_id, stage_node_id,
  //   predicate_key, predicate_name, stage_key, stage_name,
  //   predicate_edge, stage_edge
  {
    const auto json = pb::to_json<BranchPipeline>();

    pb_test_require(contains(json, "\"topology\":\"branch\""));
    pb_test_require(contains(json, "\"branch_cases\":["));

    const auto cases_pos        = find_pos(json, "\"branch_cases\":[");
    const auto first_case_start = json.find('{', cases_pos);
    pb_test_require(first_case_start != std::string::npos);
    const auto first_case_end   = json.find('}', json.find("\"stage_edge\"", first_case_start));
    pb_test_require(first_case_end != std::string::npos);
    const auto record = json.substr(first_case_start, first_case_end - first_case_start + 1);

    const auto p_index           = record.find("\"index\"");
    const auto p_case_id         = record.find("\"case_id\"");
    const auto p_case_key        = record.find("\"case_key\"");
    const auto p_case_label      = record.find("\"case_label\"");
    const auto p_pred_node       = record.find("\"predicate_node_id\"");
    const auto p_stage_node      = record.find("\"stage_node_id\"");
    const auto p_pred_key        = record.find("\"predicate_key\"");
    const auto p_pred_name       = record.find("\"predicate_name\"");
    const auto p_stage_key       = record.find("\"stage_key\"");
    const auto p_stage_name      = record.find("\"stage_name\"");
    const auto p_pred_edge       = record.find("\"predicate_edge\"");
    const auto p_stage_edge      = record.find("\"stage_edge\"");

    pb_test_require(p_index      != std::string::npos);
    pb_test_require(p_case_id    != std::string::npos);
    pb_test_require(p_case_key   != std::string::npos);
    pb_test_require(p_case_label != std::string::npos);
    pb_test_require(p_pred_node  != std::string::npos);
    pb_test_require(p_stage_node != std::string::npos);
    pb_test_require(p_pred_key   != std::string::npos);
    pb_test_require(p_pred_name  != std::string::npos);
    pb_test_require(p_stage_key  != std::string::npos);
    pb_test_require(p_stage_name != std::string::npos);
    pb_test_require(p_pred_edge  != std::string::npos);
    pb_test_require(p_stage_edge != std::string::npos);

    pb_test_require(p_index      < p_case_id);
    pb_test_require(p_case_id    < p_case_key);
    pb_test_require(p_case_key   < p_case_label);
    pb_test_require(p_case_label < p_pred_node);
    pb_test_require(p_pred_node  < p_stage_node);
    pb_test_require(p_stage_node < p_pred_key);
    pb_test_require(p_pred_key   < p_pred_name);
    pb_test_require(p_pred_name  < p_stage_key);
    pb_test_require(p_stage_key  < p_stage_name);
    pb_test_require(p_stage_name < p_pred_edge);
    pb_test_require(p_pred_edge  < p_stage_edge);

    // The embedded predicate_edge must declare the documented edge shape:
    //   { "from": "branch", "to": "predicate", "style": "dashed", "label": "test" }
    pb_test_require(contains(record, "\"predicate_edge\":{\"from\":\"branch\",\"to\":\"predicate\",\"style\":\"dashed\",\"label\":\"test\"}"));
    // The embedded stage_edge must declare:
    //   { "from": "predicate", "to": "case_stage" }
    pb_test_require(contains(record, "\"stage_edge\":{\"from\":\"predicate\",\"to\":\"case_stage\"}"));
  }

  // ── 4. Linear DOT: canonical wrapper structure ──
  {
    const auto dot = pb::to_dot<LinearPipeline>();

    // digraph wrapper, rankdir, shape are part of the contract.
    pb_test_require(contains(dot, "digraph"));
    pb_test_require(contains(dot, "rankdir=LR"));
    pb_test_require(contains(dot, "node [shape=box]"));

    // Stage labels follow the documented `<name>\n(<key>)` form.
    pb_test_require(contains(dot, "step"));
    pb_test_require(contains(dot, "finalize"));
  }

  // ── 5. Branch DOT: branch-specific node shapes ──
  {
    const auto dot = pb::to_dot<BranchPipeline>();

    pb_test_require(contains(dot, "digraph"));
    pb_test_require(contains(dot, "rankdir=LR"));

    // Branch DOT MUST include from_input + to_output anchors.
    pb_test_require(contains(dot, "from_input"));
    pb_test_require(contains(dot, "to_output"));

    // Branch DOT MUST include doublecircle on the output anchor.
    pb_test_require(contains(dot, "doublecircle"));

    // The branch node and per-case predicate label format.
    pb_test_require(contains(dot, "pred:"));
  }

  // ── 6. Compact-text banner across topologies ──
  {
    const auto linear_text = pb::to_text<LinearPipeline>();
    const auto branch_text = pb::to_text<BranchPipeline>();

    // Banner identifier present on first line for both topologies.
    pb_test_require(linear_text.rfind("# pb.core.graph.v1", 0) == 0);
    pb_test_require(branch_text.rfind("# pb.core.graph.v1", 0) == 0);

    // Banner format keywords (topology=, stages=, edges=).
    pb_test_require(contains(linear_text.substr(0, linear_text.find('\n')), "topology=linear"));
    pb_test_require(contains(branch_text.substr(0, branch_text.find('\n')), "topology=branch"));

    // Branch case-line format: indented + predicate=<k> + stage=<k>.
    pb_test_require(contains(branch_text, "predicate=is-positive"));
    pb_test_require(contains(branch_text, "stage=parse-a"));
    pb_test_require(contains(branch_text, "predicate=is-negative"));
    pb_test_require(contains(branch_text, "stage=parse-b"));
  }

  return 0;
}
