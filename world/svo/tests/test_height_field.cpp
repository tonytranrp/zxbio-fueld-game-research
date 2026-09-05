#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "world/generation/heightmap_generator.hpp"
#include "world/svo/height_field.hpp"

using namespace world::svo;

// The soundness contract behind every Air/Solid box classification: no column inside a queried
// footprint may ever fall outside the returned range. Checked against the REAL generator by dense
// re-sampling, because the margin is an empirical bound on this specific noise, not a theorem.
TEST_CASE("height field ranges contain every densely re-sampled column, margin included",
          "[svo][heightfield]") {
    const world::generation::HeightmapGenerator heightmap(1337);
    const float xMin = -64.0f;
    const float zMin = -64.0f;
    const float extent = 128.0f;
    const HeightField field(heightmap, xMin, zMin, extent, 0.5f);
    std::printf("height field: %d cells, margin %.3f m\n", field.cell_count(),
                static_cast<double>(field.margin()));
    REQUIRE(field.cell_count() == 256);
    REQUIRE(field.margin() > 0.0f);

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> pos(0.0f, extent);
    std::uniform_real_distribution<float> sizeLog(-4.5f, 3.0f); // 0.044 m .. 8 m footprints
    std::size_t violations = 0;
    float worstOvershoot = 0.0f;
    constexpr int kSamples = 7;
    std::vector<float> h(static_cast<std::size_t>(kSamples) * static_cast<std::size_t>(kSamples));
    for (int trial = 0; trial < 12000; ++trial) {
        const float size = std::exp2(sizeLog(rng));
        const float x0 = xMin + std::min(pos(rng), extent - size);
        const float z0 = zMin + std::min(pos(rng), extent - size);
        const HeightField::Range r = field.range(x0, z0, x0 + size, z0 + size);
        const float step = size / static_cast<float>(kSamples - 1);
        heightmap.generate_column_heights_spaced(x0, z0, kSamples, kSamples, step, h.data());
        for (const float v : h) {
            if (v < r.min || v > r.max) {
                ++violations;
                worstOvershoot = std::max(worstOvershoot, std::max(r.min - v, v - r.max));
            }
        }
    }
    std::printf("height field soundness: %zu violations (worst overshoot %.4f m)\n", violations,
                static_cast<double>(worstOvershoot));
    CHECK(violations == 0);

    const HeightField::Range whole = field.whole_range();
    CHECK(whole.min < -10.0f); // this seed's terrain really does span both signs
    CHECK(whole.max > 10.0f);
}

TEST_CASE("fine 1/16 m height field is sound too", "[svo][heightfield]") {
    const world::generation::HeightmapGenerator heightmap(1337);
    const float xMin = 4.0f;
    const float zMin = -12.0f;
    const float extent = 32.0f;
    const HeightField field(heightmap, xMin, zMin, extent, 1.0f / 16.0f);
    std::printf("fine height field: %d cells, margin %.4f m\n", field.cell_count(),
                static_cast<double>(field.margin()));
    REQUIRE(field.cell_count() == 512);
    CHECK(field.covers(xMin, zMin, xMin + extent, zMin + extent));
    CHECK_FALSE(field.covers(xMin - 1.0f, zMin, xMin + 2.0f, zMin + 2.0f));

    std::mt19937 rng(7);
    std::uniform_real_distribution<float> pos(0.0f, extent);
    std::uniform_real_distribution<float> sizeLog(-6.0f, 2.0f); // 1.6 cm .. 4 m footprints
    std::size_t violations = 0;
    constexpr int kSamples = 7;
    std::vector<float> h(static_cast<std::size_t>(kSamples) * static_cast<std::size_t>(kSamples));
    for (int trial = 0; trial < 12000; ++trial) {
        const float size = std::exp2(sizeLog(rng));
        const float x0 = xMin + std::min(pos(rng), extent - size);
        const float z0 = zMin + std::min(pos(rng), extent - size);
        const HeightField::Range r = field.range(x0, z0, x0 + size, z0 + size);
        const float step = size / static_cast<float>(kSamples - 1);
        heightmap.generate_column_heights_spaced(x0, z0, kSamples, kSamples, step, h.data());
        for (const float v : h) {
            violations += (v < r.min || v > r.max) ? 1u : 0u;
        }
    }
    std::printf("fine height field soundness: %zu violations\n", violations);
    CHECK(violations == 0);
}

TEST_CASE("height field range is monotone in the footprint", "[svo][heightfield]") {
    const world::generation::HeightmapGenerator heightmap(1337);
    const HeightField field(heightmap, -32.0f, -32.0f, 64.0f, 0.5f);
    const HeightField::Range small = field.range(0.0f, 0.0f, 1.0f, 1.0f);
    const HeightField::Range big = field.range(-8.0f, -8.0f, 8.0f, 8.0f);
    CHECK(big.min <= small.min);
    CHECK(big.max >= small.max);
    // A query narrower than a cell still answers with its enclosing cell's range.
    const HeightField::Range tiny = field.range(0.1f, 0.1f, 0.15f, 0.15f);
    CHECK(tiny.min <= small.min);
    CHECK(tiny.max <= small.max + 1.0e-6f + field.margin());
}
