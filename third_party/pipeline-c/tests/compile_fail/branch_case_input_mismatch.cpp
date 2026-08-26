#include <pb/pipeline.hpp>

struct Raw {};
struct OtherRaw {};
struct Parsed {};

struct Predicate {
  using input_type = Raw;
  using output_type = bool;
};

struct ParseOther {
  using input_type = OtherRaw;
  using output_type = Parsed;
};

// Phase 5 branch case markers require predicate and branch target stages to
// describe the same branch source input.
using BadCase = pb::case_<Predicate>::then<ParseOther>;
static_assert(sizeof(BadCase) > 0);

int main() { return 0; }
