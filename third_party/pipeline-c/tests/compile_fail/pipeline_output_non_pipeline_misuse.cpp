#include <pb/pipeline.hpp>

struct NotAPipeline {};

// Should fail with a public ValidPipeline constraint diagnostic for output alias misuse.
using BadOutput = pb::pipeline_output_t<NotAPipeline>;

int main() { return 0; }
