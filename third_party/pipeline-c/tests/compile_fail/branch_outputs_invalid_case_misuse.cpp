#include <pb/pipeline.hpp>

struct NotABranchCase {};

// Phase 5 branch output markers accept only pb::case_<Predicate>::then<Stage>
// branch-case metadata; topology execution remains intentionally unsupported.
using BadOutputs = pb::branch_outputs<NotABranchCase>;
static_assert(sizeof(BadOutputs) > 0);

int main() { return 0; }
