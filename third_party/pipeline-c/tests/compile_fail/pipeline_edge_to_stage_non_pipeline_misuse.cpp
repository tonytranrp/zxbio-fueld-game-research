#include <pb/pipeline.hpp>

struct NotAPipeline {};

// Should fail with a public ValidPipeline constraint diagnostic for edge stage alias misuse.
using BadToStage = pb::pipeline_edge_to_stage_t<NotAPipeline, 0>;

int main() { return 0; }
