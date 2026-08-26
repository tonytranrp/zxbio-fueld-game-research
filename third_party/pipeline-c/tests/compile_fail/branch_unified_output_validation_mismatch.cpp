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

// Unified output validation checks the execution output type, which is
// std::variant<Parsed, Reviewed> for this heterogeneous branch. A single raw
// case output type is not the unified output.
using BadValidation = pb::branch_unified_output_validation<Outputs, Parsed>;
static_assert(sizeof(BadValidation) > 0);

int main() { return 0; }
