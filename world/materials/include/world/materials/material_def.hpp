#pragma once

#include <concepts>
#include <cstdint>

#include "world/materials/terrain_query.hpp"

namespace world::materials {

struct Color {
    float r;
    float g;
    float b;
};

// What a voxel of this material physically is. Solid: a floor and a wall (collision, mesh occupancy).
// Liquid: buoyancy, mesh occupancy (water needs a surface against air), never a floor. Gas: the empty
// voxel. Foliage: present in voxel data (the sparse-brick path voxelizes canopies) but neither a
// floor nor a fluid -- a body walks through it and the mesh path never stores it as a voxel.
enum class Phase : std::uint8_t { Gas, Solid, Liquid, Foliage };

// WHICH shading path a renderer applies; the renderers own HOW each one looks (the mesh path's
// depth-tinted water and the svo path's noise-rippled water were each tuned by viewed captures and
// deliberately differ). The numeric values are exported to every shader as MAT_SHADING_* macros
// (render/diligent/detail/material_macros.hpp), so a shader tests the model, never a material id.
enum class Shading : std::uint8_t { Lit = 0, Water = 1, Foliage = 2 };

[[nodiscard]] constexpr const char* shading_macro_name(Shading shading) noexcept {
    switch (shading) {
    case Shading::Lit:
        return "MAT_SHADING_LIT";
    case Shading::Water:
        return "MAT_SHADING_WATER";
    case Shading::Foliage:
        return "MAT_SHADING_FOLIAGE";
    }
    return "MAT_SHADING_UNKNOWN";
}
inline constexpr Shading kAllShadings[] = {Shading::Lit, Shading::Water, Shading::Foliage};

// Swimming (goal 79) in a liquid of this material: feet below the surface get upthrust proportional
// to submersion (up to one voxel), drag damps the bob, and the equilibrium floats the feet this far
// under the surface. All zero for anything that is not a liquid.
struct LiquidPhysics {
    float buoyancy_acceleration = 0.0f; // world units / s^2 at >= 1 voxel submersion
    float drag = 0.0f;                  // exponential vertical damping while submerged
    float swim_equilibrium_depth = 0.0f;
};

// The record every consumer reads: a plain aggregate, so the registry can hold it in a constexpr
// table and walk it at compile time (the shader macros, the palette uploads, the tests).
struct MaterialDef {
    const char* name;
    Color albedo; // linear-space; the renderers' palettes and the `material` debug view
    Phase phase;
    Shading shading;
    LiquidPhysics liquid;
    bool yields_to_trees;   // a tree's voxel may replace this one (air, water)
    bool overrides_terrain; // this material replaces even solid terrain (the trunk, sunk on purpose)

    [[nodiscard]] constexpr bool is_solid() const noexcept { return phase == Phase::Solid; }
    [[nodiscard]] constexpr bool is_liquid() const noexcept { return phase == Phase::Liquid; }
    // "Needs a mesh boundary against open air": solid or liquid -- the mesh extractor's occupancy
    // question, distinct from is_solid() (gameplay collision), where water must NOT count.
    [[nodiscard]] constexpr bool is_occupied() const noexcept { return is_solid() || is_liquid(); }
};

// The component contract. One struct per material, one file per struct (defs/), every member
// static constexpr, nothing else: a material definition has no behavior of its own beyond saying
// where the terrain fill places it (`fills`). Anything a consumer needs that is not here is a new
// member here and in every def -- never a new `== MaterialID::X` comparison at the consumer.
template <typename T>
concept MaterialDefinition = requires(const TerrainQuery& q) {
    { T::name } -> std::convertible_to<const char*>;
    { T::albedo } -> std::convertible_to<Color>;
    { T::phase } -> std::convertible_to<Phase>;
    { T::shading } -> std::convertible_to<Shading>;
    { T::liquid } -> std::convertible_to<LiquidPhysics>;
    { T::yields_to_trees } -> std::convertible_to<bool>;
    { T::overrides_terrain } -> std::convertible_to<bool>;
    { T::fills(q) } -> std::same_as<bool>;
};

} // namespace world::materials
