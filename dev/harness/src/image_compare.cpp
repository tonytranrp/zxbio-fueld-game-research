#include "image_compare.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "render/diligent/frame_verify.hpp"

namespace dev::harness {

namespace {

[[nodiscard]] Image from_frame(const render::diligent::FrameImage& frame) {
    Image image;
    image.width = frame.width;
    image.height = frame.height;
    image.rgb = frame.rgb;
    return image;
}

} // namespace

Image load_png(const std::string& path) {
    return from_frame(render::diligent::decode_png_file(path.c_str()));
}

bool save_png(const std::string& path, const Image& image) {
    render::diligent::FrameImage frame;
    frame.width = image.width;
    frame.height = image.height;
    frame.rgb = image.rgb;
    return render::diligent::encode_png_file(path.c_str(), frame);
}

Comparison compare(const Image& golden, const Image& actual) {
    Comparison result;
    if (golden.empty()) {
        result.note = "no golden";
        return result;
    }
    if (actual.empty()) {
        result.note = "no captured frame";
        return result;
    }
    if (golden.width != actual.width || golden.height != actual.height) {
        result.note = "size mismatch: golden " + std::to_string(golden.width) + "x" +
                      std::to_string(golden.height) + " vs actual " + std::to_string(actual.width) + "x" +
                      std::to_string(actual.height);
        return result;
    }
    result.comparable = true;

    // Two numbers, deliberately (see image_compare.hpp): a mean, which a small localized error can
    // hide in, and a changed-pixel count, which driver noise floods. Neither alone is a gate.
    std::uint64_t absSum = 0;
    std::uint64_t changed = 0;
    const std::size_t pixels = static_cast<std::size_t>(golden.width) * golden.height;
    for (std::size_t i = 0; i < pixels; ++i) {
        std::uint32_t maxDelta = 0;
        for (std::size_t c = 0; c < 3; ++c) {
            const auto delta = static_cast<std::uint32_t>(
                std::abs(static_cast<int>(golden.rgb[i * 3 + c]) - static_cast<int>(actual.rgb[i * 3 + c])));
            absSum += delta;
            maxDelta = std::max(maxDelta, delta);
        }
        result.max_channel_difference = std::max(result.max_channel_difference, maxDelta);
        if (maxDelta > kPixelTolerance) {
            ++changed;
        }
    }
    result.mean_abs_difference = static_cast<double>(absSum) / static_cast<double>(pixels * 3);
    result.changed_pixel_fraction = static_cast<double>(changed) / static_cast<double>(pixels);
    return result;
}

bool passes(const Comparison& comparison) noexcept {
    return comparison.comparable && comparison.mean_abs_difference <= kMaxMeanAbsDifference &&
           comparison.changed_pixel_fraction <= kMaxChangedPixelFraction;
}

Image difference_image(const Image& golden, const Image& actual) {
    Image out;
    if (golden.empty() || actual.empty() || golden.width != actual.width || golden.height != actual.height) {
        return out;
    }
    out.width = golden.width;
    out.height = golden.height;
    out.rgb.resize(golden.rgb.size());
    const std::size_t pixels = static_cast<std::size_t>(out.width) * out.height;
    for (std::size_t i = 0; i < pixels; ++i) {
        std::uint32_t maxDelta = 0;
        for (std::size_t c = 0; c < 3; ++c) {
            maxDelta = std::max(
                maxDelta, static_cast<std::uint32_t>(std::abs(static_cast<int>(golden.rgb[i * 3 + c]) -
                                                              static_cast<int>(actual.rgb[i * 3 + c]))));
        }
        if (maxDelta > kPixelTolerance) {
            // Magenta, brightness scaled by the size of the change and floored so a 9/255 delta is
            // still visible -- the point of this image is to answer WHERE, not HOW MUCH.
            const auto intensity = static_cast<std::uint8_t>(std::min<std::uint32_t>(255, 96 + maxDelta * 4));
            out.rgb[i * 3 + 0] = intensity;
            out.rgb[i * 3 + 1] = 0;
            out.rgb[i * 3 + 2] = intensity;
        } else {
            // The actual frame, dimmed to a quarter and desaturated, so the magenta reads against
            // a recognisable scene rather than against black.
            const auto grey = static_cast<std::uint8_t>((static_cast<std::uint32_t>(actual.rgb[i * 3 + 0]) +
                                                         actual.rgb[i * 3 + 1] + actual.rgb[i * 3 + 2]) /
                                                        12u);
            out.rgb[i * 3 + 0] = grey;
            out.rgb[i * 3 + 1] = grey;
            out.rgb[i * 3 + 2] = grey;
        }
    }
    return out;
}

} // namespace dev::harness
