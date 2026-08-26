#include <pb/pipeline.hpp>

struct Raw {};
struct Parsed {};
struct Parse {
  using input_type = Raw;
  using output_type = Parsed;
};
struct NotAStage {};

// Should fail through the public edge_descriptor helper when the target stage is invalid.
using BadDescriptor = pb::edge_descriptor<0, Parse, NotAStage>;
using BadInput = BadDescriptor::to_input_type;

int main() { return 0; }
