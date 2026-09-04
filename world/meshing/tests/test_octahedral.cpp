#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

#include <catch2/catch_test_macros.hpp>

#include "world/meshing/octahedral.hpp"

using world::meshing::decode_octahedral_16;
using world::meshing::encode_octahedral_16;
using world::meshing::OctNormal16;

namespace {

float angular_error_degrees(const glm::vec3& original) {
    const glm::vec3 decoded = decode_octahedral_16(encode_octahedral_16(original));
    const float d = std::clamp(glm::dot(glm::normalize(original), decoded), -1.0f, 1.0f);
    return std::acos(d) * 180.0f / std::numbers::pi_v<float>;
}

// Deterministic low-discrepancy directions over the whole sphere (spherical Fibonacci) -- a
// representative stand-in for Surface-Nets normals, which can point anywhere.
glm::vec3 fibonacci_direction(int i, int count) {
    const float golden = std::numbers::phi_v<float>;
    const float z = 1.0f - (2.0f * static_cast<float>(i) + 1.0f) / static_cast<float>(count);
    const float r = std::sqrt(std::max(0.0f, 1.0f - z * z));
    const float phi = 2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / golden;
    return {r * std::cos(phi), r * std::sin(phi), z};
}

} // namespace

TEST_CASE("octahedral 16-bit round-trip stays within stated angular error", "[meshing][octahedral]") {
    // Acceptance threshold (Group K task 25): 8+8-bit octahedral encoding is known-good to ~1 deg
    // worst case in the literature; 1.5 deg is far below anything visible in diffuse terrain
    // lighting. The mean is asserted much tighter to catch systematic (not just worst-case) bias.
    constexpr int kSamples = 4096;
    float maxError = 0.0f;
    double sumError = 0.0;
    for (int i = 0; i < kSamples; ++i) {
        const float e = angular_error_degrees(fibonacci_direction(i, kSamples));
        maxError = std::max(maxError, e);
        sumError += e;
    }
    const double meanError = sumError / kSamples;
    INFO("max " << maxError << " deg, mean " << meanError << " deg over " << kSamples << " directions");
    CHECK(maxError < 1.5f);
    CHECK(meanError < 0.6);
}

TEST_CASE("octahedral encoding is exact-ish on axis-aligned normals", "[meshing][octahedral]") {
    // Terrain is full of flat floors/walls; the six axis directions must decode essentially
    // exactly (they sit on octahedron vertices/edge midpoints, representable up to UNORM rounding).
    const glm::vec3 axes[] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    for (const glm::vec3& axis : axes) {
        CHECK(angular_error_degrees(axis) < 0.5f);
    }
}

TEST_CASE("octahedral re-encoding does not drift the decoded normal", "[meshing][octahedral]") {
    // Byte-exact idempotency is NOT guaranteed by the plain encoding: float rounding at UNORM
    // half-step boundaries and across the hemisphere fold can shift a re-encode by one lattice
    // step (the JCGT paper's costlier "precise" variant exists for exactly this). What the
    // pipeline actually needs is that a re-encode never *accumulates* error: the twice-decoded
    // normal must stay within one quantization step (~0.45 deg) of the once-decoded one.
    for (int i = 0; i < 512; ++i) {
        const glm::vec3 n = fibonacci_direction(i, 512);
        const glm::vec3 once = decode_octahedral_16(encode_octahedral_16(n));
        const glm::vec3 twice = decode_octahedral_16(encode_octahedral_16(once));
        const float d = std::clamp(glm::dot(once, twice), -1.0f, 1.0f);
        const float driftDegrees = std::acos(d) * 180.0f / std::numbers::pi_v<float>;
        CHECK(driftDegrees < 0.6f);
    }
}

TEST_CASE("octahedral encoding normalizes unnormalized input", "[meshing][octahedral]") {
    // Surface Nets hands over area-weighted sums, not unit vectors -- scaling must not matter.
    const glm::vec3 n{0.3f, -0.9f, 0.4f};
    CHECK(encode_octahedral_16(n) == encode_octahedral_16(n * 37.5f));
}
