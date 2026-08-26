#include <pb/pipeline.hpp>

struct NotAPipeline {};

// Should fail with a public ValidPipeline constraint diagnostic for stage descriptor alias misuse.
using BadStageDescriptor = pb::pipeline_stage_descriptor_t<NotAPipeline, 0>;

int main() { return 0; }
