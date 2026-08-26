#include <pb/pipeline.hpp>

struct Raw {};
struct Parsed {};
struct Receipt {};

struct Parse {
  using input_type = Raw;
  using output_type = Parsed;
};

struct Persist {
  using input_type = Receipt;
  using output_type = Receipt;
};

using Broken = pb::from<Raw>::then<Parse>::then<Persist>::to<Receipt>;
static_assert(pb::valid<Broken>);
