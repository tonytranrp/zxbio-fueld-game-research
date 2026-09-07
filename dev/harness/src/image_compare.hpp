#pragma once

// Golden-image comparison (Prompt 002 goal 217).
//
// THE REASONING, because this project has already buried two image metrics and the third has to
// justify itself (research/gpu-voxel-streaming-and-profiling-research.md §6.5/§6.6):
//
//   * A BYTEWISE compare died first: bloom made 97.7% of sky pixels differ.
//   * A TOLERANCE BAND died second: the gradient sky sweeps 100+/255 across a frame, so any band
//     wide enough to pass the sky was wide enough to pass a broken terrain.
//   * The LOCAL-CONTRAST metric that replaced them is a SCENE-CONTENT check -- "is there a world
//     here at all" -- and by construction cannot see a regression that keeps the same amount of
//     texture. It stays; it is not the regression metric.
//
// What is used here instead, and why:
//
//   * Per-pixel MEAN ABSOLUTE DIFFERENCE over all channels, plus the FRACTION OF PIXELS whose
//     max-channel difference exceeds a per-pixel tolerance. Two numbers, because §6.5's own
//     conclusion is that a perceptual mean can HIDE a small, real, localized error (Darnell's
//     warning) while a pure pixel count is drowned by driver noise -- Unity's two-level structure,
//     with the cheap per-pixel term this project can actually implement.
//   * NOT FLIP, despite it being the best-motivated metric in the literature. FLIP is a
//     BSD-3-Clause single header and would be a genuinely good fit, but it is a new dependency for
//     a comparison whose entire job here is "did this change" rather than "how bad does this look
//     to a human", and the calibration below shows the simple metric separates the cases by more
//     than an order of magnitude. Recorded as a decision, not an oversight: if cross-backend
//     comparison is ever wanted (see below), FLIP is the thing to reach for.
//   * PER-BACKEND GOLDENS, not one shared golden. §6.6 is unambiguous -- Unreal keys goldens by
//     RHI and hardware hash, Khronos' CTS designates a primary platform -- and the calibration in
//     research/dev-harness-log.md measured the vk-vs-d3d12 distance directly rather than assuming.

#include <cstdint>
#include <string>
#include <vector>

namespace dev::harness {

struct Image {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> rgb; // 3 bytes per pixel

    [[nodiscard]] bool empty() const noexcept { return rgb.empty(); }
};

struct Comparison {
    bool comparable = false;             // false when the two images differ in size, or one is missing
    double mean_abs_difference = 0.0;    // 0..255, over every channel of every pixel
    double changed_pixel_fraction = 0.0; // pixels whose max-channel delta exceeds kPixelTolerance
    std::uint32_t max_channel_difference = 0;
    std::string note;
};

// A pixel counts as changed above this. 8/255 is above per-channel FP-rounding and driver AA
// jitter and far below a real shading change.
inline constexpr std::uint32_t kPixelTolerance = 8;

// THE THRESHOLDS, AND THE FOUR MEASUREMENTS THEY SIT BETWEEN.
//
// Measured on `macro_ground`'s pose, 1280x720, this machine (RTX, 2026-09-06), by promoting a
// golden and re-running. Every number is in research/dev-harness-log.md with the commands:
//
//   case                                    mean/255   changed %   max chan   verdict
//   vk vs vk,  TAA ON                         0.7376     0.99609        212   (see below)
//   vk vs vk,  TAA off                        0.7226     0.34082        213   the NOISE FLOOR
//   vk vs vk,  --grain 0.5 (a real change)    1.8007     7.19032        216   the SIGNAL
//   vk vs d3d12, TAA off                      2.5481    12.76360        219   a different backend
//
// Three conclusions, each with a number behind it:
//
// 1. THE THRESHOLDS ARE THE GEOMETRIC MIDPOINT of floor and signal, not a round number chosen to
//    make the current build pass: sqrt(0.34 x 7.19) = 1.56% and sqrt(0.72 x 1.80) = 1.14. Rounded
//    to 1.5% and 1.2, they sit 4.4x above the floor and 4.8x below the signal.
// 2. VISUAL SCENARIOS RUN WITH --no-taa. With TAA on, vk-vs-vk moves 0.996% of pixels -- a
//    scenario would fail against its own golden roughly as often as not. This is exactly the
//    recommendation in research/gpu-voxel-streaming-and-profiling-research.md 6.6, now with this
//    engine's own number attached rather than taken on faith.
// 3. PER-BACKEND GOLDENS ARE MANDATORY. vk-vs-d3d12 moves 12.8% of pixels at the same pose with
//    the same settings -- 37x the noise floor and larger than a deliberate shading change. No
//    threshold separates "a different backend" from "a regression", which is what 6.6 predicted
//    and what UE and Khronos' CTS both do in practice. Cross-backend comparison is goal 218's
//    open follow-up, and FLIP is the thing to reach for if it is ever wanted.
//
// AND ONE HONEST NEGATIVE, because the prompt asked for a 1-pixel change to fail and it does not:
// the noise floor is 0.34% of 921,600 pixels, or about 3,100 pixels. A single changed pixel is
// 0.0001% -- three and a half orders of magnitude under the floor. NO threshold over this metric
// can catch a 1-pixel change without failing on every clean re-run, and `max_channel_difference`
// cannot rescue it either: it reads 212-219 in EVERY case above, including vk-vs-vk, because TAA
// and AA always move some pixel on a high-contrast edge by most of its range. What this metric
// does catch is a shading-term change, which is what a regression in this engine actually looks
// like -- and the --grain row is the evidence that it does.
inline constexpr double kMaxMeanAbsDifference = 1.2;
inline constexpr double kMaxChangedPixelFraction = 0.015; // 1.5% of the frame

[[nodiscard]] Image load_png(const std::string& path);
[[nodiscard]] bool save_png(const std::string& path, const Image& image);
[[nodiscard]] Comparison compare(const Image& golden, const Image& actual);
[[nodiscard]] bool passes(const Comparison& comparison) noexcept;

// Where the difference is, so a human can LOOK at a failure rather than read a number about it:
// unchanged pixels go to a dimmed greyscale of the actual frame, changed ones to a magenta whose
// brightness is the size of the change.
[[nodiscard]] Image difference_image(const Image& golden, const Image& actual);

} // namespace dev::harness
