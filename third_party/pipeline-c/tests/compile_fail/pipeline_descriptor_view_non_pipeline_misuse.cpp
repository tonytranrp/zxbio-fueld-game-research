#include <pb/pipeline.hpp>

struct NotAPipeline {};

// Should fail with a public ValidPipeline constraint diagnostic for descriptor view misuse.
static_assert(pb::descriptor_view<NotAPipeline>().edge_records().empty());

int main() { return 0; }
