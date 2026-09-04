// Group H task 12 / Group N task 42: chunk-granularity map operations at the realistic loaded-set
// size Group D's autofly runs established (~528-588 live chunks; 558 used here), on the REAL key
// type and its real access mix -- insert/find/erase at streaming's chunk granularity, NOT the
// per-sample path NeighborCache already removed.
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include <ankerl/unordered_dense.h>
#include <benchmark/benchmark.h>
#include <boost/unordered/unordered_flat_map.hpp>

#include "world/chunk/chunk_coord.hpp"

namespace {

using world::chunk::ChunkCoord;

constexpr std::int32_t kLiveChunks = 558;

// The realistic population: one radius-5 desired cube plus the trailing hysteresis band, the
// exact shape autofly holds live. Deterministic, spatially clustered.
std::vector<ChunkCoord> live_set() {
    std::vector<ChunkCoord> keys;
    keys.reserve(1024);
    for (std::int32_t x = -5; keys.size() < kLiveChunks; ++x) {
        for (std::int32_t z = -5; z <= 5 && keys.size() < kLiveChunks; ++z) {
            for (std::int32_t y = -3; y <= 2 && keys.size() < kLiveChunks; ++y) {
                keys.push_back(ChunkCoord{x, y, z});
            }
        }
    }
    return keys;
}

// The mixing hash proposed for the unordered_dense migration (validated for distribution by the
// Group H task-8 avalanche test; is_avalanching skips the library's own re-mix).
struct MixedCoordHash {
    using is_avalanching = void;
    [[nodiscard]] auto operator()(const ChunkCoord& c) const noexcept -> std::uint64_t {
        const std::uint64_t lo = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(c.x)) << 32) |
                                 static_cast<std::uint32_t>(c.y);
        return ankerl::unordered_dense::detail::wyhash::mix(
            ankerl::unordered_dense::detail::wyhash::hash(lo),
            ankerl::unordered_dense::detail::wyhash::hash(
                static_cast<std::uint64_t>(static_cast<std::uint32_t>(c.z))));
    }
};

using StdMap = std::unordered_map<ChunkCoord, std::uint64_t>;
using DenseMap = ankerl::unordered_dense::map<ChunkCoord, std::uint64_t, MixedCoordHash>;
using SegmentedMap = ankerl::unordered_dense::segmented_map<ChunkCoord, std::uint64_t, MixedCoordHash>;
// On the project's existing std::hash specialization (boost::hash<ChunkCoord> doesn't exist):
// Boost.Unordered post-mixes any hash not marked avalanching automatically, so this measures
// its documented out-of-the-box hash-hardening story over the engine's real hash.
using BoostFlatMap = boost::unordered_flat_map<ChunkCoord, std::uint64_t, std::hash<ChunkCoord>>;

template <typename Map>
void BM_map_build_teardown(benchmark::State& state) {
    const auto keys = live_set();
    for (auto _ : state) {
        Map map;
        for (const ChunkCoord& c : keys) {
            map.emplace(c, static_cast<std::uint64_t>(c.x));
        }
        for (const ChunkCoord& c : keys) {
            map.erase(c);
        }
        benchmark::DoNotOptimize(map);
    }
    state.SetItemsProcessed(state.iterations() * kLiveChunks * 2);
}

template <typename Map>
void BM_map_find_hit(benchmark::State& state) {
    const auto keys = live_set();
    Map map;
    for (const ChunkCoord& c : keys) {
        map.emplace(c, static_cast<std::uint64_t>(c.x));
    }
    for (auto _ : state) {
        std::uint64_t sum = 0;
        for (const ChunkCoord& c : keys) {
            sum += map.find(c)->second;
        }
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * kLiveChunks);
}

template <typename Map>
void BM_map_find_miss(benchmark::State& state) {
    const auto keys = live_set();
    Map map;
    for (const ChunkCoord& c : keys) {
        map.emplace(c, static_cast<std::uint64_t>(c.x));
    }
    for (auto _ : state) {
        std::size_t misses = 0;
        for (const ChunkCoord& c : keys) {
            misses += map.find(ChunkCoord{c.x + 1000, c.y, c.z}) == map.end() ? 1 : 0;
        }
        benchmark::DoNotOptimize(misses);
    }
    state.SetItemsProcessed(state.iterations() * kLiveChunks);
}

BENCHMARK(BM_map_build_teardown<StdMap>)->Name("map_build_teardown/std_unordered_map");
BENCHMARK(BM_map_build_teardown<DenseMap>)->Name("map_build_teardown/unordered_dense");
BENCHMARK(BM_map_build_teardown<SegmentedMap>)->Name("map_build_teardown/segmented");
BENCHMARK(BM_map_build_teardown<BoostFlatMap>)->Name("map_build_teardown/boost_flat");
BENCHMARK(BM_map_find_hit<StdMap>)->Name("map_find_hit/std_unordered_map");
BENCHMARK(BM_map_find_hit<DenseMap>)->Name("map_find_hit/unordered_dense");
BENCHMARK(BM_map_find_hit<SegmentedMap>)->Name("map_find_hit/segmented");
BENCHMARK(BM_map_find_hit<BoostFlatMap>)->Name("map_find_hit/boost_flat");
BENCHMARK(BM_map_find_miss<StdMap>)->Name("map_find_miss/std_unordered_map");
BENCHMARK(BM_map_find_miss<DenseMap>)->Name("map_find_miss/unordered_dense");
BENCHMARK(BM_map_find_miss<SegmentedMap>)->Name("map_find_miss/segmented");
BENCHMARK(BM_map_find_miss<BoostFlatMap>)->Name("map_find_miss/boost_flat");

} // namespace
