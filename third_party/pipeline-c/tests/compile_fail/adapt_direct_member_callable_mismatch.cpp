#include <pb/pipeline.hpp>

#include <string>
#include <string_view>

struct CounterName {
  static constexpr std::string_view value{"counter.bump"};
};

struct Counter {
  int bump(int) const { return 1; }
};

// Wrong input type for direct named member adapter; should fail adapted_stage invocability.
using BrokenDirectMember =
    pb::adapt<pb::name<CounterName>, pb::member<&Counter::bump>, pb::in<std::string>, pb::out<int>>;

static_assert(pb::adapted_stage<BrokenDirectMember>,
              "Adapted stage callable mismatch: declared input_type must be invocable by wrapped callable");
