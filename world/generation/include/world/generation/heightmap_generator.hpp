#pragma once

#include <cstdint>
#include <memory>

namespace world::generation {

struct HeightmapMinMax {
    float min;
    float max;
};

// Wraps the FastNoise2 node tree (M1_2_BRIEF.md §2) -- the only place FastNoise2 headers are
// included (PROJECT_BRIEF.md §8: "FastNoise2 usage lives here, nowhere else"). PIMPL keeps
// FastNoise2's own types from leaking into this header, the same compile-firewall pattern
// render/diligent uses for DiligentCore.
class HeightmapGenerator {
public:
    explicit HeightmapGenerator(int seed);
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
