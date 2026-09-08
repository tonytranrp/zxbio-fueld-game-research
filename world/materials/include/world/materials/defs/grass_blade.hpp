#pragma once

#include "world/materials/defs/grass.hpp"
#include "world/materials/material_def.hpp"

namespace world::materials::defs {

// Prompt 007 goal 338 (= docs/goals.md goal 193): ground cover as VOXELS, in the finest LOD ring.
//
// `research/grass-rendering-research.md` ranks this #2 for this engine, behind an instanced raster
// overlay, and names its two costs honestly: vegetation is the worst-case SVO content class
// (Laine & Karras), and there is no published way to animate stored ray-marched voxels. Both are
// accepted here. The blades do not move; goal 333's shimmer is what makes them read as alive, and
// goal 339's overlay is the tier that can actually bend.
//
// WHY IT IS A MATERIAL OF ITS OWN RATHER THAN `Grass`. Ground grass is `Phase::Solid` -- it is the
// surface you stand on. A blade is `Phase::Foliage`: the body walks through it, it does not stop a
// footstep, and the aim readout should name it without the collider ever seeing it. One material
// cannot be both, which is the whole reason this file exists.
//
// It is a NINTH material, and the brick palette used to hold exactly eight. See `world/svo/brick.hpp`
// for what that cost and why the answer was a per-brick invariant rather than a wider index.
struct GrassBlade {
    static constexpr const char* name = "GrassBlade";
    // A shade lighter and yellower than the ground it stands on, so a tuft is legible against it.
    // Ground Grass is {0.23, 0.48, 0.13}.
    static constexpr Color albedo{0.31f, 0.56f, 0.16f};
    static constexpr Phase phase = Phase::Foliage;
    static constexpr Stipple stipple{0.0f};
    // Shading::Foliage, so a blade gets the canopy's own shading and -- deliberately -- goal 337's
    // domain warp if it is ever turned on. A blade is exactly the thing that warp was for.
    static constexpr Shading shading = Shading::Foliage;
    static constexpr LiquidPhysics liquid{};
    static constexpr bool wind_responsive = true;
    static constexpr bool yields_to_trees = true; // a trunk through a tuft wins
    static constexpr bool overrides_terrain = false;

    // The palette fallback (world/svo/brick.hpp): if a brick ever holds more distinct materials
    // than its eight-entry palette can name, a blade becomes the ground grass it stands in rather
    // than becoming Air. Never observed on any measured build -- the counter that would catch it is
    // asserted at zero -- but stated, because "never observed" is not "cannot happen".
    using palette_fallback = Grass;

    // Blades are placed by the voxelizer, not by the column rule: `fills` is what decides the
    // banded terrain skin, and a tuft is a scattered object like a tree.
    [[nodiscard]] static constexpr bool fills(const TerrainQuery&) noexcept { return false; }
};

} // namespace world::materials::defs
