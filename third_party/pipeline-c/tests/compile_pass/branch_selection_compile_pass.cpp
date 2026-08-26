#include <pb/pipeline.hpp>
#include <type_traits>

namespace domain {
struct Raw {
  int selector{};
  int value{};
};

struct Decision {
  int code{};
};

struct IsParseable {
  using input_type = Raw;
  using output_type = bool;

  auto operator()(Raw input) const -> bool { return input.selector == 1; }
};

struct IsReview {
  using input_type = Raw;
  using output_type = bool;

  auto operator()(Raw input) const -> bool { return input.selector == 0; }
};

struct Parse {
  using input_type = Raw;
  using output_type = Decision;
  auto operator()(Raw) const -> Decision { return Decision{1}; }
};

struct Review {
  using input_type = Raw;
  using output_type = Decision;
  auto operator()(Raw) const -> Decision { return Decision{2}; }
};

struct Finish {
  using input_type = Decision;
  using output_type = int;
  auto operator()(Decision decision) const -> int { return decision.code; }
};
} // namespace domain

using ParseCase = pb::case_<domain::IsParseable>::then<domain::Parse>;
using ReviewCase = pb::case_<domain::IsReview>::then<domain::Review>;

using BranchPipeline = pb::from<domain::Raw>::branch<ParseCase, ReviewCase>::to<domain::Decision>;
using BranchJoinPipeline = pb::from<domain::Raw>::branch<ParseCase, ReviewCase>::join<domain::Finish>::to<int>;
using BranchOutputs = pb::branch_outputs<ParseCase, ReviewCase>;
using BranchOutputTypeList = pb::meta::type_list<domain::Decision, domain::Decision>;

static_assert(pb::valid<BranchPipeline>);
static_assert(pb::valid<BranchJoinPipeline>);
static_assert(std::is_same_v<pb::pipeline_input_t<BranchPipeline>, domain::Raw>);
static_assert(std::is_same_v<pb::pipeline_output_t<BranchPipeline>, domain::Decision>);
static_assert(pb::pipeline_size_v<BranchPipeline> == 1);
static_assert(pb::pipeline_size_v<BranchJoinPipeline> == 2);
static_assert(std::is_same_v<pb::pipeline_output_t<BranchJoinPipeline>, int>);
static_assert(std::is_same_v<pb::pipeline_stage_t<BranchPipeline, 0>::input_type, domain::Raw>);
static_assert(std::is_same_v<pb::pipeline_stage_t<BranchJoinPipeline, 1>, domain::Finish>);
static_assert(std::is_same_v<typename BranchOutputs::output_types, BranchOutputTypeList>);
