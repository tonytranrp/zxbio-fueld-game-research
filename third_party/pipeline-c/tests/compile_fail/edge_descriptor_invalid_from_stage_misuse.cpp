#include <pb/pipeline.hpp>

struct Parsed {};
struct Done {};
struct Finish {
  using input_type = Parsed;
  using output_type = Done;
};
struct NotAStage {};

// Should fail through the public edge_descriptor helper when the source stage is invalid.
using BadDescriptor = pb::edge_descriptor<0, NotAStage, Finish>;
using BadOutput = BadDescriptor::from_output_type;

int main() { return 0; }
