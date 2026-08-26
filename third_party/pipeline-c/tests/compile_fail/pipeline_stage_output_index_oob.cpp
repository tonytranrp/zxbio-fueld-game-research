#include <pb/pipeline.hpp>

struct Raw {};
struct Parsed {};

struct Parse {
  using input_type  = Raw;
  using output_type = Parsed;
  Parsed operator()(Raw) const { return {}; }
};

using Pipeline = pb::from<Raw>::then<Parse>::to<Parsed>;

// Index 1 is out of range for a single-stage pipeline (stage_count == 1).
// Expected: named static_assert naming "pipeline_stage_output_t" and
//           "pipeline_size_v<Pipeline>".
using Bad = pb::pipeline_stage_output_t<Pipeline, 1>;

int main() { return 0; }
