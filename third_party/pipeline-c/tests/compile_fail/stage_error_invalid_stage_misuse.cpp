#include <pb/pipeline.hpp>

struct NotAStage {};

// Should fail with the public stage_error_t alias and explicit valid-stage diagnostics.
using BadError = pb::stage_error_t<NotAStage>;

int main() { return 0; }
