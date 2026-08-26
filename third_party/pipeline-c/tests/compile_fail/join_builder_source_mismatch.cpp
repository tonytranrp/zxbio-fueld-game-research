#include <pb/pipeline.hpp>

struct Raw {};
struct Parsed {};
struct Reviewed {};
struct Joined {};

struct Predicate {
  using input_type = Raw;
  using output_type = bool;
};

struct Parse {
  using input_type = Raw;
  using output_type = Parsed;
};

struct ManualReview {
  using input_type = Raw;
  using output_type = Reviewed;
};

struct JoinParsedOnly {
  using input_type = Parsed;
  using output_type = Joined;
};

using ParseCase = pb::case_<Predicate>::then<Parse>;
using ReviewCase = pb::case_<Predicate>::then<ManualReview>;
using Outputs = pb::branch_outputs<ParseCase, ReviewCase>;

// The eventual public ::join<Stage> builder must consume the whole branch
// output pack. A join stage that consumes only one branch output is not a valid
// join source for this branch fan-out.
using BadJoinBuilder = pb::join_builder_validation<Outputs, JoinParsedOnly>;
static_assert(sizeof(BadJoinBuilder) > 0);

int main() { return 0; }
