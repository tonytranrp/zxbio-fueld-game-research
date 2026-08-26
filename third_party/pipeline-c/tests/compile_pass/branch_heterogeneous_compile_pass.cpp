#include <pb/pipeline.hpp>

#include <type_traits>
#include <variant>

struct Raw {};
struct Parsed {};
struct Reviewed {};
struct Joined {};

struct IsParseable {
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
  using output_type = Reviewed;
};

struct JoinResults {
  using input_type = std::variant<Parsed, Reviewed>;
  using output_type = Joined;
  auto operator()(std::variant<Parsed, Reviewed>) const -> Joined { return {}; }
};

using ParseCase = pb::case_<IsParseable>::then<Parse>;
using ReviewCase = pb::case_<NeedsReview>::then<Review>;
using Outputs = pb::branch_outputs<ParseCase, ReviewCase>;
static_assert(std::same_as<Outputs::output_types, pb::meta::type_list<Parsed, Reviewed>>);
static_assert(std::same_as<Outputs::output_type, std::variant<Parsed, Reviewed>>);
static_assert(std::same_as<pb::branch_raw_output_types_t<ParseCase, ReviewCase>,
                           pb::meta::type_list<Parsed, Reviewed>>);
static_assert(std::same_as<pb::branch_unified_output_t<ParseCase, ReviewCase>,
                           std::variant<Parsed, Reviewed>>);
using UnifiedOutputValidation =
    pb::branch_unified_output_validation<Outputs, std::variant<Parsed, Reviewed>>;
static_assert(std::same_as<UnifiedOutputValidation::branch_outputs_type, Outputs>);
static_assert(std::same_as<UnifiedOutputValidation::raw_output_types,
                           pb::meta::type_list<Parsed, Reviewed>>);
static_assert(std::same_as<UnifiedOutputValidation::unified_output_type,
                           std::variant<Parsed, Reviewed>>);
static_assert(std::same_as<UnifiedOutputValidation::output_type,
                           std::variant<Parsed, Reviewed>>);
static_assert(UnifiedOutputValidation::output_count == 2);

// Heterogeneous branch outputs produce std::variant<Parsed, Reviewed>
using HeterogeneousBranch = pb::from<Raw>::branch<ParseCase, ReviewCase>;
using BranchOutputType = HeterogeneousBranch::current_type;

static_assert(std::same_as<BranchOutputType, std::variant<Parsed, Reviewed>>,
              "Heterogeneous branch output_type must be std::variant of all branch case output types");

// Join stage accepts the variant
using HeterogeneousJoinPipeline = pb::from<Raw>::branch<ParseCase, ReviewCase>::join<JoinResults>::to<Joined>;
static_assert(pb::valid<HeterogeneousJoinPipeline>);
static_assert(pb::pipeline_size_v<HeterogeneousJoinPipeline> == 2);
static_assert(std::same_as<pb::pipeline_output_t<HeterogeneousJoinPipeline>, Joined>);

// Homogeneous branches still use a single type (backward compatible)
struct ParseOnly {
  using input_type = Raw;
  using output_type = Parsed;
};

struct Finish {
  using input_type = Parsed;
  using output_type = int;
  auto operator()(Parsed) const -> int { return 0; }
};

using ParseOnlyCase = pb::case_<IsParseable>::then<ParseOnly>;
using HomogeneousBranch = pb::from<Raw>::branch<ParseOnlyCase>;
static_assert(std::same_as<HomogeneousBranch::current_type, Parsed>,
              "Homogeneous branch output_type must be a single type, not std::variant");

using HomogeneousJoinPipeline = pb::from<Raw>::branch<ParseOnlyCase>::join<Finish>::to<int>;
static_assert(pb::valid<HomogeneousJoinPipeline>);
static_assert(std::same_as<pb::pipeline_output_t<HomogeneousJoinPipeline>, int>);

int main() { return 0; }
