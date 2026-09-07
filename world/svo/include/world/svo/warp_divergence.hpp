#pragma once

// Prompt 004 goal 271: warp divergence, measured from a per-pixel cost map.
//
// WHY THIS EXISTS. Goals 267 and 269 both rest on a claim about warp behaviour -- that a marcher's
// lanes finish at very different times, so redistributing their work would win a lot. The published
// numbers behind that claim are large (Aila & Laine's Table 2: 63.6 -> 122.1 Mrays/s from persistent
// threads on identical traversal code). The honest way to decide whether they transfer to THIS
// content is to measure the divergence here, and this machine cannot: its NVIDIA driver does not
// expose VK_KHR_performance_query (only the Intel iGPU does -- see research/frame-time-and-gpu-architecture-log.md
// section 12), so the occupancy and stall-reason counters are not collectable without the Nsight
// Perf SDK and an admin-only registry change.
//
// This is the instrument that measures the same underlying quantity with no driver dependency, from
// the CPU reference's own per-pixel step counts -- deterministic, testable, and available in CI.
//
// THE METRIC. A warp runs until its SLOWEST lane finishes, so a warp costs `lanes * max(steps)`
// issue slots while doing only `sum(steps)` useful work. Over a whole frame,
//
//     efficiency = sum over warps of sum(steps) / sum over warps of (lanes * max(steps))
//
// and `1 - efficiency` is the theoretical ceiling on ANY work-redistribution scheme: persistent
// threads, ray reordering, SER. If efficiency is already 93%, no such scheme can win more than 7%,
// however large its published figure on other content.
//
// WHAT IT DOES NOT MEASURE, stated plainly. This is divergence in traversal STEPS, not in time. Two
// lanes taking the same number of steps still finish at different times if one misses cache and the
// other does not, so the result is a lower bound on total latency variance. The complementary
// measurement is goal 267's: removing `SV_Depth` -- and with it the ROP's ordered export -- won 17%
// on vk, which is larger than the step divergence and is therefore evidence that the remainder is
// memory latency rather than work imbalance.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

namespace world::svo {

/// One warp tiling's divergence result over a frame's per-pixel cost map.
struct WarpDivergence {
    std::uint64_t useful_lane_steps = 0; ///< Sum of every lane's own step count.
    std::uint64_t issued_lane_steps = 0; ///< Sum over warps of `lanes * max(steps)`.

    /// Fraction of issued warp slots doing real work, in [0, 1]. An empty or all-zero map is 1.0:
    /// a frame with no work has no imbalance, which is the honest reading of "nothing diverged".
    [[nodiscard]] constexpr double efficiency() const noexcept {
        return issued_lane_steps > 0
                   ? static_cast<double>(useful_lane_steps) / static_cast<double>(issued_lane_steps)
                   : 1.0;
    }
};

/// Divergence of `costs` (row-major, `width` x `height`) under a `tile_w` x `tile_h` warp tiling.
///
/// The pixel-to-warp mapping on NVIDIA is not publicly documented -- a 32-thread pixel-shader warp
/// is eight 2x2 quads, and 8x4 is the commonly cited arrangement -- so callers should report several
/// tilings and show the conclusion does not depend on picking the right one. Tiles are clipped at
/// the right and bottom edges, and a clipped tile is charged only for the lanes it actually has.
[[nodiscard]] inline WarpDivergence warp_divergence(std::span<const std::uint32_t> costs, std::uint32_t width,
                                                    std::uint32_t height, std::uint32_t tile_w,
                                                    std::uint32_t tile_h) noexcept {
    WarpDivergence out;
    if (width == 0 || height == 0 || tile_w == 0 || tile_h == 0 ||
        costs.size() < static_cast<std::size_t>(width) * height) {
        return out;
    }
    for (std::uint32_t ty = 0; ty < height; ty += tile_h) {
        for (std::uint32_t tx = 0; tx < width; tx += tile_w) {
            std::uint32_t peak = 0;
            std::uint64_t sum = 0;
            std::uint32_t lanes = 0;
            for (std::uint32_t dy = 0; dy < tile_h && ty + dy < height; ++dy) {
                for (std::uint32_t dx = 0; dx < tile_w && tx + dx < width; ++dx) {
                    const std::uint32_t cost = costs[static_cast<std::size_t>(ty + dy) * width + (tx + dx)];
                    peak = std::max(peak, cost);
                    sum += cost;
                    ++lanes;
                }
            }
            out.useful_lane_steps += sum;
            out.issued_lane_steps += static_cast<std::uint64_t>(lanes) * peak;
        }
    }
    return out;
}

} // namespace world::svo
