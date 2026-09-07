#pragma once

#include <cstdint>
#include <vector>

#include "engine/core/math.hpp"
#include "world/chunk/material.hpp"
#include "world/generation/heightmap_generator.hpp"
#include "world/generation/tree_placement.hpp"
#include "world/svo/cell_grid.hpp"
#include "world/svo/brick_tree.hpp"

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

// Goal 242: how far the readout is willing to name a material.
//
// The old value was 300 m, a round number nobody derived. The criterion chosen instead, from
// `research/human-eye-and-vision-research.md` Part 1 section 8.1: **a 1 cm detail is resolvable to
// 34 m at 20/20** (1 arcmin MAR; d = s / tan(1')). One centimetre is the scale of the surface
// detail that distinguishes one material from another in this world -- a 7.8 mm voxel -- so beyond
// 34 m a 20/20 observer cannot resolve the thing the readout is naming, and naming it is a claim
// the eye cannot check.
//
// The alternatives, and why not:
//   * 54 m -- the same 1 cm detail for the 94-ppd young-observer ceiling (0.64' MAR). A real
//     number, but it describes the best measured eye rather than the nominal one, and `--aim-range`
//     exists so a capture session can use it.
//   * 619 m -- an 18 cm face as a resolvable BLOB. Wrong criterion: detecting that something is
//     there is not identifying what it is made of, which is exactly what this readout claims.
//   * 1719 m -- a 0.5 m tree trunk at 1 arcmin. Same objection, further out.
inline constexpr float kAimResolvableRange = 34.0f;

// The same arithmetic, exposed so a test can re-derive it rather than trusting the constant:
// the distance at which `detailMetres` subtends `marArcminutes`.
[[nodiscard]] constexpr float resolvable_distance(float detailMetres, float marArcminutes) noexcept {
    // tan(theta) for a small angle in arcminutes: theta_rad = arcmin * pi / (180 * 60).
    const float radians = marArcminutes * 3.14159265358979f / (180.0f * 60.0f);
    return detailMetres / radians;
}

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
                               glm::vec3 direction, float maxDistance = kAimResolvableRange,
                               const TreeLookup* trees = nullptr);

// Goal 241: the same question, asked of the OCTREE -- the structure collision walks and the GPU
// marches, via the same `world::svo::trace_ray` the shader mirrors. The analytic version above
// re-derives the world from the height function, which was correct and was also a second opinion:
// the crosshair could disagree with the body about what is there, and on the svo path it did,
// because the tree carries LOD and edits and the height function carries neither.
//
// Use this wherever a tree is resident. `query_aim` remains for the mesh path and for the tests
// that want an analytic ground truth to compare against -- which is exactly what the goal's own
// check does with it.
[[nodiscard]] AimHit query_aim_octree(const world::svo::BrickTree& tree, glm::vec3 origin,
                                      glm::vec3 direction, float maxDistance = kAimResolvableRange);

// Prompt 004 goal 256: the same query over the cell grid. The crosshair has to ask the structure
// the renderer is actually marching -- that is this function's whole reason for existing over
// `query_aim`, and a grid resident with a null tree would otherwise silently fall back to the
// height function, which is the disagreement goal 178 removed.
[[nodiscard]] AimHit query_aim_octree(const world::svo::FlatCellGrid& grid, glm::vec3 origin,
                                      glm::vec3 direction, float maxDistance = kAimResolvableRange);

[[nodiscard]] const char* material_name(world::chunk::MaterialID material) noexcept;

} // namespace app
