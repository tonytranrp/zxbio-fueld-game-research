#pragma once

#include <array>

#include <cstddef>
#include <cstdint>

#include "world/materials/defs/air.hpp"
#include "world/materials/defs/dirt.hpp"
#include "world/materials/defs/grass.hpp"
#include "world/materials/defs/grass_blade.hpp"
#include "world/materials/defs/leaves.hpp"
#include "world/materials/defs/sand.hpp"
#include "world/materials/defs/stone.hpp"
#include "world/materials/defs/water.hpp"
#include "world/materials/defs/wood.hpp"
#include "world/materials/registry.hpp"

namespace world::materials {

// THE composition (docs/goals.md Group AC). Adding a material is one new file under defs/, one
// entry here, and one enumerator below -- the static_assert after the enum catches a missing
// enumerator. Nothing else in the codebase names the count or the order: the renderers size their
// palettes from kMaterialCount, the shaders receive MATERIAL_COUNT and the shading models as
// macros, the terrain fill asks terrain_material(), and the id of each material is its position.
//
// The pack order is the id order. Air must stay first (RegistryOf asserts it); the rest is the
// order the world has always used, kept so every baked id in a saved capture or a test stays valid.
using Registry = RegistryOf<defs::Air, defs::Stone, defs::Dirt, defs::Water, defs::Wood, defs::Leaves,
                            defs::Sand, defs::Grass, defs::GrassBlade>;

namespace detail {
template <typename Def>
[[nodiscard]] constexpr std::uint8_t id_of() noexcept {
    return static_cast<std::uint8_t>(Registry::index_of<Def>());
}
} // namespace detail

// Derived from the composition, not declared beside it: an enumerator's value IS its component's
// registry index, so the enum cannot drift from the table.
enum class MaterialID : std::uint8_t {
    Air = detail::id_of<defs::Air>(),
    Stone = detail::id_of<defs::Stone>(),
    Dirt = detail::id_of<defs::Dirt>(),
    Water = detail::id_of<defs::Water>(),
    Wood = detail::id_of<defs::Wood>(),
    Leaves = detail::id_of<defs::Leaves>(),
    Sand = detail::id_of<defs::Sand>(),
    Grass = detail::id_of<defs::Grass>(),
    // Prompt 007 goal 338. The NINTH material, and the one that made the brick palette's global
    // "at most eight" guarantee into a per-brick one -- world/svo/brick.hpp records what that cost.
    GrassBlade = detail::id_of<defs::GrassBlade>(),
};

inline constexpr std::size_t kMaterialCount = Registry::size;
static_assert(static_cast<std::size_t>(MaterialID::GrassBlade) + 1 == kMaterialCount,
              "a material was registered above without an enumerator here");

namespace detail {
// A def may declare `using palette_fallback = SomeOtherDef;`. Most do not, and a material with no
// declared fallback is its own -- which is the identity case, so the brick code needs no special
// path for it.
template <typename Def>
concept HasPaletteFallback = requires { typename Def::palette_fallback; };

template <typename Def>
[[nodiscard]] constexpr std::uint8_t fallback_id_of() noexcept {
    if constexpr (HasPaletteFallback<Def>) {
        return id_of<typename Def::palette_fallback>();
    } else {
        return id_of<Def>();
    }
}

template <typename... Defs>
[[nodiscard]] constexpr std::array<std::uint8_t, sizeof...(Defs)> fallback_table(RegistryOf<Defs...>*) noexcept {
    return {fallback_id_of<Defs>()...};
}
inline constexpr auto kFallbacks = fallback_table(static_cast<Registry*>(nullptr));
} // namespace detail

/// The material to store when a brick's eight-entry palette cannot name this one
/// (world/svo/brick.hpp's overflow policy). Almost always the material itself.
[[nodiscard]] constexpr MaterialID palette_fallback_of(MaterialID id) noexcept {
    return static_cast<MaterialID>(detail::kFallbacks[static_cast<std::size_t>(id)]);
}

[[nodiscard]] constexpr const MaterialDef& properties_of(MaterialID id) noexcept {
    return Registry::table[static_cast<std::size_t>(id)];
}

[[nodiscard]] constexpr const char* name_of(MaterialID id) noexcept {
    return properties_of(id).name;
}

// "Needs a mesh boundary against open air" -- the mesh extractor's question (solid or liquid). Not
// gameplay collision: that is properties_of(id).is_solid(), where Water must not count.
[[nodiscard]] constexpr bool is_occupied(MaterialID id) noexcept {
    return properties_of(id).is_occupied();
}

// The terrain band rule: which material a voxel of a terrain column is. One function for the chunk
// fill (voxel_edge 1), the sparse-brick sampler (sub-meter edges), and the aim readout (the surface
// voxel) -- the three copies of the rule and its constants that Group AC replaced.
[[nodiscard]] constexpr MaterialID terrain_material(const TerrainQuery& q) noexcept {
    return static_cast<MaterialID>(Registry::terrain_index(q));
}

// Voxelizing a tree over already-filled terrain: the tree's voxel lands where the current material
// yields (air, water) or the tree's material overrides (the sunk trunk). Terrain otherwise wins --
// no carving grass out of a hillside a canopy leans into.
[[nodiscard]] constexpr bool tree_replaces(MaterialID current, MaterialID tree) noexcept {
    return properties_of(current).yields_to_trees || properties_of(tree).overrides_terrain;
}

} // namespace world::materials
