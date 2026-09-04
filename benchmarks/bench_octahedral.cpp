// Group N task 43 / Group K task 28's CPU half: encode/decode throughput of the 16-bit octahedral
// normal path, at the granularity the streaming upload path pays it (one encode per vertex).
#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include <benchmark/benchmark.h>

#include "world/meshing/octahedral.hpp"

namespace {

std::vector<glm::vec3> sample_normals(int count) {
    std::vector<glm::vec3> normals;
    normals.reserve(static_cast<std::size_t>(count));
    const float golden = std::numbers::phi_v<float>;
    for (int i = 0; i < count; ++i) {
        const float z = 1.0f - (2.0f * static_cast<float>(i) + 1.0f) / static_cast<float>(count);
        const float r = std::sqrt(std::max(0.0f, 1.0f - z * z));
        const float phi = 2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / golden;
        normals.emplace_back(r * std::cos(phi), r * std::sin(phi), z);
    }
    return normals;
}

void BM_octahedral_encode(benchmark::State& state) {
    const auto normals = sample_normals(4096);
    for (auto _ : state) {
        std::uint32_t sum = 0;
        for (const glm::vec3& n : normals) {
            const auto packed = world::meshing::encode_octahedral_16(n);
            sum += packed.u + packed.v;
        }
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * 4096);
}

void BM_octahedral_decode(benchmark::State& state) {
    const auto normals = sample_normals(4096);
    std::vector<world::meshing::OctNormal16> packed;
    packed.reserve(normals.size());
    for (const glm::vec3& n : normals) {
        packed.push_back(world::meshing::encode_octahedral_16(n));
    }
    for (auto _ : state) {
        float sum = 0.0f;
        for (const auto p : packed) {
            sum += world::meshing::decode_octahedral_16(p).z;
        }
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * 4096);
}

BENCHMARK(BM_octahedral_encode);
BENCHMARK(BM_octahedral_decode);

} // namespace
