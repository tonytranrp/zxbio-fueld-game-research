#pragma once

// world::chunk's view of the material set. The ids and the count come from the component registry
// in world/materials (docs/goals.md Group AC, research/materials-as-components.md): one file per
// material under world/materials/include/world/materials/defs/, composed in materials.hpp, with the
// MaterialID enumerators DERIVED from the composition order. This header exists so the chunk layer's
// own headers and every consumer that spells world::chunk::MaterialID keep compiling unchanged.
//
// Properties (albedo, phase, shading model, liquid physics, band rule) are
// world::materials::properties_of() and friends -- include "world/materials/materials.hpp" for those.
#include "world/materials/materials.hpp"

namespace world::chunk {

using world::materials::kMaterialCount;
using world::materials::MaterialID;

} // namespace world::chunk
