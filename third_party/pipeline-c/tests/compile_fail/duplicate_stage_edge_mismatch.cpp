#include <pb/pipeline.hpp>

struct Raw {};
struct Parsed {};
struct Receipt {};

struct Parse {
  using input_type = Raw;
  using output_type = Parsed;
};

using Broken = pb::from<Raw>::then<Parse>::then<Parse>::to<Receipt>;
static_assert(pb::valid<Broken>);

int main() { return 0; }
