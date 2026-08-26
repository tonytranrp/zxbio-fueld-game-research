#include <pb/adapt/reflect.hpp>

struct ManualStageShape {
  using input_type = int;
  using output_type = int;

  int operator()(int value) const { return value + 1; }
};

static_assert(!pb::reflect_stage_available_v<ManualStageShape>);

// Merely naming pb::reflect_stage<T> is allowed on non-reflection builds, but
// actual instantiation must fail with the C++26/P2996 gate diagnostic.
using ReflectedStage = pb::reflect_stage<ManualStageShape>;
static_assert(sizeof(ReflectedStage) > 0);
