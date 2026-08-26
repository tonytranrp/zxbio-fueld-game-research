#include <pb/pipeline.hpp>

// Boundary regression: the indexed-I/O OOB diagnostic must fire with the
// SAME named static_assert whether the index is OOB by 1 or by 50.  This
// covers the "well beyond the boundary" axis that the OOB-by-1 test does
// not.

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

// stage_count is 2; 50 is far beyond.  The same named static_assert from
// the OOB-by-1 test MUST fire here too.
using Bad = pb::pipeline_stage_input_t<Pipeline, 50>;

int main() { return 0; }
