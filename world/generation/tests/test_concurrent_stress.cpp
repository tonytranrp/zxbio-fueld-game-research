#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "world/chunk/chunk.hpp"
#include "world/generation/heightmap_generator.hpp"
#include "world/generation/terrain_fill.hpp"

using world::chunk::Chunk;
using world::chunk::ChunkCoord;
using world::generation::HeightmapGenerator;

// The chunk-streaming system (app/src/chunk_streaming.cpp) runs fill_terrain on a shared
// HeightmapGenerator from every worker thread at once. FastNoise2 documents generation as
// thread-safe, and M1.2's determinism test covered cross-thread but *sequential* use -- this is
// the genuinely-concurrent stress that streaming actually needs, kept as a regression test.
TEST_CASE(
    "Many threads generating through one shared HeightmapGenerator concurrently is safe and deterministic",
    "[generation][stress]") {
    const HeightmapGenerator heightmap(1337);

    // Reference fills, single-threaded.
    constexpr std::int32_t kSpan = 4;
    std::vector<Chunk> reference;
    for (std::int32_t z = -kSpan; z < kSpan; ++z) {
        for (std::int32_t x = -kSpan; x < kSpan; ++x) {
            Chunk chunk(ChunkCoord{x, 0, z});
            world::generation::fill_terrain(chunk, heightmap);
            reference.push_back(std::move(chunk));
        }
    }

    constexpr int kThreads = 16;
    constexpr int kRounds = 8;
    std::atomic<int> mismatches{0};
    {
        std::vector<std::jthread> workers;
        workers.reserve(kThreads);
        for (int t = 0; t < kThreads; ++t) {
            workers.emplace_back([&, t] {
                for (int round = 0; round < kRounds; ++round) {
                    // Each thread re-fills every reference coordinate and compares voxel-for-voxel.
                    std::size_t refIndex = 0;
                    for (std::int32_t z = -kSpan; z < kSpan; ++z) {
                        for (std::int32_t x = -kSpan; x < kSpan; ++x) {
                            Chunk chunk(ChunkCoord{x, 0, z});
                            world::generation::fill_terrain(chunk, heightmap);
                            const Chunk& ref = reference[refIndex++];
                            for (std::size_t i = 0; i < world::chunk::kVoxelsPerChunk; ++i) {
                                if (chunk.voxels().at(i) != ref.voxels().at(i)) {
                                    mismatches.fetch_add(1);
                                    break;
                                }
                            }
                        }
                    }
                    (void)t;
                }
            });
        }
    }

    CHECK(mismatches.load() == 0);
}
