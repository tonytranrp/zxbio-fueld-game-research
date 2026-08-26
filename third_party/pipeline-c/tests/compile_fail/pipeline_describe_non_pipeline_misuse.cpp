#include <pb/pipeline.hpp>

struct NotAPipeline {};

// Should fail with a public ValidPipeline constraint diagnostic for descriptor count access misuse.
static_assert(pb::describe<NotAPipeline>().edge_count == 0);

int main() { return 0; }
