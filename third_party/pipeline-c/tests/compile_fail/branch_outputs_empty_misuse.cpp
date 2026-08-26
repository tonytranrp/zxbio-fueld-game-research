#include <pb/pipeline.hpp>

// Branch output validation requires at least one case to produce metadata.
using EmptyOutputs = pb::branch_outputs<>;
static_assert(sizeof(EmptyOutputs) > 0);

int main() { return 0; }
