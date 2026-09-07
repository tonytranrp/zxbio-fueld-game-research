#pragma once

#include <cstdint>
#include <vector>

#include "engine/core/math.hpp"
#include "world/chunk/material.hpp"
#include "world/generation/heightmap_generator.hpp"
#include "world/generation/tree_placement.hpp"

namespace app {

// Crosshair-raycast material report (goal 84, extended by Prompt 001 A4): what the camera is
// aiming at. Purely ANALYTIC -- marches the same height function that generates terrain and
// re-derives the surface-banding rule terrain_fill uses, so it needs no voxel access, works on any
// thread, and is exactly testable against known columns.
struct AimHit {
    bool hit = false;
    world::chunk::MaterialID material = world::chunk::MaterialID::Air;
    glm::vec3 position{0.0f}; // world-space hit point (surface, water plane, or tree)
    float distance = 0.0f;    // metres from the ray origin
};

// The trees along the ray (A4). The query used to see only the height field, so trunks and
// canopies -- the things a player most often looks at -- reported as whatever terrain was BEHIND
// them. Placements are deterministic per chunk column, so this just needs the column's list; the
// cache keeps a frame's worth of columns rather than recomputing per 0.5 m step.
//
// Not a template over a concept, unlike world/collision's SolidQuery: there is exactly one
// implementation (the generator's own placement function) and the reason to inject anything here is
// testability, which a seed already provides.
class TreeLookup {
public:
    TreeLookup(const world::generation::HeightmapGenerator& heightmap, int seed) noexcept
        : heightmap_(&heightmap), seed_(seed) {}

    // Wood/Leaves/Air at a world point, over every tree whose column could reach it. Neighbouring
    // columns are included because a canopy overhangs its own column's edge.
    [[nodiscard]] world::chunk::MaterialID material_at(const glm::vec3& p) const;

private:
    struct Column {
        std::int32_t x = 0;
        std::int32_t z = 0;
        std::vector<world::generation::TreePlacement> trees;
    };

    [[nodiscard]] const std::vector<world::generation::TreePlacement>& column(std::int32_t cx,
                                                                              std::int32_t cz) const;

    const world::generation::HeightmapGenerator* heightmap_;
    int seed_;
    mutable std::vector<Column> cache_; // small: a 300 m ray crosses ~10 columns of 32 m
};

// `trees` may be null (the mesh path's own decoration is emitted elsewhere, and the tests that
// only care about terrain pass nothing).
[[nodiscard]] AimHit query_aim(const world::generation::HeightmapGenerator& heightmap, glm::vec3 origin,
                               glm::vec3 direction, float maxDistance = 300.0f,
                               const TreeLookup* trees = nullptr);

[[nodiscard]] const char* material_name(world::chunk::MaterialID material) noexcept;

} // namespace app
