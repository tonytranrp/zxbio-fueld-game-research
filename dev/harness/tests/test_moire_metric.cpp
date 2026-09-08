// Prompt 005 goal 276: test the INSTRUMENT before trusting its readings.
//
// This repo has twice shipped a "verified" frame that was wrong, so a metric that judges a look
// pass has to be falsified before it is used. These cases are synthetic on purpose: the answer is
// known by construction rather than by my opinion of a screenshot, and they run with no GPU, no
// PNG on disk and no scene.
//
// The reference-capture validation -- the owner's target capture scoring 1.14 against the frame he
// complained about scoring 10.88, a 9.5x separation -- is in research/fine-grain-look-log.md,
// because it depends on two committed PNGs and belongs with the reasoning rather than in a unit
// test that would then need image files to run.

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>

#include "moire_metric.hpp"

using dev::harness::Image;
using dev::harness::kMinCarrierRms;
using dev::harness::moire_ratio;

namespace {

/// A smooth large-scale form plus an optional sinusoid of the given period along x.
[[nodiscard]] Image make_pattern(double period_px, double amplitude) {
    constexpr std::uint32_t kSize = 192;
    Image image;
    image.width = kSize;
    image.height = kSize;
    image.rgb.resize(static_cast<std::size_t>(kSize) * kSize * 3);
    for (std::uint32_t y = 0; y < kSize; ++y) {
        for (std::uint32_t x = 0; x < kSize; ++x) {
            // The form: low-frequency, and present in every case so the mask has something to keep.
            double v = 0.45 + 0.10 * std::sin(static_cast<double>(x) / 53.0);
            if (period_px > 0.0) {
                v += amplitude * std::sin(2.0 * std::numbers::pi * static_cast<double>(x) / period_px);
            }
            const auto b = static_cast<std::uint8_t>(std::lround(255.0 * (v < 0 ? 0 : (v > 1 ? 1 : v))));
            const std::size_t p = (static_cast<std::size_t>(y) * kSize + x) * 3;
            image.rgb[p + 0] = b;
            image.rgb[p + 1] = b;
            image.rgb[p + 2] = b;
        }
    }
    return image;
}

} // namespace

TEST_CASE("the moire metric separates a wanted stipple from aliasing", "[harness][moire]") {
    // A 6 px pattern is texture a viewer can resolve; a 2.1 px pattern is against the pixel Nyquist
    // limit and is the thing being complained about. Same amplitude, same form underneath, so the
    // ONLY difference between these two images is the frequency -- which is the property the metric
    // claims to measure and therefore the only thing that may move its answer.
    const auto wanted = moire_ratio(make_pattern(6.0, 0.08));
    const auto aliased = moire_ratio(make_pattern(2.1, 0.08));

    REQUIRE(wanted.valid);
    REQUIRE(aliased.valid);
    CHECK(wanted.ratio < 1.0);   // measured 0.173
    CHECK(aliased.ratio > 10.0); // measured 43.08
    // The separation is the number that makes the metric usable at all; measured 249x.
    CHECK(aliased.ratio / wanted.ratio > 50.0);
}

TEST_CASE("the moire metric refuses a frame with no texture to judge", "[harness][moire]") {
    // THE STATED LIMITATION, pinned so it cannot regress into a silent lie. With no carrier the
    // ratio is numerical noise -- a smooth synthetic reads 0.21 and a textureless one 4.57, and the
    // second would look like a failing frame to a caller that only read `ratio`.
    const auto smooth = moire_ratio(make_pattern(0.0, 0.0));
    CHECK_FALSE(smooth.valid);
    CHECK_FALSE(smooth.note.empty());
    CHECK(smooth.carrier_rms < kMinCarrierRms);

    Image uniform;
    uniform.width = 64;
    uniform.height = 64;
    uniform.rgb.assign(static_cast<std::size_t>(64) * 64 * 3, 128);
    CHECK_FALSE(moire_ratio(uniform).valid);
}

TEST_CASE("the moire metric degrades gracefully on an unusable image", "[harness][moire]") {
    CHECK_FALSE(moire_ratio(Image{}).valid);
    Image sliver;
    sliver.width = 4;
    sliver.height = 4;
    sliver.rgb.assign(4ull * 4 * 3, 200);
    CHECK_FALSE(moire_ratio(sliver).valid);
}

TEST_CASE("cropping the top removes an overlay from the measurement", "[harness][moire]") {
    // svo_ground_hilltop.png has a stats panel baked into its first 160 rows. A panel is flat text
    // on flat grey, so it dilutes the carrier and drags the ratio toward the no-texture case;
    // `crop_top` exists for exactly that and this asserts it actually reaches the arithmetic.
    Image image = make_pattern(2.1, 0.08);
    for (std::uint32_t y = 0; y < 96; ++y) {
        for (std::uint32_t x = 0; x < image.width; ++x) {
            const std::size_t p = (static_cast<std::size_t>(y) * image.width + x) * 3;
            image.rgb[p + 0] = image.rgb[p + 1] = image.rgb[p + 2] = 30;
        }
    }
    const auto uncropped = moire_ratio(image, 0);
    const auto cropped = moire_ratio(image, 96);
    REQUIRE(cropped.valid);
    // The cropped half is entirely the aliased pattern, so its textured fraction is far higher.
    CHECK(cropped.textured_fraction > uncropped.textured_fraction);
}
