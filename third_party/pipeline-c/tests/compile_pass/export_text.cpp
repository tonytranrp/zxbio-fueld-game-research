#include <pb/pipeline.hpp>

#include <cassert>
#include <string>
#include <string_view>

namespace {

struct Raw   { int value{}; };
struct Routed { int value{}; };
struct Done  { int value{}; };

struct IsEven {
  using input_type  = Raw;
  using output_type = bool;
  static constexpr auto stage_key()  noexcept { return "is.even"; }
  static constexpr auto stage_name() noexcept { return "is even"; }
  bool operator()(Raw raw) const { return raw.value % 2 == 0; }
};

struct IsOdd {
  using input_type  = Raw;
  using output_type = bool;
  static constexpr auto stage_key()  noexcept { return "is.odd"; }
  static constexpr auto stage_name() noexcept { return "is odd"; }
  bool operator()(Raw raw) const { return raw.value % 2 != 0; }
};

struct EvenRoute {
  using input_type  = Raw;
  using output_type = Routed;
  static constexpr auto stage_key()  noexcept { return "route.even"; }
  static constexpr auto stage_name() noexcept { return "route even"; }
  Routed operator()(Raw raw) const { return {raw.value * 2}; }
};

struct OddRoute {
  using input_type  = Raw;
  using output_type = Routed;
  static constexpr auto stage_key()  noexcept { return "route.odd"; }
  static constexpr auto stage_name() noexcept { return "route odd"; }
  Routed operator()(Raw raw) const { return {raw.value * 3}; }
};

struct LinearRoute {
  using input_type  = Raw;
  using output_type = Routed;
  static constexpr auto stage_key()  noexcept { return "route.linear"; }
  static constexpr auto stage_name() noexcept { return "route linear"; }
  Routed operator()(Raw raw) const { return {raw.value}; }
};

struct Finish {
  using input_type  = Routed;
  using output_type = Done;
  static constexpr auto stage_key()  noexcept { return "finish"; }
  static constexpr auto stage_name() noexcept { return "finish"; }
  Done operator()(Routed routed) const { return {routed.value + 1}; }
};

using EvenCase = pb::case_<IsEven>::then<EvenRoute>;
using OddCase  = pb::case_<IsOdd>::then<OddRoute>;

using LinearPipeline = pb::from<Raw>::then<LinearRoute>::then<Finish>::to<Done>;
using BranchPipeline = pb::from<Raw>::branch<EvenCase, OddCase>::join<Finish>::to<Done>;

constexpr auto contains(std::string_view haystack, std::string_view needle) noexcept -> bool {
  return haystack.find(needle) != std::string_view::npos;
}

} // namespace

int main() {
  const std::string linear_text = pb::to_text<LinearPipeline>();
  assert(contains(linear_text, "# pb.core.graph.v1"));
  assert(contains(linear_text, "topology=linear"));
  assert(contains(linear_text, "stages=2"));
  assert(contains(linear_text, "edges=1"));
  assert(contains(linear_text, "[0] route.linear"));
  assert(contains(linear_text, "[1] finish"));
  assert(contains(linear_text, "edges:"));
  assert(contains(linear_text, "0 -> 1  route.linear -> finish"));

  const std::string branch_text = pb::to_text<BranchPipeline>();
  assert(contains(branch_text, "topology=branch"));
  assert(contains(branch_text, "stages=2"));
  assert(contains(branch_text, "[0] branch"));
  assert(contains(branch_text, "case 0  predicate=is.even  stage=route.even"));
  assert(contains(branch_text, "case 1  predicate=is.odd  stage=route.odd"));
  assert(contains(branch_text, "[1] finish"));
  assert(contains(branch_text, "0 -> 1  branch -> finish"));

  // Single-stage pipeline should still emit zero-edge text without a trailing
  // "edges:" header — guards against accidental empty-edges-section regressions.
  using IdentityPipeline = pb::from<Raw>::then<LinearRoute>::to<Routed>;
  const std::string identity_text = pb::to_text<IdentityPipeline>();
  assert(contains(identity_text, "stages=1"));
  assert(contains(identity_text, "edges=0"));
  assert(!contains(identity_text, "edges:"));

  return 0;
}
