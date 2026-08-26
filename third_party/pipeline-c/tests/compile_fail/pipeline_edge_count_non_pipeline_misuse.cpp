#include <pb/pipeline.hpp>

struct NotAPipeline {};

// Should fail with a public ValidPipeline constraint diagnostic for edge count helper misuse.
static_assert(pb::pipeline_edge_count_v<NotAPipeline> == 0);

int main() { return 0; }
