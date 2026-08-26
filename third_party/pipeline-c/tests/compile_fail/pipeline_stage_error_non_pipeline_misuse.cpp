#include <pb/pipeline.hpp>

struct NotAPipeline {};

// Should fail with a public ValidPipeline constraint diagnostic for stage error alias misuse.
using BadStageError = pb::pipeline_stage_error_t<NotAPipeline, 0>;

int main() { return 0; }
