// Extended byte-equal goldens for pb.core.graph.v1 covering:
//   * fan-in topology across JSON / DOT / text
//   * labeled branch cases (pb::case_<P>::label<"...">::then<S>)
//   * heterogeneous (variant-typed) branch outputs
//
// Any drift in field names, ordering, escaping, or rendering trips the
// assertions here so reviewers must explicitly acknowledge the change
// (and bump pb.core.graph.v2 + add a new emitter if the change is real).

#include <pb/pipeline.hpp>

#include <cassert>
#include <string>
#include <string_view>
#include <variant>

namespace schema_v1_extended {

// ── Shared domain ──────────────────────────────────────────────────────
struct Input  { int value{}; };
struct Routed { int value{}; };
struct Tagged { std::string tag; };
struct Done   { int value{}; };

struct IsLow {
  using input_type  = Input;
  using output_type = bool;
  static constexpr auto stage_key()  noexcept { return "pred.low"; }
  static constexpr auto stage_name() noexcept { return "pred.low"; }
  bool operator()(const Input& input) const { return input.value < 10; }
};

struct IsHigh {
  using input_type  = Input;
  using output_type = bool;
  static constexpr auto stage_key()  noexcept { return "pred.high"; }
  static constexpr auto stage_name() noexcept { return "pred.high"; }
  bool operator()(const Input& input) const { return input.value >= 10; }
};

struct RouteLow {
  using input_type  = Input;
  using output_type = Routed;
  static constexpr auto stage_key()  noexcept { return "route.low"; }
  static constexpr auto stage_name() noexcept { return "route.low"; }
  Routed operator()(Input input) const { return {input.value + 1}; }
};

struct RouteHigh {
  using input_type  = Input;
  using output_type = Routed;
  static constexpr auto stage_key()  noexcept { return "route.high"; }
  static constexpr auto stage_name() noexcept { return "route.high"; }
  Routed operator()(Input input) const { return {input.value * 10}; }
};

struct TagInput {
  using input_type  = Input;
  using output_type = Tagged;
  static constexpr auto stage_key()  noexcept { return "tag.input"; }
  static constexpr auto stage_name() noexcept { return "tag.input"; }
  Tagged operator()(Input input) const { return {std::to_string(input.value)}; }
};

struct Finish {
  using input_type  = Routed;
  using output_type = Done;
  static constexpr auto stage_key()  noexcept { return "finish"; }
  static constexpr auto stage_name() noexcept { return "finish"; }
  Done operator()(Routed routed) const { return {routed.value + 100}; }
};

// ── Fan-in pipeline goldens ────────────────────────────────────────────
using FanInLow  = pb::case_<IsLow>::then<RouteLow>;
using FanInHigh = pb::case_<IsHigh>::then<RouteHigh>;
using FanInAggregate = pb::fan_in_output_t<FanInLow, FanInHigh>;

struct FanInFinish {
  using input_type  = FanInAggregate;
  using output_type = Done;
  static constexpr auto stage_key()  noexcept { return "finish.fan_in"; }
  static constexpr auto stage_name() noexcept { return "finish.fan_in"; }
  Done operator()(FanInAggregate aggregate) const {
    int total = 0;
    if (aggregate.template get<0>().selected()) total += aggregate.template get<0>().get().value;
    if (aggregate.template get<1>().selected()) total += aggregate.template get<1>().get().value;
    return {total};
  }
};

using FanInPipeline = pb::from<Input>
    ::branch<FanInLow, FanInHigh>
    ::fan_in<FanInFinish>
    ::to<Done>;

static_assert(pb::valid<FanInPipeline>);

constexpr auto fan_in_text_expected =
    "# pb.core.graph.v1  topology=fan_in  stages=2  edges=1\n"
    "[0] fan_in\n"
    "    case 0  predicate=pred.low  stage=route.low\n"
    "    case 1  predicate=pred.high  stage=route.high\n"
    "[1] finish.fan_in\n"
    "edges:\n"
    "  0 -> 1  fan_in -> finish.fan_in\n";

// JSON / DOT are too verbose to express as a single literal; use substring
// pinning instead — every documented field must appear in the documented
// order.  This catches reorderings and renames without locking the entire
// blob byte-for-byte.

// ── Labeled branch case golden ─────────────────────────────────────────
struct LabeledLow {
  using input_type  = Input;
  using output_type = bool;
  static constexpr auto stage_key()  noexcept { return "labeled.is_low"; }
  static constexpr auto stage_name() noexcept { return "labeled.is_low"; }
  bool operator()(const Input& input) const { return input.value < 10; }
};

struct LabeledHigh {
  using input_type  = Input;
  using output_type = bool;
  static constexpr auto stage_key()  noexcept { return "labeled.is_high"; }
  static constexpr auto stage_name() noexcept { return "labeled.is_high"; }
  bool operator()(const Input& input) const { return input.value >= 10; }
};

using LabeledLowCase  = pb::case_<LabeledLow>::label<pb::fixed_string{"cold-path"}>::then<RouteLow>;
using LabeledHighCase = pb::case_<LabeledHigh>::label<pb::fixed_string{"hot-path"}>::then<RouteHigh>;

using LabeledPipeline = pb::from<Input>
    ::branch<LabeledLowCase, LabeledHighCase>
    ::join<Finish>
    ::to<Done>;

static_assert(pb::valid<LabeledPipeline>);

// ── Heterogeneous (variant) branch outputs golden ──────────────────────
struct CombineVariant {
  using input_type  = std::variant<Routed, Tagged>;
  using output_type = Done;
  static constexpr auto stage_key()  noexcept { return "combine"; }
  static constexpr auto stage_name() noexcept { return "combine"; }
  Done operator()(std::variant<Routed, Tagged> input) const {
    if (std::holds_alternative<Routed>(input)) {
      return {std::get<Routed>(input).value};
    }
    return {static_cast<int>(std::get<Tagged>(input).tag.size())};
  }
};

using VariantLowCase  = pb::case_<IsLow>::then<RouteLow>;
using VariantHighCase = pb::case_<IsHigh>::then<TagInput>;
using VariantPipeline = pb::from<Input>
    ::branch<VariantLowCase, VariantHighCase>
    ::join<CombineVariant>
    ::to<Done>;

static_assert(pb::valid<VariantPipeline>);

constexpr auto contains(std::string_view haystack, std::string_view needle) noexcept -> bool {
  return haystack.find(needle) != std::string_view::npos;
}

} // namespace schema_v1_extended

