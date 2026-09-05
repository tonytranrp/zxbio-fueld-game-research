#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "engine/core/math.hpp"
#include "engine/jobs/thread_pool.hpp"
#include "world/svo/brick_tree.hpp"
#include "world/svo/sampler.hpp"
#include "world/svo/terrain_sampler.hpp"

namespace world::svo {

// How the builder decides where the tree stops subdividing (research/micro-voxel-pivot-log.md §2.2:
// LOD lives IN the tree). A box is refined until its brick voxel edge is <= the target voxel edge
// at the box's distance from `lod_center`:
//     target(d) = max(finest, d * finest / lod_radius)
// i.e. full resolution within lod_radius, doubling with every doubling of distance beyond -- a
// roughly constant screen-space voxel size, which is also the Laine-Karras traversal criterion the
// marcher applies on top. Because a box's distance is its NEAREST point, every point inside is at
// least that far, so a box that stops here is never coarser than any of its points would ask for.
struct BuildParams {
    glm::vec3 lod_center{0.0f};
    float lod_radius = 4.0f;
    bool uniform_lod = false; // full resolution everywhere (tests, tiny regions)
    // Subtrees at this level become independent pool jobs. 8^5 = 32768 of them: with distance LOD
    // nearly all the work sits in the handful of subtrees around the camera, so a coarser split
    // (512 jobs at level 3, the first version) left ~4 jobs running for seconds while the rest
    // of the pool idled -- measured 12.4 s vs. the ~1 s the CPU-time total predicted.
    int parallel_split_level = 5;
};

struct BuildStats {
    std::size_t boxes_classified = 0;
    std::size_t bricks_sampled = 0; // fill_brick calls
    std::size_t bricks_kept = 0;    // survived as real brick leaves (non-homogeneous)
    std::size_t solid_leaves = 0;
    std::size_t padding_words = 0;
    double seconds = 0.0;          // wall-clock for the whole build
    double classify_seconds = 0.0; // CPU-time summed over every thread: sampler.classify()
    double fill_seconds = 0.0;     // CPU-time summed over every thread: sampler.fill_brick()
    std::array<std::size_t, kMaxLevels> sampled_per_level{};
    std::array<std::size_t, kMaxLevels> empty_per_level{}; // sampled, came back all-air
    std::array<std::size_t, kMaxLevels> solid_per_level{}; // sampled, came back homogeneous non-air
};

// Builds the tree over `geometry` from `sampler`. With a pool, the subtrees at
// params.parallel_split_level are built concurrently and merged deterministically (result bytes are
// identical with or without a pool -- tested). Throws if geometry.voxel_bits() > kMaxVoxelBits.
template <VoxelSampler S>
[[nodiscard]] BrickTree build_tree(const S& sampler, const TreeGeometry& geometry, const BuildParams& params,
                                   engine::jobs::ThreadPool* pool = nullptr, BuildStats* stats = nullptr);

// The one production sampler is instantiated in tree_builder.cpp (cpp-heavy-templates rule 13);
// test samplers instantiate from detail/tree_builder_impl.hpp themselves.
extern template BrickTree build_tree<TerrainSampler>(const TerrainSampler&, const TreeGeometry&,
                                                     const BuildParams&, engine::jobs::ThreadPool*,
                                                     BuildStats*);

} // namespace world::svo
