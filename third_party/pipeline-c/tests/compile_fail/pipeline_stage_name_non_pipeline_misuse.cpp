#include <pb/pipeline.hpp>

struct NotAPipeline {};

// Should fail with a public ValidPipeline constraint diagnostic for helper misuse.
static_assert(pb::stage_name<NotAPipeline, 0>() == "bad");

int main() { return 0; }
