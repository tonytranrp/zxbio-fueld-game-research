#include <pb/pipeline.hpp>

#include <cassert>
#include <string_view>

struct Raw { int value{}; };
struct Routed { int value{}; };
struct Done { int value{}; };

struct IsEven {
  using input_type = Raw;
  using output_type = bool;
  static constexpr auto stage_key() noexcept { return "is.even"; }
  static constexpr auto stage_name() noexcept { return "IsEven"; }
  bool operator()(Raw raw) const { return raw.value % 2 == 0; }
};

struct IsOdd {
  using input_type = Raw;
  using output_type = bool;
  static constexpr auto stage_key() noexcept { return "is.odd"; }
  static constexpr auto stage_name() noexcept { return "IsOdd"; }
  bool operator()(Raw raw) const { return raw.value % 2 != 0; }
};

struct EvenRoute {
  using input_type = Raw;
  using output_type = Routed;
  static constexpr auto stage_key() noexcept { return "route.even"; }
  static constexpr auto stage_name() noexcept { return "EvenRoute"; }
  Routed operator()(Raw raw) const { return {raw.value * 2}; }
};

struct OddRoute {
  using input_type = Raw;
  using output_type = Routed;
  static constexpr auto stage_key() noexcept { return "route.odd"; }
  static constexpr auto stage_name() noexcept { return "OddRoute"; }
  Routed operator()(Raw raw) const { return {raw.value * 3}; }
};

struct Finish {
  using input_type = Routed;
  using output_type = Done;
  static constexpr auto stage_key() noexcept { return "finish"; }
  static constexpr auto stage_name() noexcept { return "Finish"; }
  Done operator()(Routed routed) const { return {routed.value + 1}; }
};

using EvenCase = pb::case_<IsEven>::then<EvenRoute>;
using OddCase = pb::case_<IsOdd>::then<OddRoute>;

// Branch + join pipeline
using BranchJoinPipeline = pb::from<Raw>::branch<EvenCase, OddCase>::join<Finish>::to<Done>;

// Branch-only pipeline (no join)
using BranchOnlyPipeline = pb::from<Raw>::branch<EvenCase, OddCase>::to<Routed>;

int main() {
  // ── Branch + join DOT output ──────────────────────────────────
  {
    const auto dot = pb::to_dot<BranchJoinPipeline>("branch_pipeline");

    // Graph header
    assert(dot.find("digraph branch_pipeline") != std::string_view::npos);
    assert(dot.find("rankdir=LR") != std::string_view::npos);

    // Input and output boundary nodes
    assert(dot.find("from_input [label=\"Input\"]") != std::string_view::npos);
    assert(dot.find("to_output [shape=doublecircle, label=\"Output\"]") != std::string_view::npos);

    // Branch diamond
    assert(dot.find("branch_0 [shape=diamond, label=\"branch\"]") != std::string_view::npos);

    // Case 0 subgraph
    assert(dot.find("subgraph cluster_case_0_0") != std::string_view::npos);
    assert(dot.find("label=\"Case 0: IsEven\"") != std::string_view::npos);
    assert(dot.find("pred_0_0 [label=\"pred: IsEven\"]") != std::string_view::npos);
    assert(dot.find("case_0_0 [label=\"EvenRoute\"]") != std::string_view::npos);

    // Case 1 subgraph
    assert(dot.find("subgraph cluster_case_0_1") != std::string_view::npos);
    assert(dot.find("label=\"Case 1: IsOdd\"") != std::string_view::npos);
    assert(dot.find("pred_0_1 [label=\"pred: IsOdd\"]") != std::string_view::npos);
    assert(dot.find("case_0_1 [label=\"OddRoute\"]") != std::string_view::npos);

    // Branch -> predicate dashed test edges (inside case subgraphs)
    assert(dot.find("branch_0 -> pred_0_0 [style=dashed, label=\"test\"]") != std::string_view::npos);
    assert(dot.find("branch_0 -> pred_0_1 [style=dashed, label=\"test\"]") != std::string_view::npos);

    // Predicate -> case stage edges
    assert(dot.find("pred_0_0 -> case_0_0") != std::string_view::npos);
    assert(dot.find("pred_0_1 -> case_0_1") != std::string_view::npos);

    // Input -> branch edge
    assert(dot.find("from_input -> branch_0") != std::string_view::npos);

    // Case stages -> join stage (regular node at index 1)
    assert(dot.find("case_0_0 -> stage_1") != std::string_view::npos);
    assert(dot.find("case_0_1 -> stage_1") != std::string_view::npos);

    // Join stage node
    assert(dot.find("stage_1 [label=\"Finish") != std::string_view::npos);

    // Join -> output edge
    assert(dot.find("stage_1 -> to_output") != std::string_view::npos);
  }

  // ── Branch-only DOT output (no join) ──────────────────────────
  {
    const auto dot = pb::to_dot<BranchOnlyPipeline>("branch_only");

    assert(dot.find("digraph branch_only") != std::string_view::npos);

    // Branch diamond present
    assert(dot.find("branch_0 [shape=diamond, label=\"branch\"]") != std::string_view::npos);

    // Case stage nodes present
    assert(dot.find("case_0_0 [label=\"EvenRoute\"]") != std::string_view::npos);
    assert(dot.find("case_0_1 [label=\"OddRoute\"]") != std::string_view::npos);

    // Case stages -> output (no join stage in between)
    assert(dot.find("case_0_0 -> to_output") != std::string_view::npos);
    assert(dot.find("case_0_1 -> to_output") != std::string_view::npos);

    // No stage_1 node (only branch stage exists)
    assert(dot.find("stage_1") == std::string_view::npos);
  }

  return 0;
}
