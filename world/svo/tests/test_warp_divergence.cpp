// Prompt 004 goal 271: tests for the warp-divergence instrument.
//
// This pass has now found SIX measuring instruments that reported a number while measuring nothing
// (research/frame-time-and-gpu-architecture-log.md's running note). This one gets tests before its number is quoted, and
// its cases are chosen so that a plausible implementation bug in each direction fails one of them:
// charging the peak per PIXEL instead of per warp, forgetting clipped tiles have fewer lanes,
// striding only in x, or returning 0 rather than 1 for an empty frame.

#include <cstdint>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "world/svo/warp_divergence.hpp"

using Catch::Approx;
using world::svo::warp_divergence;

TEST_CASE("uniform lane costs have no imbalance", "[svo][divergence]") {
    const std::vector<std::uint32_t> costs(8u * 4u, 37u);
    const auto d = warp_divergence(costs, 8, 4, 8, 4);
    REQUIRE(d.useful_lane_steps == 32u * 37u);
    REQUIRE(d.issued_lane_steps == 32u * 37u);
    REQUIRE(d.efficiency() == Approx(1.0));
}

TEST_CASE("one expensive lane costs the whole warp", "[svo][divergence]") {
    // The worst case: 32 lanes charged at the peak for one lane's work. This is the case that fails
    // if the peak is taken per-pixel rather than per-warp -- that bug reports a perfect 1.0.
    std::vector<std::uint32_t> costs(8u * 4u, 0u);
    costs[13] = 100u;
    const auto d = warp_divergence(costs, 8, 4, 8, 4);
    REQUIRE(d.useful_lane_steps == 100u);
    REQUIRE(d.issued_lane_steps == 32u * 100u);
    REQUIRE(d.efficiency() == Approx(1.0 / 32.0));
}

TEST_CASE("imbalance BETWEEN warps is free; inside one it is not", "[svo][divergence]") {
    // Two warps side by side, each internally uniform at a different cost. Warps do not wait for
    // each other, so this frame is perfectly efficient. A bug that ignores the tiling and treats
    // the frame as one warp reports 0.625 here.
    std::vector<std::uint32_t> costs(16u * 4u, 0u);
    for (std::uint32_t y = 0; y < 4; ++y) {
        for (std::uint32_t x = 0; x < 16; ++x) {
            costs[static_cast<std::size_t>(y) * 16u + x] = x < 8 ? 10u : 40u;
        }
    }
    REQUIRE(warp_divergence(costs, 16, 4, 8, 4).efficiency() == Approx(1.0));

    // The same data under a 16-wide tiling puts the imbalance inside one warp, and it costs.
    REQUIRE(warp_divergence(costs, 16, 4, 16, 4).efficiency() ==
            Approx((8.0 * 10.0 + 8.0 * 40.0) / (16.0 * 40.0)));
}

TEST_CASE("a clipped tile is charged only for the lanes it has", "[svo][divergence]") {
    const std::vector<std::uint32_t> costs{1u, 2u, 3u};
    const auto d = warp_divergence(costs, 3, 1, 8, 4);
    REQUIRE(d.useful_lane_steps == 6u);
    REQUIRE(d.issued_lane_steps == 3u * 3u); // three real lanes at the peak of 3, not eight
    REQUIRE(d.efficiency() == Approx(6.0 / 9.0));
}

TEST_CASE("the tiling is two-dimensional", "[svo][divergence]") {
    // Cost varies along y only. An 8x4 tiling splits exactly along the boundary (both warps
    // uniform); a 4x8 tiling straddles it. A bug that only ever strides in x makes them equal.
    std::vector<std::uint32_t> costs(8u * 8u, 0u);
    for (std::uint32_t y = 0; y < 8; ++y) {
        for (std::uint32_t x = 0; x < 8; ++x) {
            costs[static_cast<std::size_t>(y) * 8u + x] = y < 4 ? 1u : 9u;
        }
    }
    REQUIRE(warp_divergence(costs, 8, 8, 8, 4).efficiency() == Approx(1.0));
    REQUIRE(warp_divergence(costs, 8, 8, 4, 8).efficiency() == Approx((4.0 * 1.0 + 4.0 * 9.0) / (8.0 * 9.0)));
}

TEST_CASE("degenerate inputs return the identity rather than dividing by zero", "[svo][divergence]") {
    const std::vector<std::uint32_t> empty;
    REQUIRE(warp_divergence(empty, 0, 0, 8, 4).efficiency() == Approx(1.0));

    const std::vector<std::uint32_t> zeros(32u, 0u);
    REQUIRE(warp_divergence(zeros, 8, 4, 8, 4).efficiency() == Approx(1.0));
    REQUIRE(warp_divergence(zeros, 8, 4, 0, 4).efficiency() == Approx(1.0));

    // A span too short for the stated frame is refused, not read past its end.
    const std::vector<std::uint32_t> tooShort(10u, 5u);
    REQUIRE(warp_divergence(tooShort, 8, 4, 8, 4).issued_lane_steps == 0u);
}

TEST_CASE("efficiency is always a fraction, over random maps", "[svo][divergence]") {
    // The bound is asserted over data rather than taken from the algebra, per rule 34's
    // property-based discipline.
    std::uint32_t rng = 0x9E3779B9u;
    const auto next = [&] {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        return rng;
    };
    for (int trial = 0; trial < 200; ++trial) {
        const std::uint32_t w = 1u + next() % 40u;
        const std::uint32_t h = 1u + next() % 40u;
        std::vector<std::uint32_t> costs(static_cast<std::size_t>(w) * h);
        bool anyNonZero = false;
        for (std::uint32_t& c : costs) {
            c = next() % 200u;
            anyNonZero = anyNonZero || c != 0u;
        }
        const auto d = warp_divergence(costs, w, h, 8, 4);
        REQUIRE(d.efficiency() > 0.0);
        REQUIRE(d.efficiency() <= Approx(1.0));
        REQUIRE(d.useful_lane_steps <= d.issued_lane_steps);
        if (anyNonZero) {
            REQUIRE(d.issued_lane_steps > 0u);
        }
    }
}
