#include <pb/pipeline.hpp>
#include <type_traits>

// Verify that move-only branch inputs are supported.
// Predicates inspect the input by const&, and the selected branch
// stage receives the input by move.

struct MoveOnly {
  MoveOnly() = default;
  MoveOnly(MoveOnly&&) = default;
  MoveOnly& operator=(MoveOnly&&) = default;
  MoveOnly(const MoveOnly&) = delete;
  MoveOnly& operator=(const MoveOnly&) = delete;
  int value{};
};

struct Routed {
  int value{};
};

struct Always {
  using input_type = MoveOnly;
  using output_type = bool;
  bool operator()(const MoveOnly&) const { return true; }
};

struct Never {
  using input_type = MoveOnly;
  using output_type = bool;
  bool operator()(const MoveOnly&) const { return false; }
};

struct Double {
  using input_type = MoveOnly;
  using output_type = Routed;
  auto operator()(const MoveOnly& m) const -> Routed { return Routed{m.value * 2}; }
};

struct Triple {
  using input_type = MoveOnly;
  using output_type = Routed;
  auto operator()(const MoveOnly& m) const -> Routed { return Routed{m.value * 3}; }
};

using CaseA = pb::case_<Always>::then<Double>;
using CaseB = pb::case_<Never>::then<Triple>;

// This pipeline must compile: move-only input is now supported.
using MoveOnlyPipeline = pb::from<MoveOnly>::branch<CaseA, CaseB>::to<Routed>;

static_assert(pb::valid<MoveOnlyPipeline>);
static_assert(std::is_same_v<pb::pipeline_input_t<MoveOnlyPipeline>, MoveOnly>);
static_assert(std::is_same_v<pb::pipeline_output_t<MoveOnlyPipeline>, Routed>);
static_assert(pb::pipeline_size_v<MoveOnlyPipeline> == 1);

int main() { return 0; }
