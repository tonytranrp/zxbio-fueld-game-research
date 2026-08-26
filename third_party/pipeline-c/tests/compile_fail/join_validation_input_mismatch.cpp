#include <pb/pipeline.hpp>

struct Raw {};
struct Parsed {};
struct Reviewed {};
struct Joined {};

struct ParsePredicate {
  using input_type = Raw;
  using output_type = bool;
};

struct ReviewPredicate {
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

struct JoinWrongInput {
  using input_type = Parsed;
  using output_type = Joined;
};

using ParseCase = pb::case_<ParsePredicate>::then<Parse>;
using ReviewCase = pb::case_<ReviewPredicate>::then<Review>;
using Outputs = pb::branch_outputs<ParseCase, ReviewCase>;
using Join = pb::join_node<JoinWrongInput>;

// Join validation must consume the whole branch output type-list, not just one
// branch result. This keeps marker-only Phase 5 validation honest while branch
// execution remains unsupported.
using BadJoinValidation = pb::join_validation<Outputs, Join>;
static_assert(sizeof(BadJoinValidation) > 0);

int main() { return 0; }
