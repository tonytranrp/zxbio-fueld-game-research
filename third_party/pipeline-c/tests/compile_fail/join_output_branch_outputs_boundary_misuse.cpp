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
using Outputs = pb::branch_outputs<Case>;

// Join output metadata is produced from join_node metadata, not branch output packs.
using BadJoinOutput = pb::join_output<Outputs>;
static_assert(sizeof(BadJoinOutput) > 0);

int main() { return 0; }
