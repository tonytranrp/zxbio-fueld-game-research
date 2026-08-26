#include <pb/pipeline.hpp>

struct NotAPipeline {};

// Should fail with a public ValidPipeline constraint diagnostic for size helper misuse.
static_assert(pb::pipeline_size_v<NotAPipeline> == 0);

int main() { return 0; }
