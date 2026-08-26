#include <pb/pipeline.hpp>

struct Raw {};
struct Parsed {};
struct Joined {};

struct Predicate {
  using input_type = Raw;
  using output_type = bool;
};

struct Parse {
  using input_type = Raw;
  using output_type = Parsed;
};

struct JoinStage {
  using input_type = Parsed;
  using output_type = Joined;
};

using Case = pb::case_<Predicate>::then<Parse>;
using Outputs = pb::branch_outputs<Case>;

// Join validation receives pb::join_node<Stage>, not the raw join stage.
using BadJoinValidation = pb::join_validation<Outputs, JoinStage>;
static_assert(sizeof(BadJoinValidation) > 0);

int main() { return 0; }
