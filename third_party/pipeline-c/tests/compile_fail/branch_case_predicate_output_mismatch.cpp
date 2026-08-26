#include <pb/pipeline.hpp>

struct Raw {};
struct Parsed {};

struct NonBoolPredicate {
  using input_type = Raw;
  using output_type = Parsed;
};

struct Parse {
  using input_type = Raw;
  using output_type = Parsed;
};

// Phase 5 branch case markers require bool-like predicate outputs.
using BadCase = pb::case_<NonBoolPredicate>::then<Parse>;
static_assert(sizeof(BadCase) > 0);

int main() { return 0; }
