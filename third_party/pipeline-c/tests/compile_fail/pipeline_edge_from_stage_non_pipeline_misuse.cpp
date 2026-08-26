#include <pb/pipeline.hpp>

struct NotAPipeline {};

// Should fail with a public ValidPipeline constraint diagnostic for edge stage alias misuse.
using BadFromStage = pb::pipeline_edge_from_stage_t<NotAPipeline, 0>;

int main() { return 0; }
