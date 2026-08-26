#include <pb/pipeline.hpp>

struct NotAPipeline {};

// Should fail with a public ValidPipeline constraint diagnostic for metadata alias misuse.
using BadStage = pb::pipeline_stage_t<NotAPipeline, 0>;

int main() { return 0; }
