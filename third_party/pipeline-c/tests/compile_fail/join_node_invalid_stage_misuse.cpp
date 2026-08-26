#include <pb/pipeline.hpp>

struct NotAJoinStage {};

// Phase 5 join markers require the join target to at least be a valid stage.
using BadJoin = pb::join_node<NotAJoinStage>;
static_assert(sizeof(BadJoin) > 0);

int main() { return 0; }
