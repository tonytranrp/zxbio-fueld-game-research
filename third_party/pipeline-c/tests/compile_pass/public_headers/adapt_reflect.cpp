#include <pb/adapt/reflect.hpp>

#include <type_traits>

namespace {
struct HeaderReflectShape {
  using input_type = int;
  using output_type = int;

  int operator()(int value) const { return value; }
};

static_assert(pb::reflect_stage_available_v<HeaderReflectShape> == (PB_HAS_REFLECTION != 0));

#if !PB_HAS_REFLECTION
static_assert(!pb::reflect_stage_available_v<HeaderReflectShape>);
// The fallback contract permits spelling the adapter name without forcing the
// diagnostic; code can keep declarations around gated implementation branches.
using ReflectStageName = pb::reflect_stage<HeaderReflectShape>;
static_assert(!std::is_void_v<ReflectStageName>);
#endif
} // namespace

int pb_public_header_adapt_reflect() { return 0; }
