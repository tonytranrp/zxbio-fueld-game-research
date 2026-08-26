#include <pb/pipeline.hpp>

struct NotAStage {};

// Should fail through the public stage_descriptor helper when the stage is invalid.
using BadDescriptor = pb::stage_descriptor<0, NotAStage>;
using BadInput = BadDescriptor::input_type;

int main() { return 0; }
