#include <pb/pipeline.hpp>

// Boundary regression: pipeline_stage_output_t variant of the same
// well-beyond-the-boundary axis.

struct Raw {};
struct Mid {};
struct Done {};

struct ParseA {
  using input_type  = Raw;
  using output_type = Mid;
  Mid operator()(Raw) const { return {}; }
};

struct ParseB {
  using input_type  = Mid;
  using output_type = Done;
  Done operator()(Mid) const { return {}; }
};

using Pipeline = pb::from<Raw>::then<ParseA>::then<ParseB>::to<Done>;

// stage_count is 2; 50 is far beyond.  Same named static_assert from
// pipeline_stage_output_t MUST fire.
using Bad = pb::pipeline_stage_output_t<Pipeline, 50>;

int main() { return 0; }
