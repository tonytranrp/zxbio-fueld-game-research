#include <pb/pipeline.hpp>

struct Parsed {};
struct Joined {};

struct JoinStage {
  using input_type = Parsed;
  using output_type = Joined;
};

using Join = pb::join_node<JoinStage>;

// Join validation receives branch output metadata, not arbitrary marker types.
using BadJoinValidation = pb::join_validation<Join, Join>;
static_assert(sizeof(BadJoinValidation) > 0);

int main() { return 0; }
