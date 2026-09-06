#pragma once

#include <array>
#include <cstddef>
#include <string_view>
#include <type_traits>

#include "world/materials/material_def.hpp"
#include "world/materials/terrain_query.hpp"

namespace world::materials {

template <MaterialDefinition T>
[[nodiscard]] constexpr MaterialDef make_def() noexcept {
    return MaterialDef{
        T::name, T::albedo, T::phase, T::shading, T::liquid, T::yields_to_trees, T::overrides_terrain};
}

// The compile-time composition of material components. A material's id is its position in the
// pack -- contiguous from 0 by construction -- so there is no enum order, table order, or count to
// keep in sync anywhere: materials.hpp derives the MaterialID enumerators from index_of<>(), the
// renderers size their palettes from `size`, and the shaders receive both as macros.
//
// Still a constexpr table with zero runtime dispatch, as goal 113 decided: this project's material
// set is small and compile-time-known, so the "registry" is a type list, not a runtime container.
template <MaterialDefinition... Defs>
struct RegistryOf {
    static constexpr std::size_t size = sizeof...(Defs);
    static_assert(size > 0, "a world needs at least the empty voxel");
    static_assert(size <= 256, "material ids are 8-bit everywhere (brick bytes, node headers, the vertex)");

    static constexpr std::array<MaterialDef, size> table{make_def<Defs>()...};

    static_assert(table[0].phase == Phase::Gas,
                  "id 0 is the empty voxel everywhere (occupancy bits, absent nodes, the palette default, "
                  "the shader's miss material): the first component must be the gas");

    template <typename D>
    [[nodiscard]] static constexpr std::size_t index_of() noexcept {
        static_assert((std::is_same_v<D, Defs> || ...), "not a material of this registry");
        constexpr bool matches[] = {std::is_same_v<D, Defs>...};
        for (std::size_t i = 0; i < size; ++i) {
            if (matches[i]) {
                return i;
            }
        }
        return size;
    }

    [[nodiscard]] static constexpr bool names_unique() noexcept {
        for (std::size_t i = 0; i < size; ++i) {
            for (std::size_t j = i + 1; j < size; ++j) {
                if (std::string_view{table[i].name} == std::string_view{table[j].name}) {
                    return false;
                }
            }
        }
        return true;
    }
    static_assert(names_unique(), "two materials share a display name");

    // The terrain band rule as a search over the components: the one whose `fills` claims the voxel.
    // The predicates are mutually exclusive and exhaustive (test_registry.cpp proves both over a
    // grid of queries), so the order of the pack is irrelevant here and exactly one component
    // answers; `terrain_claims` is the count the test checks.
    [[nodiscard]] static constexpr std::size_t terrain_index(const TerrainQuery& q) noexcept {
        constexpr bool (*const fills[])(const TerrainQuery&) = {&Defs::fills...};
        for (std::size_t i = 0; i < size; ++i) {
            if (fills[i](q)) {
                return i;
            }
        }
        return 0; // nothing claims it: the empty voxel
    }
    [[nodiscard]] static constexpr std::size_t terrain_claims(const TerrainQuery& q) noexcept {
        constexpr bool (*const fills[])(const TerrainQuery&) = {&Defs::fills...};
        std::size_t n = 0;
        for (std::size_t i = 0; i < size; ++i) {
            n += fills[i](q) ? 1u : 0u;
        }
        return n;
    }
};

} // namespace world::materials
