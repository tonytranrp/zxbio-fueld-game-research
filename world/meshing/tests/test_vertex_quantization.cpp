#include <cmath>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "world/meshing/vertex_quantization.hpp"

using world::meshing::dequantize_position_16;
using world::meshing::kPositionQuantScale;
using world::meshing::quantize_position_16;

TEST_CASE("position quantization round-trip error is bounded by half a step", "[meshing][quantization]") {
    // Sweep the full legal range [-1, 32] densely; half a 1/1024-voxel step is the bound.
    const float halfStep = 0.5f / kPositionQuantScale;
    for (int i = 0; i <= 33000; ++i) {
        const float p = -1.0f + static_cast<float>(i) * (33.0f / 33000.0f);
        const float back = dequantize_position_16(quantize_position_16(p));
        CHECK(std::abs(back - p) <= halfStep * 1.0001f); // epsilon for the final float mad
    }
}

TEST_CASE("chunk-size offsets are exact step multiples (seam lattice alignment)", "[meshing][quantization]") {
    // The dyadic-step property Group K's design leans on: local coordinate p in chunk B and
    // p + 32 in its -X neighbor A describe the same world position, and their quantized values
    // must differ by exactly 32 * 1024 steps whenever the two encoders see bit-identical local
    // fractions. (When their input floats disagree, drift is bounded by 1 step -- covered by the
    // round-trip bound above.)
    for (int i = 0; i < 1024; ++i) {
        const float frac = static_cast<float>(i) / 1024.0f; // exactly representable fractions
        const std::uint16_t qLow = quantize_position_16(frac);
        const std::uint16_t qHigh = quantize_position_16(frac + 32.0f);
        CHECK(static_cast<int>(qHigh) - static_cast<int>(qLow) == 32 * 1024);
    }
}

TEST_CASE("quantization clamps out-of-range input instead of wrapping", "[meshing][quantization]") {
    CHECK(quantize_position_16(-5.0f) == 0);
    CHECK(dequantize_position_16(quantize_position_16(100.0f)) > 32.0f); // clamped high, not wrapped to 0
}
