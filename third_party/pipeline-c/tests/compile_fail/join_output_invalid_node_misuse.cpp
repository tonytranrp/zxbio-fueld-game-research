#include <pb/pipeline.hpp>

struct Parsed {};
struct Joined {};

struct JoinStage {
  using input_type = Parsed;
  using output_type = Joined;
};

// Phase 5 join output metadata is exposed for pb::join_node<Stage>, not the
// raw join stage itself.
using BadJoinOutput = pb::join_output<JoinStage>;
static_assert(sizeof(BadJoinOutput) > 0);

int main() { return 0; }
