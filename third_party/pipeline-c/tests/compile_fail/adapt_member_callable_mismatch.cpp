#include <pb/pipeline.hpp>

struct Counter {
  int bump(int) const { return 1; }
};

// Wrong input type for wrapped member; should fail the adapted_stage invocability check.
using BrokenAdaptedMember = pb::adapt_member<&Counter::bump, std::string, int>;

static_assert(pb::adapted_stage<BrokenAdaptedMember>,
              "Adapted stage callable mismatch: declared input_type must be invocable by wrapped callable");
