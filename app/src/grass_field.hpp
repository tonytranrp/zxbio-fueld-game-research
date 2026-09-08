#pragma once

// The raster grass overlay's instance list (Prompt 007 goal 339 = goal 194).
//
// This is the CPU half: it walks the SAME `grass_tufts_in_patch` the voxel tier walks (goal 338),
// with the same seed and the same params, and turns each tuft into blades for the renderer. That
// shared call is not a convenience -- it is goal 340's whole requirement. A tuft has to be the same
// tuft in both tiers, at the same position, with the same identity, or the boundary between them is
// a visible ring of doubled or missing grass.
//
// The overlay's ring is SMALLER than the voxel ring, and inside it the voxel blades are still
// there. That overlap is deliberate and is what goal 340 measures: two representations of one tuft,
// agreeing.

#include <cstddef>
#include <span>
#include <vector>

#include "engine/core/math.hpp"
#include "render/diligent/grass_overlay.hpp"
#include "world/generation/field/biome.hpp"
#include "world/generation/grass_cover.hpp"
#include "world/generation/heightmap_generator.hpp"

namespace app {

struct GrassFieldOptions {
    /// Metres of raster overlay. Much smaller than the voxel ring: a blade is a handful of pixels
    /// past ~10 m, and the research's own consensus is that distance culling is where all the win
    /// is (a Vulkan grass renderer measured 1.45-3.98x from distance culling alone).
    float radius_m = 14.0f;
    /// A hard ceiling on blades, so a dense biome cannot turn one frame into a stall. The research
    /// calls 32K-131K a production range; the default here sits at the bottom of it.
    std::size_t max_blades = 40000;
    /// The overlay's own density, relative to the voxel tier's. 1.0 draws one raster blade per
    /// voxel blade -- the setting goal 340's agreement test uses.
    float density_scale = 1.0f;
    bool enabled = false;
};

class GrassField {
public:
    GrassField(const world::generation::HeightmapGenerator& heightmap, int seed,
               const world::generation::GrassCoverParams& cover, const GrassFieldOptions& options)
        : heightmap_(&heightmap), seed_(seed), cover_(cover), options_(options) {}

    /// Rebuild the instance list around `camera`. Hysteresis at a quarter of the radius, for the
    /// same reason the sway forest has it: this walks thousands of tufts and the answer barely
    /// changes when the player takes a step.
    void refresh(glm::vec3 camera);

    [[nodiscard]] std::span<const render::diligent::GrassBladeInstance> blades() const noexcept {
        return blades_;
    }
    [[nodiscard]] std::size_t tuft_count() const noexcept { return tufts_; }
    [[nodiscard]] double build_seconds() const noexcept { return buildSeconds_; }
    [[nodiscard]] bool truncated() const noexcept { return truncated_; }

private:
    const world::generation::HeightmapGenerator* heightmap_;
    int seed_;
    world::generation::GrassCoverParams cover_;
    GrassFieldOptions options_;

    std::vector<render::diligent::GrassBladeInstance> blades_;
    std::size_t tufts_ = 0;
    glm::vec3 builtAt_{0.0f};
    bool built_ = false;
    bool truncated_ = false;
    double buildSeconds_ = 0.0;
};

} // namespace app
