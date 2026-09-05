#pragma once

#include "engine/core/math.hpp"
#include "world/chunk/material.hpp"
#include "world/generation/heightmap_generator.hpp"

namespace app {

// Crosshair-raycast material report (goal 84): what terrain material is under the camera's aim.
// Purely ANALYTIC -- marches the same height function that generates terrain and re-derives the
// surface-banding rule terrain_fill uses, so it needs no voxel access, works on any thread, and
// is exactly testable against known columns. Honest scope: reports terrain and water, not trees
// (decoration geometry isn't part of the height field).
struct AimHit {
    bool hit = false;
    world::chunk::MaterialID material = world::chunk::MaterialID::Air;
    glm::vec3 position{0.0f}; // world-space hit point (surface or water plane)
};

[[nodiscard]] AimHit query_aim(const world::generation::HeightmapGenerator& heightmap, glm::vec3 origin,
                               glm::vec3 direction, float maxDistance = 300.0f);

[[nodiscard]] const char* material_name(world::chunk::MaterialID material) noexcept;

} // namespace app
