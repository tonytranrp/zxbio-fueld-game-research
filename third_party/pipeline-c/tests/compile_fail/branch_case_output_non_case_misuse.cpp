#include <pb/pipeline.hpp>

struct Raw {};
struct Parsed {};

struct Predicate {
  using input_type = Raw;
  using output_type = bool;
};

struct Parse {
  using input_type = Raw;
  using output_type = Parsed;
};

using Case = pb::case_<Predicate>::then<Parse>;

// Phase 5 branch output metadata is per branch case, not per branch node.
using BadOutput = pb::branch_case_output<pb::branch_node<Case>>;
static_assert(sizeof(BadOutput) > 0);

int main() { return 0; }
