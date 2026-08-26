#include <pb/pipeline.hpp>

struct NotAPipeline {};

// Should fail with a public ValidPipeline constraint diagnostic for edge helper misuse.
static_assert(pb::edge_from_name<NotAPipeline, 0>() == "bad");

int main() { return 0; }
