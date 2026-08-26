#include <pb/pipeline.hpp>

struct Parsed {};
struct Joined {};

struct JoinStage {
  using input_type = Parsed;
  using output_type = Joined;
};

using Join = pb::join_node<JoinStage>;

// Branch output metadata is produced from branch cases, not join metadata nodes.
using BadBranchOutputs = pb::branch_outputs<Join>;
static_assert(sizeof(BadBranchOutputs) > 0);

int main() { return 0; }
