#include <pb/pipeline.hpp>

#include <type_traits>
#include <variant>

struct Raw {};
struct Parsed {};
struct Decision {};

struct IsParsed {
  using input_type = Raw;
  using output_type = bool;
};

struct NeedsReview {
  using input_type = Raw;
  using output_type = bool;
};

struct Parse {
  using input_type = Raw;
  using output_type = Parsed;
};

struct Review {
  using input_type = Raw;
  using output_type = Decision;
};

// Heterogeneous branch topology is now supported via std::variant output
using HeterogeneousBranch =
    pb::from<Raw>::branch<pb::case_<IsParsed>::then<Parse>, pb::case_<NeedsReview>::then<Review>>;

// Verify the branch output type is the variant
static_assert(std::same_as<HeterogeneousBranch::current_type, std::variant<Parsed, Decision>>,
              "Heterogeneous branch output must be std::variant of branch case outputs");

// Branch with a join that accepts the variant
struct Finish {
  using input_type = std::variant<Parsed, Decision>;
  using output_type = int;
  auto operator()(std::variant<Parsed, Decision>) const -> int { return 0; }
};

using BranchJoinPipeline =
    pb::from<Raw>::branch<pb::case_<IsParsed>::then<Parse>, pb::case_<NeedsReview>::then<Review>>::join<Finish>::to<int>;
static_assert(pb::valid<BranchJoinPipeline>);
static_assert(pb::pipeline_size_v<BranchJoinPipeline> == 2);
static_assert(std::same_as<pb::pipeline_output_t<BranchJoinPipeline>, int>);

int main() { return 0; }
