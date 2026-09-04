#include <future>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "engine/jobs/thread_pool.hpp"
#include "world/chunk/chunk.hpp"
#include "world/chunk/chunk_coord.hpp"
#include "world/chunk/chunk_voxels.hpp"
#include "world/generation/heightmap_generator.hpp"
#include "world/generation/terrain_fill.hpp"

using namespace world::chunk;
using namespace world::generation;

namespace {

// Same seed + coordinate as test_terrain_fill.cpp's "straddles the generated surface" case --
// already confirmed there to produce a genuine mix of materials, not a coordinate picked blind.
constexpr int kSeed = 1337;
constexpr ChunkCoord kCoord{0, 0, 0};

// Snapshots every voxel's material as a flat vector, so two generations of the "same" chunk can
// be compared byte-for-byte rather than just eyeballing a summary statistic.
std::vector<MaterialID> snapshot(const Chunk& chunk) {
    std::vector<MaterialID> out;
    out.reserve(kVoxelsPerChunk);
    for (std::size_t i = 0; i < kVoxelsPerChunk; ++i) {
        out.push_back(chunk.voxels().at(i));
    }
    return out;
}

std::vector<MaterialID> generate_once(int seed, ChunkCoord coord) {
    HeightmapGenerator heightmap(seed);
    Chunk chunk(coord);
    fill_terrain(chunk, heightmap);
    return snapshot(chunk);
}

} // namespace

TEST_CASE("Same seed + chunk coordinate produces byte-identical output across repeated same-thread runs",
          "[generation][determinism]") {
    const std::vector<MaterialID> first = generate_once(kSeed, kCoord);
    const std::vector<MaterialID> second = generate_once(kSeed, kCoord);

    REQUIRE(first.size() == second.size());
    REQUIRE(first == second);

    // Sanity check this chunk is actually exercising a non-trivial (non-homogeneous) fill --
    // otherwise this test could pass trivially even with a real determinism bug elsewhere.
    bool sawMoreThanOneMaterial = false;
    for (std::size_t i = 1; i < first.size(); ++i) {
        if (first[i] != first[0]) {
            sawMoreThanOneMaterial = true;
            break;
        }
    }
    REQUIRE(sawMoreThanOneMaterial);
}

TEST_CASE("Same seed + chunk coordinate produces byte-identical output when generated concurrently across "
          "ThreadPool worker threads",
          "[generation][determinism]") {
    // This is simultaneously a world-gen correctness test AND a second, independent stress test
    // of M1.1's ThreadPool destructor-order fix under real concurrent load (M1.2 brief's own
    // opening note) -- many worker threads genuinely racing to generate, not just queued serially.
    engine::jobs::ThreadPool pool(8);

    constexpr int kRuns = 32;
    std::vector<std::future<std::vector<MaterialID>>> futures;
    futures.reserve(kRuns);
    for (int i = 0; i < kRuns; ++i) {
        futures.push_back(pool.submit([] { return generate_once(kSeed, kCoord); }));
    }

    std::vector<std::vector<MaterialID>> results;
    results.reserve(kRuns);
    for (auto& f : futures) {
        results.push_back(f.get());
    }

    REQUIRE(results.size() == static_cast<std::size_t>(kRuns));
    for (std::size_t i = 1; i < results.size(); ++i) {
        INFO("run " << i << " differs from run 0");
        REQUIRE(results[i] == results[0]);
    }
}
