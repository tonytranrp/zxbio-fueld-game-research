#include <pb/pipeline.hpp>

struct NotAPipeline {};

// Should fail with a public ValidPipeline constraint diagnostic for empty helper misuse.
static_assert(pb::pipeline_empty_v<NotAPipeline>);

int main() { return 0; }
