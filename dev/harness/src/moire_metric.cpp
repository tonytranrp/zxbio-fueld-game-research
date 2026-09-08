#include "moire_metric.hpp"

#include <cmath>
#include <vector>

namespace dev::harness {
namespace {

// Rec.709 luminance, 0..1.
[[nodiscard]] std::vector<double> luminance(const Image& image, std::uint32_t cropTop) {
    const std::uint32_t h = image.height > cropTop ? image.height - cropTop : 0;
    std::vector<double> out(static_cast<std::size_t>(image.width) * h, 0.0);
    for (std::uint32_t y = 0; y < h; ++y) {
        const std::size_t src = static_cast<std::size_t>(y + cropTop) * image.width * 3;
        const std::size_t dst = static_cast<std::size_t>(y) * image.width;
        for (std::uint32_t x = 0; x < image.width; ++x) {
            const std::size_t p = src + static_cast<std::size_t>(x) * 3;
            const double r = image.rgb[p + 0] / 255.0;
            const double g = image.rgb[p + 1] / 255.0;
            const double b = image.rgb[p + 2] / 255.0;
            out[dst + x] = 0.2126 * r + 0.7152 * g + 0.0722 * b;
        }
    }
    return out;
}

// Separable box mean over a k x k window with edge replication. A box blur is a prefix sum, so the
// whole filter bank costs a handful of linear passes over the frame -- which is why this metric
// needs no FFT and can run inside a scenario without showing up in the frame report.
[[nodiscard]] std::vector<double> box_blur(const std::vector<double>& in, std::uint32_t w, std::uint32_t h,
                                           int k) {
    const int r = k / 2;
    std::vector<double> tmp(in.size(), 0.0);
    std::vector<double> out(in.size(), 0.0);
    const auto clampi = [](int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); };
    for (std::uint32_t y = 0; y < h; ++y) {
        const std::size_t row = static_cast<std::size_t>(y) * w;
        for (std::uint32_t x = 0; x < w; ++x) {
            double sum = 0.0;
            for (int d = -r; d <= r; ++d) {
                sum += in[row + static_cast<std::uint32_t>(
                                    clampi(static_cast<int>(x) + d, 0, static_cast<int>(w) - 1))];
            }
            tmp[row + x] = sum / static_cast<double>(k);
        }
    }
    for (std::uint32_t x = 0; x < w; ++x) {
        for (std::uint32_t y = 0; y < h; ++y) {
            double sum = 0.0;
            for (int d = -r; d <= r; ++d) {
                const auto yy =
                    static_cast<std::uint32_t>(clampi(static_cast<int>(y) + d, 0, static_cast<int>(h) - 1));
                sum += tmp[static_cast<std::size_t>(yy) * w + x];
            }
            out[static_cast<std::size_t>(y) * w + x] = sum / static_cast<double>(k);
        }
    }
    return out;
}

} // namespace

MoireResult moire_ratio(const Image& image, std::uint32_t cropTop) {
    MoireResult result;
    if (image.empty() || image.height <= cropTop + 8 || image.width < 16) {
        result.note = "image too small to measure";
        return result;
    }
    const std::uint32_t w = image.width;
    const std::uint32_t h = image.height - cropTop;

    const std::vector<double> L = luminance(image, cropTop);
    const std::vector<double> b3 = box_blur(L, w, h, 3);
    const std::vector<double> b7 = box_blur(L, w, h, 7);

    // The two bands. `hi` passes roughly a 2-4 px period and `mid` roughly 4-8 px; the ratio
    // between their energies is the metric (see the header for why this is the right question).
    std::vector<double> hi(L.size());
    std::vector<double> mid(L.size());
    std::vector<double> hiSquared(L.size());
    for (std::size_t i = 0; i < L.size(); ++i) {
        hi[i] = L[i] - b3[i];
        mid[i] = b3[i] - b7[i];
        hiSquared[i] = hi[i] * hi[i];
    }

    // The textured-region mask: the carrier's own local amplitude, thresholded against the frame's
    // mean of the same quantity.
    const std::vector<double> envSquared = box_blur(hiSquared, w, h, 9);
    double envSum = 0.0;
    std::size_t envCount = 0;
    for (double v : envSquared) {
        const double e = std::sqrt(v > 0.0 ? v : 0.0);
        if (e > 0.0) {
            envSum += e;
            ++envCount;
        }
    }
    if (envCount == 0) {
        result.note = "no high-frequency content at all";
        return result;
    }
    const double threshold = kTextureMaskFraction * (envSum / static_cast<double>(envCount));

    double energyHi = 0.0;
    double energyMid = 0.0;
    std::size_t kept = 0;
    for (std::size_t i = 0; i < L.size(); ++i) {
        if (std::sqrt(envSquared[i] > 0.0 ? envSquared[i] : 0.0) <= threshold) {
            continue;
        }
        energyHi += hi[i] * hi[i];
        energyMid += mid[i] * mid[i];
        ++kept;
    }
    if (kept < 100) {
        result.note = "textured region too small to measure";
        return result;
    }
    const double n = static_cast<double>(kept);
    energyHi /= n;
    energyMid /= n;

    result.carrier_rms = std::sqrt(energyHi);
    result.textured_fraction = n / static_cast<double>(L.size());
    result.ratio = energyHi / (energyMid > 1e-12 ? energyMid : 1e-12);
    // The stated limitation: with no carrier the ratio is numerical noise, and a textureless
    // synthetic reads 4.57 -- which would look like a failing frame. Refuse rather than mislead.
    if (result.carrier_rms < kMinCarrierRms) {
        result.note = "too little texture to judge (carrier below the floor)";
        return result;
    }
    result.valid = true;
    return result;
}

} // namespace dev::harness
