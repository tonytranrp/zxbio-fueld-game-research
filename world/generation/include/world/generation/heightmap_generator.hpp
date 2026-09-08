#pragma once

#include <cstdint>
#include <memory>

#include "world/generation/field/terrain_field.hpp"

namespace world::generation {

struct HeightmapMinMax {
    float min;
    float max;
};

// Wraps the FastNoise2 node tree (M1.2 brief §2) -- the only place FastNoise2 headers are
// included (project brief §8: "FastNoise2 usage lives here, nowhere else"). PIMPL keeps
// FastNoise2's own types from leaking into this header, the same compile-firewall pattern
// render/diligent uses for DiligentCore.
/// The high-frequency half of the surface, exposed so it can be SWEPT against the acceptance
/// suite rather than guessed at.
///
/// Goal 320 measured that the pass's own macro field made the surface spectrum worse -- spectral
/// beta fell from 2.27 to 0.50 and its log-log R² from 0.92 to 0.54 -- because the detail term was
/// never re-tuned to CONTINUE the macro field's spectrum after the macro field was introduced. It
/// went from four octaves carrying the whole surface to two octaves sitting on something with a
/// different slope, and nothing checked the join.
///
/// These are the knobs that join has to be tuned on, and `test_detail_spectrum.cpp` is the sweep.
struct DetailParams {
    /// Coarsest detail wavelength, metres. Must sit at or below the macro field's own resolution
    /// (16 m cells reconstruct nothing finer than ~32 m) or the two terms overlap and beat.
    float scale_m = 32.0f;
    /// How many octaves BELOW that. Each halves the wavelength, so the finest is
    /// scale_m / 2^(octaves-1).
    int octaves = 5;
    float lacunarity = 2.0f;
    /// Amplitude ratio between successive octaves. With lacunarity 2, the fractal's Hurst exponent
    /// is H = -log2(gain), and research §9.2's beta = 2H + 1 -- so gain 0.5 is H = 1 (beta 3, too
    /// smooth per octave) and gain 0.71 is H = 0.5 (beta 2, the target).
    float gain = 0.71f;
    /// Peak-to-zero amplitude of the whole detail stack, metres.
    ///
    /// SWEPT, not chosen. `test_detail_spectrum.cpp` measures beta and Hurst across the amplitude,
    /// and both move monotonically with it because the amplitude is what decides whether the macro
    /// field or the detail term owns each scale:
    ///
    ///     6 m -> beta 1.78 (R² 0.88), H 0.448      4 m -> beta 2.11 (R² 0.91), H 0.546
    ///     3 m -> beta 2.34 (R² 0.93), H 0.610      2 m -> beta 2.66 (R² 0.95), H 0.686
    ///
    /// 4 m is shipped: of the values that put BOTH inside their bands ([1.6, 2.5] and [0.46, 0.77])
    /// it lands beta closest to §9.2's stated target of 2, with Hurst mid-band. 6 m puts Hurst at
    /// 0.448, below its floor.
    ///
    /// THE SWEEP MUST BE RUN ON THE WORLD THAT SHIPS, and this parameter has now moved FOUR times
    /// because the sweep's own conditions moved:
    ///
    ///   raw field centre (a bay on the shipped seed)   1.12 / 1.43 / 1.66 / 1.98  -> chose 2 m
    ///   with a stamped land bump under the origin      1.97 / 2.29 / 2.51 / 2.82  -> chose 4 m
    ///   recentred onto land, 5.1 km field              1.23 / 1.55 / 1.78 / 2.11  -> chose 2 m
    ///   recentred, shipped 8 km field, with strata     1.78 / 2.11 / 2.34 / 2.66  -> chose 4 m
    ///
    /// Four tables for one parameter, and the pattern in them is the finding: **the detail amplitude
    /// is not independent of the macro relief underneath it, nor of the field's extent.** The
    /// five-seed spread says the same thing from a third direction. An absolute amplitude cannot
    /// track a varying macro; expressing it as a fraction of local relief is the open goal, and
    /// until it exists this constant must be re-swept whenever the macro stages change.
    float amplitude_m = 4.0f;
};

class HeightmapGenerator {
public:
    explicit HeightmapGenerator(int seed);

