#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "engine/core/math.hpp"

namespace render::diligent {

// Prompt 007 goal 339 (= docs/goals.md goal 194): the instanced raster grass overlay.
//
// This header exists so the APP can build the instance list -- the placement rule, the biome and
// the ground height all live in `world/generation`, and the renderer has no business knowing any of
// them. The renderer's job is to take an array of blades and draw it.
//
// PER-BLADE DATA IS 32 BYTES, two float4s, and that number is the one the grass research invites a
// comparison on (§4): Ghost of Tsushima 16 floats / 64 B for ~83,000 blades in 2.5 ms;
// Project-GrassFlow 32 B; Acerola 7 floats / 28 B. Half of GoT's, exactly GrassFlow's. The
// difference is that nothing here is stored that can be derived -- the blade's shape comes from the
// vertex id, its wind from the shared field by world position, its identity from the same hash the
// voxel tier already computed.
struct GrassBladeInstance {
    glm::vec4 base_height{0.0f};      ///< xyz = the blade's root in world space, w = its height (m)
    glm::vec4 lean_width_phase{0.0f}; ///< xy = lean, z = width (m), w = wind phase offset (s)
};
static_assert(sizeof(GrassBladeInstance) == 32, "the shader reads two float4s per blade");

struct GrassOverlaySettings {
    bool enabled = false;
    /// Vertical segments per blade. 3 is 18 vertices and 6 triangles -- between the research's
    /// low-LOD (7 verts) and high-LOD (15 verts) Ghost of Tsushima figures.
    int segments = 3;
    float width_scale = 1.0f;
    /// How far the wind bends a blade, as a fraction of its height at full strength.
    float bend_gain = 0.35f;
    /// Radius, metres, within which the player's body pushes blades aside. 0 disables it.
    float player_bend_radius = 0.6f;
    /// Occlusion at a blade's root, 0 = black. The single largest thing that makes raster grass
    /// read as grass rather than as green needles.
    float root_occlusion = 0.45f;
};

} // namespace render::diligent
