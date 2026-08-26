#include <pb/pipeline.hpp>

struct Raw {};
struct Parsed {};
struct Done {};

struct Parse {
  using input_type = Raw;
  using output_type = Parsed;
  Parsed operator()(Raw) const { return {}; }
};

struct Finish {
  using input_type = Parsed;
  using output_type = Done;
  Done operator()(Parsed) const { return {}; }
};

using Pipeline = pb::from<Raw>::then<Parse>::then<Finish>::to<Done>;

// Should fail with an explicit out-of-range pipeline stage metadata index diagnostic.
static_assert(pb::stage_key<Pipeline, 2>() == "bad");