    // Prompt 006 Group AM-A: the same generator, reading a BAKED MACRO FIELD for its
    // low-frequency shape instead of the low octaves of a noise stack.
    //
    // The interface above is unchanged, deliberately -- `height_at` has twelve callers
    // including a per-tick collision query and the per-brick SIMD grid path, and option (c)
    // of goal 295 (region-scoped queries) would have touched all of them for no gain. What
    // changes is only what sits behind it:
    //
    //     height(x, z) = macro(x, z)   <- Catmull-Rom from the baked field
    //                  + detail(x, z)  <- analytic, high-frequency, ALWAYS PRESENT
    //
    // Both terms are always present and both are deterministic. That is what distinguishes
    // this from goal 295's option (b), whose residual is absent until a tile is baked and
    // whose world therefore has a shape that depends on where the player has been -- a
    // determinism hazard, and determinism is this pass's first rule.
    HeightmapGenerator(int seed, std::shared_ptr<const field::TerrainField> macro,
                       const DetailParams& detail = {});

    /// The gradient of the same surface `height_at` returns, in metres per metre.
    ///
    /// It exists because several callers already finite-difference `height_at` with their
    /// OWN epsilon -- TerrainSampler at 1 m, the collider at its own -- so "the slope" is
    /// three slightly different quantities depending on who asks. This is one answer, and
    /// it is the analytic derivative of the reconstruction the height came from rather than
    /// a difference of two samples of it.
    [[nodiscard]] glm::vec2 slope_at(float worldX, float worldZ) const;

    /// Null until a macro field is attached, which is what the analytic-only constructor
    /// leaves it as. Exposed so a tool can report which world it is looking at.
    [[nodiscard]] const field::TerrainField* macro_field() const noexcept;
    ~HeightmapGenerator();

    HeightmapGenerator(const HeightmapGenerator&) = delete;
    HeightmapGenerator& operator=(const HeightmapGenerator&) = delete;
    HeightmapGenerator(HeightmapGenerator&&) noexcept;
    HeightmapGenerator& operator=(HeightmapGenerator&&) noexcept;

    // Fills outHeights (row-major, X innermost -- outHeights[lz * width + lx]) with one surface
    // height sample per (worldX, worldZ) column and returns the min/max sampled (§2.4) -- callers
    // use this to short-circuit the per-voxel fill loop before it starts.
    HeightmapMinMax generate_column_heights(std::int32_t worldXOffset, std::int32_t worldZOffset,
                                            std::int32_t width, std::int32_t depth, float* outHeights) const;

    // Sub-voxel variant for the sparse-brick-octree path (micro-voxel pivot, Group W): one sample
    // per (xStart + i*step, zStart + j*step) column, same row-major layout. FastNoise2's own grid
    // API takes float offsets and step sizes directly, so a 7.8mm-spaced 8x8 grid is one SIMD call,
    // not 64 GenSingle2D calls. With integer xStart/zStart and step == 1 this is the exact same
    // call generate_column_heights makes -- the property the "sampler reproduces fill_terrain"
    // test relies on.
    HeightmapMinMax generate_column_heights_spaced(float xStart, float zStart, std::int32_t width,
                                                   std::int32_t depth, float step, float* outHeights) const;

    // Single-column surface height at an arbitrary world (x,z) -- the analytic ground query
    // behind walk mode (TERRAIN_FIXES_BRIEF Group V task 23). Same node tree and seed as
    // generate_column_heights, so it matches the generated terrain by construction and works for
    // columns whose chunks aren't even loaded. Thread-safe like the grid call (stress-tested).
    [[nodiscard]] float height_at(float worldX, float worldZ) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace world::generation
