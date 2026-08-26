// Byte-equal golden output for pb::to_text<Pipeline> across all three
// topologies covered by the v1 schema.  Any drift in field order, spacing,
// or schema wording trips an assertion here so reviewers must explicitly
// acknowledge it (and update docs/export-schema-v1.md if the change is
// intended).

#include <pb/pipeline.hpp>

#include <cassert>
#include <string>
#include <string_view>

namespace text_golden {

struct Raw  { int value{}; };
struct Mid  { int value{}; };
struct Done { int value{}; };

struct ParseStage {
  using input_type  = Raw;
  using output_type = Mid;
  static constexpr auto stage_key()  noexcept { return "parse"; }
  static constexpr auto stage_name() noexcept { return "parse"; }
  Mid operator()(Raw raw) const { return {raw.value + 1}; }
};

struct FinishStage {
  using input_type  = Mid;
  using output_type = Done;
  static constexpr auto stage_key()  noexcept { return "finish"; }
  static constexpr auto stage_name() noexcept { return "Finish"; }
  Done operator()(Mid mid) const { return {mid.value + 1}; }
};

struct IsEven {
  using input_type  = Raw;
  using output_type = bool;
  static constexpr auto stage_key()  noexcept { return "is.even"; }
  static constexpr auto stage_name() noexcept { return "is.even"; }
  bool operator()(Raw raw) const { return raw.value % 2 == 0; }
};

struct IsOdd {
  using input_type  = Raw;
  using output_type = bool;
  static constexpr auto stage_key()  noexcept { return "is.odd"; }
  static constexpr auto stage_name() noexcept { return "is.odd"; }
  bool operator()(Raw raw) const { return raw.value % 2 != 0; }
};

struct EvenRoute {
  using input_type  = Raw;
  using output_type = Mid;
  static constexpr auto stage_key()  noexcept { return "route.even"; }
  static constexpr auto stage_name() noexcept { return "route.even"; }
  Mid operator()(Raw raw) const { return {raw.value * 2}; }
};

struct OddRoute {
  using input_type  = Raw;
  using output_type = Mid;
  static constexpr auto stage_key()  noexcept { return "route.odd"; }
  static constexpr auto stage_name() noexcept { return "route.odd"; }
  Mid operator()(Raw raw) const { return {raw.value * 3}; }
};

using EvenCase = pb::case_<IsEven>::then<EvenRoute>;
using OddCase  = pb::case_<IsOdd>::then<OddRoute>;

using LinearPipeline = pb::from<Raw>::then<ParseStage>::then<FinishStage>::to<Done>;
using BranchPipeline = pb::from<Raw>::branch<EvenCase, OddCase>::join<FinishStage>::to<Done>;

constexpr auto linear_expected =
    "# pb.core.graph.v1  topology=linear  stages=2  edges=1\n"
    "[0] parse\n"
    "[1] finish  \"Finish\"\n"
    "edges:\n"
    "  0 -> 1  parse -> finish\n";

constexpr auto branch_expected =
    "# pb.core.graph.v1  topology=branch  stages=2  edges=1\n"
    "[0] branch\n"
    "    case 0  predicate=is.even  stage=route.even\n"
    "    case 1  predicate=is.odd  stage=route.odd\n"
    "[1] finish  \"Finish\"\n"
    "edges:\n"
    "  0 -> 1  branch -> finish\n";

} // namespace text_golden

int main() {
  const std::string linear_text = pb::to_text<text_golden::LinearPipeline>();
  const std::string branch_text = pb::to_text<text_golden::BranchPipeline>();

  assert(linear_text == text_golden::linear_expected);
  assert(branch_text == text_golden::branch_expected);

  // Single-stage pipelines must skip the `edges:` block entirely — this is
  // part of the v1 contract because consumers grep for `edges:` as a section
  // marker.
  using IdentityPipeline =
      pb::from<text_golden::Raw>::then<text_golden::ParseStage>::to<text_golden::Mid>;
  const std::string identity_text = pb::to_text<IdentityPipeline>();
  constexpr auto identity_expected =
      "# pb.core.graph.v1  topology=linear  stages=1  edges=0\n"
      "[0] parse\n";
  assert(identity_text == identity_expected);

  return 0;
}
