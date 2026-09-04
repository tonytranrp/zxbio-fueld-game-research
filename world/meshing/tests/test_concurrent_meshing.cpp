#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "world/chunk/chunk_coord.hpp"
#include "world/chunk/chunk_store.hpp"
#include "world/chunk/chunk_voxels.hpp"
#include "world/meshing/mesh_extractor.hpp"

using namespace world::chunk;
using namespace world::meshing;

// The chunk-streaming system meshes on worker threads, each job against its own private
// snapshot ChunkStore (built by copy-assigning ChunkVoxels). This stresses exactly that pattern
// -- concurrent extract_mesh over independently-owned snapshot stores, including building and
// destroying the snapshots on different threads -- and checks the output stays identical to a
// single-threaded reference. Kept as a regression test for the streaming threading model.
TEST_CASE("Concurrent extract_mesh over per-thread snapshot stores is safe and deterministic",
          "[meshing][stress]") {
    // Source store: a small solid blob crossing a chunk boundary so meshes are non-trivial.
    ChunkStore source;
    const ChunkCoord center{0, 0, 0};
    for (std::int32_t dz = -1; dz <= 1; ++dz) {
        for (std::int32_t dy = -1; dy <= 1; ++dy) {
            for (std::int32_t dx = -1; dx <= 1; ++dx) {
                source.get_or_create(ChunkCoord{dx, dy, dz});
            }
        }
    }
    Chunk& middle = source.find(center) != nullptr ? *source.find(center) : source.get_or_create(center);
    for (std::int32_t z = 10; z < 22; ++z) {
        for (std::int32_t y = 10; y < 22; ++y) {
            for (std::int32_t x = 24; x < 32; ++x) { // touches the +X face -> boundary-layer cells matter
                middle.voxels().set(local_index(x, y, z), MaterialID::Stone);
            }
        }
    }

    const MeshData reference = extract_mesh(source, center);
    REQUIRE_FALSE(reference.vertices.empty());

    const auto make_snapshot = [&source]() {
        auto snapshot = std::make_shared<ChunkStore>();
        for (std::int32_t dz = -1; dz <= 1; ++dz) {
            for (std::int32_t dy = -1; dy <= 1; ++dy) {
                for (std::int32_t dx = -1; dx <= 1; ++dx) {
                    const ChunkCoord coord{dx, dy, dz};
                    snapshot->get_or_create(coord).voxels() = source.find(coord)->voxels();
                }
            }
        }
        return snapshot;
    };

    constexpr int kThreads = 16;
    constexpr int kRounds = 24;
    std::atomic<int> mismatches{0};
    {
        std::vector<std::jthread> workers;
        workers.reserve(kThreads);
        for (int t = 0; t < kThreads; ++t) {
            // Half the threads receive a main-thread-built snapshot (the streaming system's exact
            // shape: built on one thread, consumed and destroyed on another); the rest build their
            // own to also stress concurrent snapshot construction.
            auto premade = (t % 2 == 0) ? make_snapshot() : nullptr;
            workers.emplace_back([&, premade] {
                for (int round = 0; round < kRounds; ++round) {
                    auto snapshot = premade != nullptr && round == 0 ? premade : make_snapshot();
                    const MeshData mesh = extract_mesh(*snapshot, center);
                    if (mesh.vertices.size() != reference.vertices.size() ||
                        mesh.indices != reference.indices) {
                        mismatches.fetch_add(1);
                    }
                }
            });
        }
    }

    CHECK(mismatches.load() == 0);
}
