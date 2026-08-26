#include <pb/pipeline.hpp>

struct Raw {};

struct MissingOutput {
  using input_type = Raw;
};

using Broken = pb::from<Raw>::then<MissingOutput>::to<Raw>;
static_assert(pb::valid<Broken>);
