#include <pb/pipeline.hpp>

struct NotAStage {};

// Should fail with the public stage_input_t alias and explicit valid-stage/input_type diagnostics.
using BadInput = pb::stage_input_t<NotAStage>;

int main() { return 0; }
