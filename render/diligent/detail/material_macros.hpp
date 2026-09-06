#pragma once

#include <array>
#include <cstddef>

#include "world/materials/materials.hpp"

#include "Graphics/GraphicsTools/interface/ShaderMacroHelper.hpp"

namespace render::diligent::detail {

// The material registry's shader-facing half (docs/goals.md Group AC). Every shader that reads a
// material record is compiled with MATERIAL_COUNT and one MAT_SHADING_* macro per shading model, so
// the palette array size and the "is this water / foliage" tests come from world/materials -- the
// same source the C++ palette upload reads -- instead of literals (`[8]`, `min(m, 7u)`, `== 3u`,
// `== 5u`) that had to be kept in sync by comment. A shader that names a macro this does not define
// fails to compile, which is the point.
inline void add_material_macros(Diligent::ShaderMacroHelper& macros) {
    macros.AddShaderMacro("MATERIAL_COUNT", static_cast<Diligent::Uint32>(world::materials::kMaterialCount));
    for (const world::materials::Shading shading : world::materials::kAllShadings) {
        macros.AddShaderMacro(world::materials::shading_macro_name(shading),
                              static_cast<Diligent::Uint32>(shading));
    }
}

// One float4 per material -- the record layout BOTH palettes upload (the terrain PSO's
// MaterialPalette cbuffer and the svo march's MarchConstants tail): rgb = linear albedo, w = the
// shading model as an exact small integer, read back in HLSL as uint(w + 0.5).
using MaterialRecord = std::array<float, 4>;

[[nodiscard]] constexpr MaterialRecord material_record(const world::materials::MaterialDef& m) noexcept {
    return {m.albedo.r, m.albedo.g, m.albedo.b, static_cast<float>(static_cast<int>(m.shading))};
}

[[nodiscard]] constexpr std::array<MaterialRecord, world::materials::kMaterialCount>
make_material_records() noexcept {
    std::array<MaterialRecord, world::materials::kMaterialCount> out{};
    for (std::size_t i = 0; i < world::materials::kMaterialCount; ++i) {
        out[i] = material_record(world::materials::Registry::table[i]);
    }
    return out;
}

inline constexpr std::array<MaterialRecord, world::materials::kMaterialCount> kMaterialRecords =
    make_material_records();

} // namespace render::diligent::detail
