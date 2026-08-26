#include <pb/pipeline.hpp>

struct Raw {};
struct Parsed {};

struct MissingInput {
  using output_type = Parsed;
};

using Broken = pb::from<Raw>::then<MissingInput>::to<Parsed>;
static_assert(pb::valid<Broken>);
