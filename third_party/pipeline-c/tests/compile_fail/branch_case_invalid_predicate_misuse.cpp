#include <pb/pipeline.hpp>

struct Raw {};
struct Parsed {};

struct NotAPredicate {};

struct Parse {
  using input_type = Raw;
  using output_type = Parsed;
};

// Phase 5 branch case markers require predicates to be valid stages.
using BadCase = pb::case_<NotAPredicate>::then<Parse>;
static_assert(sizeof(BadCase) > 0);

int main() { return 0; }
