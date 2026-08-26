#include <pb/pipeline.hpp>

struct NotAPipeline {};

// Should fail with a public ValidPipeline constraint diagnostic for edge metadata alias misuse.
using BadEdge = pb::pipeline_edge_descriptor_t<NotAPipeline, 0>;

int main() { return 0; }
