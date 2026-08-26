#include <pb/pipeline.hpp>

struct Raw {};
struct Parsed {};
struct Reviewed {};

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

using ParseCase = pb::case_<Predicate>::then<Parse>;
using ReviewCase = pb::case_<Predicate>::then<ManualReview>;
using Outputs = pb::branch_outputs<ParseCase, ReviewCase>;

// Branch output validation is the narrow Phase 5 compatibility check for a
// requested common branch output type. Mixed branch outputs need an explicit
// join/mapping stage before they can be treated as one output type.
using BadValidation = pb::branch_output_validation<Outputs, Parsed>;
static_assert(sizeof(BadValidation) > 0);

int main() { return 0; }