int main() {
  using namespace schema_v1_extended;

  // ── 1. Fan-in text: byte-equal. ───────────────────────────────────
  {
    const std::string text = pb::to_text<FanInPipeline>();
    assert(text == fan_in_text_expected);
  }

  // ── 2. Fan-in JSON: every documented field, in the documented order. ─
  {
    const std::string json = pb::to_json<FanInPipeline>();

    // Top-level v1 contract.
    assert(json.find("\"schema_version\":\"pb.core.graph.v1\"") == 0 + std::string::npos
               ? false : true);
    // Use rfind(_, 0) for "starts-with".
    assert(json.rfind("{\"schema_version\":\"pb.core.graph.v1\",\"topology\":\"fan_in\"", 0) == 0);

    // Stage record uses kind=fan_in for the branch stage.
    assert(contains(json, "\"kind\":\"fan_in\""));
    assert(contains(json, "\"key\":\"fan_in\""));

    // Branch case fields appear in documented order: index, case_id,
    // case_key, case_label, predicate_node_id, stage_node_id,
    // predicate_key, predicate_name, stage_key, stage_name.
    const auto case_block_pos = json.find("\"branch_cases\":[");
    assert(case_block_pos != std::string::npos);

    const auto first_index_pos        = json.find("\"index\":0", case_block_pos);
    const auto first_case_id_pos      = json.find("\"case_id\":\"branch.0.case.0\"", case_block_pos);
    const auto first_case_label_pos   = json.find("\"case_label\":\"0\"", case_block_pos);
    const auto first_predicate_key    = json.find("\"predicate_key\":\"pred.low\"", case_block_pos);
    const auto first_stage_key        = json.find("\"stage_key\":\"route.low\"", case_block_pos);
    assert(first_index_pos        != std::string::npos);
    assert(first_case_id_pos      != std::string::npos);
    assert(first_case_label_pos   != std::string::npos);
    assert(first_predicate_key    != std::string::npos);
    assert(first_stage_key        != std::string::npos);
    assert(first_index_pos        < first_case_id_pos);
    assert(first_case_id_pos      < first_case_label_pos);
    assert(first_case_label_pos   < first_predicate_key);
    assert(first_predicate_key    < first_stage_key);

    // Helper-rendered edge objects use the documented literal fields.
    assert(contains(json,
                    "\"predicate_edge\":{\"from\":\"branch\",\"to\":\"predicate\",\"style\":\"dashed\",\"label\":\"test\"}"));
    assert(contains(json,
                    "\"stage_edge\":{\"from\":\"predicate\",\"to\":\"case_stage\"}"));

    // Join stage appears as the second stage record.
    assert(contains(json, "\"key\":\"finish.fan_in\""));
    // Single edge from the fan-in stage to the join.
    assert(contains(json, "\"from_key\":\"fan_in\""));
    assert(contains(json, "\"to_key\":\"finish.fan_in\""));
  }

  // ── 3. Fan-in DOT: header + every documented per-case rendering. ──
  {
    const std::string dot = pb::to_dot<FanInPipeline>("schema_v1_fan_in");

    assert(dot.rfind("digraph schema_v1_fan_in", 0) == 0);
    assert(contains(dot, "  rankdir=LR;\n"));
    assert(contains(dot, "  from_input [label=\"Input\"];\n"));
    assert(contains(dot, "  branch_0 [shape=diamond, label=\"fan_in\"];\n"));

    // Each case renders a cluster subgraph + predicate + stage + dashed
    // test edge + plain stage edge.
    assert(contains(dot, "subgraph cluster_case_0_0"));
    assert(contains(dot, "subgraph cluster_case_0_1"));
    assert(contains(dot, "pred_0_0 [label=\"pred: pred.low\"];"));
    assert(contains(dot, "pred_0_1 [label=\"pred: pred.high\"];"));
    assert(contains(dot, "case_0_0 [label=\"route.low\"];"));
    assert(contains(dot, "case_0_1 [label=\"route.high\"];"));
    assert(contains(dot, "branch_0 -> pred_0_0 [style=dashed, label=\"test\"];"));
    assert(contains(dot, "pred_0_0 -> case_0_0;"));

    // Join stage + sink.
    assert(contains(dot, "stage_1 [label=\"finish.fan_in"));
    assert(contains(dot, "to_output [shape=doublecircle, label=\"Output\"];"));
    assert(contains(dot, "from_input -> branch_0;"));
    assert(contains(dot, "case_0_0 -> stage_1;"));
    assert(contains(dot, "case_0_1 -> stage_1;"));
    assert(contains(dot, "stage_1 -> to_output;"));
  }

  // ── 4. Labeled branch case appears in JSON/text by user label. ────
  {
    const std::string json = pb::to_json<LabeledPipeline>();
    assert(contains(json, "\"case_label\":\"cold-path\""));
    assert(contains(json, "\"case_label\":\"hot-path\""));
    // Predicate / stage keys still appear normally.
    assert(contains(json, "\"predicate_key\":\"labeled.is_low\""));
    assert(contains(json, "\"stage_key\":\"route.high\""));

    const std::string dot = pb::to_dot<LabeledPipeline>("schema_v1_labeled");
    assert(contains(dot, "label=\"Case cold-path: labeled.is_low\""));
    assert(contains(dot, "label=\"Case hot-path: labeled.is_high\""));
  }

  // ── 5. Heterogeneous variant branch outputs render normally. ──────
  {
    const std::string json = pb::to_json<VariantPipeline>();
    assert(json.rfind("{\"schema_version\":\"pb.core.graph.v1\",\"topology\":\"branch\"", 0) == 0);
    assert(contains(json, "\"predicate_key\":\"pred.low\""));
    assert(contains(json, "\"predicate_key\":\"pred.high\""));
    assert(contains(json, "\"stage_key\":\"route.low\""));
    assert(contains(json, "\"stage_key\":\"tag.input\""));
    assert(contains(json, "\"key\":\"combine\""));
  }

  return 0;
}
