// Goal 258's headroom, measured before implementing: how many DISTINCT materials does a real brick
// actually hold? A per-brick palette pays only if that number is small.
#include <array>
#include <cstdio>
#include <map>

#include "world/generation/heightmap_generator.hpp"
#include "world/svo/brick.hpp"
#include "world/svo/terrain_sampler.hpp"
#include "world/svo/tree_builder.hpp"

int main() {
    const world::generation::HeightmapGenerator heightmap(1337);
    world::svo::TreeGeometry g;
    g.root_size_log2 = 6; // 64 m: big enough to be representative, small enough to build fast
    g.voxel_size_log2 = -7;
    g.origin = glm::vec3{16.0f, -24.0f, -32.0f};
    world::svo::TerrainSamplerParams sp;
    sp.seed = 1337;
    sp.trees = true;
    const world::svo::Box region{g.origin, g.max_corner()};
    world::svo::TerrainSampler sampler(heightmap, sp, region);
    world::svo::BuildParams bp;
    bp.lod_center = glm::vec3{48.0f, 66.0f, 0.0f};
    bp.lod_radius = 4.0f;
    const world::svo::BrickTree tree = world::svo::build_tree(sampler, g, bp, nullptr, nullptr);

    std::map<std::size_t, std::size_t> histogram; // distinct materials -> brick count
    const std::size_t brickCount = tree.bricks.size() / world::svo::kBrickWords;
    for (std::size_t b = 0; b < brickCount; ++b) {
        const std::uint32_t* words = tree.bricks.data() + b * world::svo::kBrickWords;
        std::array<bool, 256> seen{};
        std::size_t distinct = 0;
        for (std::size_t v = 0; v < world::svo::kBrickVoxels; ++v) {
            const auto m = static_cast<std::size_t>(world::svo::brick_word_material(words, v));
            if (!seen[m]) {
                seen[m] = true;
                ++distinct;
            }
        }
        ++histogram[distinct];
    }
    std::printf("bricks: %zu\n", brickCount);
    std::size_t cumulative = 0;
    for (const auto& [distinct, count] : histogram) {
        cumulative += count;
        std::printf("  %2zu distinct materials: %7zu bricks (%5.1f%%, cumulative %5.1f%%)\n", distinct,
                    count, 100.0 * static_cast<double>(count) / static_cast<double>(brickCount),
                    100.0 * static_cast<double>(cumulative) / static_cast<double>(brickCount));
    }
    return 0;
}
