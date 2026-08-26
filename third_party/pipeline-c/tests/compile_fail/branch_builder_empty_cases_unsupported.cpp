#include <pb/pipeline.hpp>

struct Raw {};

struct IsParsed {
  using input_type = Raw;
  using output_type = bool;
};

struct Parse {
  using input_type = Raw;
  using output_type = int;
};

// Empty branch builders should fail early at the marker layer and must remain
// unsupported by the linear runtime boundary.
using UnsupportedEmptyBranch = pb::from<Raw>::branch<>;

int main() { return 0; }
