#include <pb/pipeline.hpp>

struct NotAPipeline {};

// Should fail with a public ValidPipeline constraint diagnostic for input alias misuse.
using BadInput = pb::pipeline_input_t<NotAPipeline>;

int main() { return 0; }
