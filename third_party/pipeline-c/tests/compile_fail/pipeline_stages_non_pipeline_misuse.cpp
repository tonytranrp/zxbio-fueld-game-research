#include <pb/pipeline.hpp>

struct NotAPipeline {};

// Should fail with a public ValidPipeline constraint diagnostic for stages alias misuse.
using BadStages = pb::pipeline_stages_t<NotAPipeline>;

int main() { return 0; }
