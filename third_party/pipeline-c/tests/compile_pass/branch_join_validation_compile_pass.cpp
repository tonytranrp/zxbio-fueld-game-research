#include <pb/pipeline.hpp>
#include <type_traits>

struct Raw { int value{}; };
struct Decision { int code{}; };

struct IsEven {
  using input_type = Raw;
  using output_type = bool;
  auto operator()(Raw r) const -> bool { return r.value % 2 == 0; }
};
struct IsOdd {
  using input_type = Raw;
  using output_type = bool;
  auto operator()(Raw r) const -> bool { return r.value % 2 != 0; }
};
struct Double {
  using input_type = Raw;
  using output_type = Decision;
  auto operator()(Raw r) const -> Decision { return {r.value * 2}; }
};
struct Triple {
  using input_type = Raw;
  using output_type = Decision;
  auto operator()(Raw r) const -> Decision { return {r.value * 3}; }
};
struct Finish {
  using input_type = Decision;
  using output_type = int;
  auto operator()(Decision d) const -> int { return d.code + 1; }
};

using EvenCase = pb::case_<IsEven>::then<Double>;
using OddCase = pb::case_<IsOdd>::then<Triple>;

using BranchJoinPipeline = pb::from<Raw>::branch<EvenCase, OddCase>::join<Finish>::to<int>;
static_assert(pb::valid<BranchJoinPipeline>);
static_assert(pb::pipeline_size_v<BranchJoinPipeline> == 2);
static_assert(std::is_same_v<pb::pipeline_output_t<BranchJoinPipeline>, int>);

int main() { return 0; }
