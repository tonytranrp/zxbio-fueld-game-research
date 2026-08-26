#include <pb/pipeline.hpp>

struct NotAPipeline {};

// Should fail with a public ValidPipeline constraint diagnostic for edge helper misuse.
static_assert(pb::edge_to_key<NotAPipeline, 0>() == "bad");

int main() { return 0; }
