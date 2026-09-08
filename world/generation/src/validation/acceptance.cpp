// Prompt 006 goal 317. See acceptance.hpp for why every band is a named constant with its citation.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numeric>

#include "world/generation/validation/acceptance.hpp"

namespace world::generation::validation {
namespace {

using field::Plane;

constexpr double kPi = 3.14159265358979323846;

/// Ordinary least squares on y = m x + c, plus R². Used by four of the ten metrics, all of which
/// are log-log fits, so a single shared implementation is also a single place for the fit to be
/// wrong in.
struct Fit {
    double slope = 0.0;
    double intercept = 0.0;
    double r_squared = 0.0;
};

[[nodiscard]] Fit least_squares(std::span<const double> x, std::span<const double> y) {
    Fit out;
    const auto n = static_cast<double>(x.size());
    if (x.size() < 2 || x.size() != y.size()) {
        return out;
    }
    const double sx = std::accumulate(x.begin(), x.end(), 0.0);
    const double sy = std::accumulate(y.begin(), y.end(), 0.0);
    const double mx = sx / n;
    const double my = sy / n;
    double sxx = 0.0;
    double sxy = 0.0;
    double syy = 0.0;
    for (std::size_t i = 0; i < x.size(); ++i) {
        sxx += (x[i] - mx) * (x[i] - mx);
        sxy += (x[i] - mx) * (y[i] - my);
        syy += (y[i] - my) * (y[i] - my);
    }
    if (sxx < 1e-18) {
        return out;
    }
    out.slope = sxy / sxx;
    out.intercept = my - out.slope * mx;
    out.r_squared = syy > 1e-18 ? (sxy * sxy) / (sxx * syy) : 1.0;
    return out;
}

/// Slope magnitude in degrees at a cell, by central difference. Boundary cells are skipped by the
/// caller rather than one-sided-differenced: a one-sided difference has a different noise
/// characteristic and would put a spurious spike in the histogram §9.1 tests for unimodality.
[[nodiscard]] double slope_deg_at(const TerrainField& f, std::span<const float> h, std::int32_t cx,
                                  std::int32_t cz) {
    const double d = 2.0 * static_cast<double>(f.geometry().cell_size);
    const double dzdx = (static_cast<double>(h[f.index(cx + 1, cz)]) - h[f.index(cx - 1, cz)]) / d;
    const double dzdz = (static_cast<double>(h[f.index(cx, cz + 1)]) - h[f.index(cx, cz - 1)]) / d;
    return std::atan(std::sqrt(dzdx * dzdx + dzdz * dzdz)) * 180.0 / kPi;
}

[[nodiscard]] double percentile(std::vector<double>& v, double q) {
    if (v.empty()) {
        return 0.0;
    }
    const auto k = std::min(v.size() - 1, static_cast<std::size_t>(q * static_cast<double>(v.size())));
    std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(k), v.end());
    return v[k];
}

} // namespace

// ------------------------------------------------------------------------------------- the FFT

void fft_in_place(std::span<double> real, std::span<double> imag) {
    const std::size_t n = real.size();
    if (n < 2 || imag.size() != n || (n & (n - 1)) != 0) {
        return; // not a power of two: the caller pads
    }
    // Bit-reversal permutation.
    for (std::size_t i = 1, j = 0; i < n; ++i) {
        std::size_t bit = n >> 1;
        for (; (j & bit) != 0; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            std::swap(real[i], real[j]);
            std::swap(imag[i], imag[j]);
        }
    }
    // Danielson-Lanczos butterflies.
    for (std::size_t len = 2; len <= n; len <<= 1) {
        const double angle = -2.0 * kPi / static_cast<double>(len);
        const double wr = std::cos(angle);
        const double wi = std::sin(angle);
        for (std::size_t i = 0; i < n; i += len) {
            double cr = 1.0;
            double ci = 0.0;
            for (std::size_t k = 0; k < len / 2; ++k) {
                const std::size_t a = i + k;
                const std::size_t b = a + len / 2;
                const double tr = real[b] * cr - imag[b] * ci;
                const double ti = real[b] * ci + imag[b] * cr;
                real[b] = real[a] - tr;
                imag[b] = imag[a] - ti;
                real[a] += tr;
                imag[a] += ti;
                const double nr = cr * wr - ci * wi;
                ci = cr * wi + ci * wr;
                cr = nr;
            }
        }
    }
}

// ------------------------------------------------------------------------------ §9.1 slope

SlopeStats slope_distribution(const TerrainField& field, bool landOnly, float seaLevel) {
    SlopeStats out;
    const std::span<const float> h = field.plane(Plane::Elevation);
    std::vector<double> slopes;
    slopes.reserve(field.cell_count());
    for (std::int32_t cz = 1; cz + 1 < field.cells(); ++cz) {
        for (std::int32_t cx = 1; cx + 1 < field.cells(); ++cx) {
            if (landOnly && h[field.index(cx, cz)] <= seaLevel) {
                continue;
            }
            slopes.push_back(slope_deg_at(field, h, cx, cz));
        }
    }
    out.samples = slopes.size();
    if (slopes.size() < 2) {
        return out;
    }
    const double mean =
        std::accumulate(slopes.begin(), slopes.end(), 0.0) / static_cast<double>(slopes.size());
    double m2 = 0.0;
    double m3 = 0.0;
    for (const double s : slopes) {
        const double d = s - mean;
        m2 += d * d;
        m3 += d * d * d;
    }
    m2 /= static_cast<double>(slopes.size());
    m3 /= static_cast<double>(slopes.size());
    out.mean_slope_deg = mean;
    out.skewness = m2 > 1e-12 ? m3 / std::pow(m2, 1.5) : 0.0;

    // Unimodality: count peaks in a smoothed 90-bin histogram. Smoothing first is load-bearing --
    // a raw histogram of a quarter-million samples has dozens of one-bin wiggles and would report
    // any real distribution as multimodal.
    constexpr std::size_t kBins = 90;
    std::vector<double> hist(kBins, 0.0);
    for (const double s : slopes) {
        const auto b = std::min(kBins - 1, static_cast<std::size_t>(s));
        hist[b] += 1.0;
    }
    for (int pass = 0; pass < 3; ++pass) {
        std::vector<double> next = hist;
        for (std::size_t i = 1; i + 1 < kBins; ++i) {
            next[i] = 0.25 * hist[i - 1] + 0.5 * hist[i] + 0.25 * hist[i + 1];
        }
        hist.swap(next);
    }
    const double peak = *std::max_element(hist.begin(), hist.end());
    // BOUNDARY MODES COUNT. A first version only looked at interior bins and reported "0 modes,
    // unimodal=yes" for a histogram whose single peak sat in bin 0 -- a monotonically decreasing
    // distribution, which is the commonest shape a gentle landscape produces. Reporting zero modes
    // as unimodal happened to give the right verdict for the wrong reason, which is worse than
    // being wrong.
    for (std::size_t i = 0; i < kBins; ++i) {
        const double left = i == 0 ? -1.0 : hist[i - 1];
        const double right = i + 1 == kBins ? -1.0 : hist[i + 1];
        // Only count peaks that are a real feature, not a ripple on a tail.
        if (hist[i] > left && hist[i] >= right && hist[i] > 0.05 * peak) {
            ++out.modes;
        }
    }
    out.unimodal = out.modes == 1;
    return out;
}

// --------------------------------------------------------------------------- §9.2 spectrum

SpectrumStats power_spectrum(const TerrainField& field) {
    SpectrumStats out;
    // Largest power of two that fits, centred -- rather than padding, which would put a step at the
    // pad boundary and inject a k^-2 ramp into the very quantity being measured.
    std::size_t n = 1;
    while (n * 2 <= static_cast<std::size_t>(field.cells())) {
        n *= 2;
    }
    if (n < 32) {
        return out;
    }
    const auto offset = static_cast<std::int32_t>((static_cast<std::size_t>(field.cells()) - n) / 2);
    const std::span<const float> h = field.plane(Plane::Elevation);

    std::vector<double> power(n / 2, 0.0);
    std::vector<double> re(n);
    std::vector<double> im(n);
    std::size_t transects = 0;

    const auto transform = [&](bool byRow, std::int32_t line) {
        double mean = 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            const auto k = static_cast<std::int32_t>(i);
            re[i] = byRow ? h[field.index(offset + k, line)] : h[field.index(line, offset + k)];
            mean += re[i];
        }
        mean /= static_cast<double>(n);
        for (std::size_t i = 0; i < n; ++i) {
            // Detrend by the mean and apply a Hann window. Without the window the transect's two
            // ends form a discontinuity when wrapped, and its spectrum is a k^-2 leak that would be
            // mistaken for exactly the Brownian slope this test looks for.
            const double w =
                0.5 - 0.5 * std::cos(2.0 * kPi * static_cast<double>(i) / static_cast<double>(n - 1));
            re[i] = (re[i] - mean) * w;
            im[i] = 0.0;
        }
        fft_in_place(re, im);
        for (std::size_t k = 1; k < n / 2; ++k) {
            power[k] += re[k] * re[k] + im[k] * im[k];
        }
        ++transects;
    };

    for (std::size_t i = 0; i < n; ++i) {
        transform(true, offset + static_cast<std::int32_t>(i));
        transform(false, offset + static_cast<std::int32_t>(i));
    }
    if (transects == 0) {
        return out;
    }
    for (double& p : power) {
        p /= static_cast<double>(transects);
    }

    // Fit over the band that is actually meaningful: above the lowest few wavenumbers (where a
    // finite transect has almost no samples) and below the top octave (where the window's own
    // roll-off dominates).
    std::vector<double> lx;
    std::vector<double> ly;
    const std::size_t kLo = 4;
    const std::size_t kHi = n / 4;
    for (std::size_t k = kLo; k < kHi; ++k) {
        if (power[k] > 1e-30) {
            lx.push_back(std::log(static_cast<double>(k)));
            ly.push_back(std::log(power[k]));
        }
    }
    const Fit fit = least_squares(lx, ly);
    out.beta = -fit.slope;
    out.r_squared = fit.r_squared;

    // §9.2: "assert no spurious peaks (periodicity)". A peak is a wavenumber whose power stands far
    // above the fitted power law -- which is the right test, because the spectrum falls steeply and
    // a raw maximum is always at k = kLo.
    double worst = 0.0;
    for (std::size_t k = kLo; k < kHi; ++k) {
        if (power[k] <= 1e-30) {
            continue;
        }
        const double expected = std::exp(fit.intercept + fit.slope * std::log(static_cast<double>(k)));
        const double excess = power[k] / std::max(expected, 1e-30);
        if (excess > worst) {
            worst = excess;
            out.peak_wavelength_cells = static_cast<double>(n) / static_cast<double>(k);
        }
    }
    // An order of magnitude above the trend is a spectral line, not scatter.
    out.spurious_peak = worst > 10.0;
    return out;
}

// ------------------------------------------------------------------------- §9.3 hypsometry

Hypsometry hypsometry(const TerrainField& field) {
    Hypsometry out;
    const std::span<const float> h = field.plane(Plane::Elevation);
    std::vector<double> land;
    land.reserve(field.cell_count());
    double lo = 0.0;
    double hi = 0.0;
    double sum = 0.0;
    bool first = true;
    for (const float v : h) {
        if (first) {
            lo = hi = v;
            first = false;
        }
        lo = std::min(lo, static_cast<double>(v));
        hi = std::max(hi, static_cast<double>(v));
        sum += v;
        if (v > 0.0f) {
            land.push_back(v);
        }
    }
    if (h.empty()) {
        return out;
    }
    out.land_fraction = static_cast<double>(land.size()) / static_cast<double>(h.size());
    const double mean = sum / static_cast<double>(h.size());
    out.hypsometric_integral = hi - lo > 1e-9 ? (mean - lo) / (hi - lo) : 0.0;
    if (land.empty()) {
        return out;
    }
    out.land_max_m = *std::max_element(land.begin(), land.end());
    out.land_median_m = percentile(land, 0.5);
    out.median_over_max = out.land_max_m > 1e-9 ? out.land_median_m / out.land_max_m : 0.0;
    return out;
}

// -------------------------------------------------------------------- §9.4 drainage density

double drainage_density(const TerrainField& field, double channelThresholdKm2) {
    const double cellArea = field.geometry().cell_area();
    const std::span<const float> acc = field.plane(Plane::FlowAccum);
    const double thresholdCells = channelThresholdKm2 * 1.0e6 / cellArea;
    // 30 m-EQUIVALENT sampling: the band is quoted at that resolution, and measuring at this
    // field's own 16 m would bias it high and make the comparison meaningless.
    const int stride = std::max(1, static_cast<int>(std::lround(30.0 / field.geometry().cell_size)));
    std::size_t channels = 0;
    std::size_t sampled = 0;
    for (std::int32_t cz = 0; cz < field.cells(); cz += stride) {
        for (std::int32_t cx = 0; cx < field.cells(); cx += stride) {
            channels += acc[field.index(cx, cz)] >= thresholdCells ? 1u : 0u;
            ++sampled;
        }
    }
    if (sampled == 0) {
        return 0.0;
    }
    const double sampleSize = static_cast<double>(stride) * field.geometry().cell_size;
    const double channelLengthKm = static_cast<double>(channels) * sampleSize / 1000.0;
    const double areaKm2 = static_cast<double>(sampled) * sampleSize * sampleSize / 1.0e6;
    return channelLengthKm / std::max(areaKm2, 1e-9);
}

// --------------------------------------------------------------- §9.5 valley cross-sections

ValleyStats valley_cross_sections(const TerrainField& field, const FlowNetwork& net,
                                  double channelThresholdKm2) {
    ValleyStats out;
    const std::span<const float> h = field.plane(Plane::Elevation);
    const std::span<const float> acc = field.plane(Plane::FlowAccum);
    const double thresholdCells = channelThresholdKm2 * 1.0e6 / field.geometry().cell_area();
    const std::int32_t cols = field.cells();
    constexpr std::int32_t kMaxReach = 24; // cells to walk before giving up on finding a ridge

    std::vector<double> exponents;
    std::vector<double> vIndices;

    for (std::int32_t cz = kMaxReach; cz + kMaxReach < cols; cz += 3) {
        for (std::int32_t cx = kMaxReach; cx + kMaxReach < cols; cx += 3) {
            const std::size_t i = field.index(cx, cz);
            if (acc[i] < thresholdCells) {
                continue;
            }
            // The transect runs PERPENDICULAR to the flow direction, so it crosses the valley
            // rather than running along it.
            const std::uint32_t r = net.receiver[i];
            if (r == i) {
                continue;
            }
            const auto rx = static_cast<std::int32_t>(r % static_cast<std::uint32_t>(cols));
            const auto rz = static_cast<std::int32_t>(r / static_cast<std::uint32_t>(cols));
            const std::int32_t fx = rx - cx;
            const std::int32_t fz = rz - cz;
            if (fx == 0 && fz == 0) {
                continue;
            }
            const std::int32_t px = -fz; // perpendicular
            const std::int32_t pz = fx;

            // Walk both ways to the first local maximum: the two ridges.
            const auto walk = [&](std::int32_t dx, std::int32_t dz, std::vector<double>& xs,
                                  std::vector<double>& ys) {
                double previous = h[i];
                for (std::int32_t s = 1; s <= kMaxReach; ++s) {
                    const std::int32_t sx = cx + dx * s;
                    const std::int32_t sz = cz + dz * s;
                    if (!field.in_bounds(sx, sz)) {
                        return false;
                    }
                    const double z = h[field.index(sx, sz)];
                    if (z < previous) {
                        // Over the crest and descending the far side: the ridge was the last cell.
                        return s > 2;
                    }
                    previous = z;
                    const double dist = static_cast<double>(s) * field.geometry().cell_size *
                                        std::sqrt(static_cast<double>(dx * dx + dz * dz));
                    xs.push_back(dist);
                    ys.push_back(z - h[i]);
                }
                return true;
            };

            std::vector<double> xs;
            std::vector<double> ys;
            const bool a = walk(px, pz, xs, ys);
            const bool b = walk(-px, -pz, xs, ys);
            if (!a || !b || xs.size() < 6) {
                continue;
            }
            // Fit y = a x^b in log-log. Points at zero relief carry no information about the
            // exponent and would take the log of zero.
            std::vector<double> lx;
            std::vector<double> ly;
            for (std::size_t k = 0; k < xs.size(); ++k) {
                if (ys[k] > 1e-3 && xs[k] > 1e-6) {
                    lx.push_back(std::log(xs[k]));
                    ly.push_back(std::log(ys[k]));
                }
            }
            if (lx.size() < 5) {
                continue;
            }
            const Fit fit = least_squares(lx, ly);
            if (!(fit.slope > 0.0 && fit.slope < 6.0) || fit.r_squared < 0.5) {
                continue;
            }
            exponents.push_back(fit.slope);

            // V-index: cross-sectional area of the actual section over the area of the V joining
            // its two endpoints, minus one. 0 is a perfect V, positive is a U.
            const double maxRelief = *std::max_element(ys.begin(), ys.end());
            const double maxDist = *std::max_element(xs.begin(), xs.end());
            double area = 0.0;
            for (std::size_t k = 0; k < xs.size(); ++k) {
                area += (maxRelief - ys[k]) * field.geometry().cell_size;
            }
            const double vArea = maxRelief * maxDist; // two triangles of half that each side
            if (vArea > 1e-9) {
                vIndices.push_back(area / vArea - 1.0);
            }
        }
    }

    out.transects = exponents.size();
    if (exponents.empty()) {
        return out;
    }
    out.mean_exponent =
        std::accumulate(exponents.begin(), exponents.end(), 0.0) / static_cast<double>(exponents.size());
    out.median_exponent = percentile(exponents, 0.5);
    if (!vIndices.empty()) {
        out.mean_v_index =
            std::accumulate(vIndices.begin(), vIndices.end(), 0.0) / static_cast<double>(vIndices.size());
    }
    return out;
}

// ------------------------------------------------------------------------ §9.6 constant drop

DropTest constant_drop(const TerrainField& field, const FlowNetwork& net, double channelThresholdKm2) {
    DropTest out;
    const std::span<const float> h = field.plane(Plane::Elevation);
    const std::span<const float> acc = field.plane(Plane::FlowAccum);
    const double thresholdCells = channelThresholdKm2 * 1.0e6 / field.geometry().cell_area();
    const std::vector<std::uint8_t> order =
        field::strahler_order(field, net, static_cast<float>(channelThresholdKm2));

    std::vector<double> first;
    std::vector<double> higher;
    for (std::size_t i = 0; i < net.receiver.size(); ++i) {
        if (acc[i] < thresholdCells || order[i] == 0) {
            continue;
        }
        const std::uint32_t r = net.receiver[i];
        // PER-CELL drop, and the same way for both orders -- which is what the t-test needs. A
        // per-link version would need a donor index to walk runs upstream, a second data structure
        // for no change in what the statistic can detect.
        const double drop = static_cast<double>(h[i]) - (r == i ? h[i] : h[r]);
        (order[i] == 1 ? first : higher).push_back(drop);
    }
    out.first_cells = first.size();
    out.higher_cells = higher.size();
    if (first.size() < 2 || higher.size() < 2) {
        return out;
    }
    const auto stats = [](const std::vector<double>& v) {
        const double mean = std::accumulate(v.begin(), v.end(), 0.0) / static_cast<double>(v.size());
        double var = 0.0;
        for (const double x : v) {
            var += (x - mean) * (x - mean);
        }
        return std::pair<double, double>{mean, var / static_cast<double>(v.size() - 1)};
    };
    const auto [m1, v1] = stats(first);
    const auto [m2, v2] = stats(higher);
    out.first_order_mean = m1;
    out.higher_order_mean = m2;
    const double se =
        std::sqrt(v1 / static_cast<double>(first.size()) + v2 / static_cast<double>(higher.size()));
    out.t_statistic = se > 1e-12 ? (m1 - m2) / se : 0.0;
    return out;
}

// --------------------------------------------------------------------------- §9.7 slope-area

SlopeAreaFit slope_area(const TerrainField& field, const FlowNetwork& net, double channelThresholdKm2,
                        float seaLevel) {
    SlopeAreaFit out;
    const std::span<const float> h = field.plane(Plane::Elevation);
    const std::span<const float> acc = field.plane(Plane::FlowAccum);
    const double cellArea = field.geometry().cell_area();
    const std::int32_t cols = field.cells();

    // Bin by log10(area) and take the MEDIAN slope per bin. The median rather than the mean because
    // the slope distribution within a bin is long-tailed and a handful of cliff cells would drag a
    // mean bin far off the trend the fit is looking for.
    constexpr std::size_t kBins = 24;
    std::vector<std::vector<double>> bins(kBins);
    double minLog = 1e30;
    double maxLog = -1e30;
    for (std::size_t i = 0; i < net.receiver.size(); ++i) {
        const double area = static_cast<double>(acc[i]) * cellArea;
        if (area <= 0.0) {
            continue;
        }
        minLog = std::min(minLog, std::log10(area));
        maxLog = std::max(maxLog, std::log10(area));
    }
    if (!(maxLog > minLog)) {
        return out;
    }
    for (std::size_t i = 0; i < net.receiver.size(); ++i) {
        const auto cx = static_cast<std::int32_t>(i % static_cast<std::size_t>(cols));
        const auto cz = static_cast<std::int32_t>(i / static_cast<std::size_t>(cols));
        if (cx == 0 || cz == 0 || cx + 1 >= cols || cz + 1 >= cols) {
            continue;
        }
        const std::uint32_t r = net.receiver[i];
        if (r == i) {
            continue;
        }
        // SUBAERIAL ONLY. With the sea floor in the fit, the ocean owns every high-area bin at
        // near-zero slope and the regression reads R² = 0.001 -- no relationship at all, which is
        // the correct answer to the wrong question. The stream-power law is about land.
        if (h[i] <= seaLevel || h[r] <= seaLevel) {
            continue;
        }
        const double area = static_cast<double>(acc[i]) * cellArea;
        if (area <= 0.0) {
            continue;
        }
        // Slope ALONG THE FLOW PATH, which is what the stream-power law is written in terms of --
        // not the surface gradient magnitude, which on a channel cell includes the valley walls.
        const auto rx = static_cast<std::int32_t>(r % static_cast<std::uint32_t>(cols));
        const auto rz = static_cast<std::int32_t>(r / static_cast<std::uint32_t>(cols));
        const double run = std::sqrt(static_cast<double>((rx - cx) * (rx - cx) + (rz - cz) * (rz - cz))) *
                           field.geometry().cell_size;
        if (run < 1e-6) {
            continue;
        }
        const double slope = (static_cast<double>(h[i]) - h[r]) / run;
        if (slope <= 1e-9) {
            continue;
        }
        const auto b =
            std::min(kBins - 1, static_cast<std::size_t>((std::log10(area) - minLog) / (maxLog - minLog) *
                                                         static_cast<double>(kBins)));
        bins[b].push_back(slope);
    }

    std::vector<double> lx;
    std::vector<double> ly;
    std::vector<double> hillslopeSlopes;
    const double thresholdArea = channelThresholdKm2 * 1.0e6;
    for (std::size_t b = 0; b < kBins; ++b) {
        if (bins[b].size() < 20) {
            continue;
        }
        const double centreLog =
            minLog + (static_cast<double>(b) + 0.5) / static_cast<double>(kBins) * (maxLog - minLog);
        const double median = percentile(bins[b], 0.5);
        if (std::pow(10.0, centreLog) >= thresholdArea) {
            lx.push_back(centreLog * std::log(10.0));
            ly.push_back(std::log(median));
        } else {
            hillslopeSlopes.push_back(median);
        }
    }
    out.bins = lx.size();
    const Fit fit = least_squares(lx, ly);
    out.exponent = fit.slope;
    out.r_squared = fit.r_squared;

    // §9.7's second half: below the channelization threshold, slope must be roughly INDEPENDENT of
    // area -- the hillslope plateau. "Its absence means your diffusion term is off or missing."
    if (hillslopeSlopes.size() >= 3) {
        const auto [lo, hi] = std::minmax_element(hillslopeSlopes.begin(), hillslopeSlopes.end());
        out.hillslope_plateau = *hi < *lo * 3.0; // under half a decade of variation across the plateau
    }
    return out;
}

// ---------------------------------------------------------------------------- §9.8 variogram

VariogramStats variogram(const TerrainField& field, bool landOnly, float seaLevel) {
    VariogramStats out;
    const std::span<const float> h = field.plane(Plane::Elevation);
    const std::int32_t cols = field.cells();
    const auto usable = [&](std::int32_t cx, std::int32_t cz) {
        return !landOnly || h[field.index(cx, cz)] > seaLevel;
    };
    std::vector<double> lx;
    std::vector<double> ly;
    for (std::int32_t lag = 1; lag <= cols / 8; lag *= 2) {
        double sum = 0.0;
        std::size_t pairs = 0;
        for (std::int32_t cz = 0; cz < cols; cz += 2) {
            for (std::int32_t cx = 0; cx + lag < cols; cx += 2) {
                if (!usable(cx, cz) || !usable(cx + lag, cz)) {
                    continue;
                }
                const double d = static_cast<double>(h[field.index(cx + lag, cz)]) - h[field.index(cx, cz)];
                sum += d * d;
                ++pairs;
            }
        }
        for (std::int32_t cz = 0; cz + lag < cols; cz += 2) {
            for (std::int32_t cx = 0; cx < cols; cx += 2) {
                if (!usable(cx, cz) || !usable(cx, cz + lag)) {
                    continue;
                }
                const double d = static_cast<double>(h[field.index(cx, cz + lag)]) - h[field.index(cx, cz)];
                sum += d * d;
                ++pairs;
            }
        }
        if (pairs == 0) {
            continue;
        }
        const double gamma = 0.5 * sum / static_cast<double>(pairs);
        if (gamma > 1e-12) {
            lx.push_back(std::log(static_cast<double>(lag)));
            ly.push_back(std::log(gamma));
        }
    }
    out.lags = lx.size();
    const Fit fit = least_squares(lx, ly);
    // gamma(h) ~ h^(2H), so H is half the log-log slope.
    out.hurst = 0.5 * fit.slope;
    out.r_squared = fit.r_squared;
    return out;
}

// ------------------------------------------------------------------------- §9.9 coherence

Coherence hydrological_coherence(const TerrainField& field, const RiverNetwork& rivers) {
    Coherence out;
    const std::span<const float> h = field.plane(Plane::Elevation);
    // An internal basin is a non-boundary cell with no strictly lower neighbour. Priority-flood
    // guarantees zero BY CONSTRUCTION, which is exactly why it is worth asserting: a construction
    // guarantee that is never checked is a comment.
    for (std::int32_t cz = 1; cz + 1 < field.cells(); ++cz) {
        for (std::int32_t cx = 1; cx + 1 < field.cells(); ++cx) {
            const double z = h[field.index(cx, cz)];
            bool hasLower = false;
            for (std::int32_t dz = -1; dz <= 1 && !hasLower; ++dz) {
                for (std::int32_t dx = -1; dx <= 1 && !hasLower; ++dx) {
                    if (dx == 0 && dz == 0) {
                        continue;
                    }
                    hasLower = h[field.index(cx + dx, cz + dz)] < z;
                }
            }
            out.internal_basins += hasLower ? 0u : 1u;
        }
    }
    out.reaches = rivers.reaches.size();
    for (const field::RiverReach& r : rivers.reaches) {
        const bool ok = r.terminus != field::RiverReach::Terminus::Lake ||
                        (r.lake >= 0 && static_cast<std::size_t>(r.lake) < rivers.lakes.size() &&
                         rivers.lakes[static_cast<std::size_t>(r.lake)].has_spill_path);
        out.reaches_terminating += ok ? 1u : 0u;
    }
    out.lakes = rivers.lakes.size();
    for (const field::Lake& l : rivers.lakes) {
        out.lakes_spilling += l.has_spill_path ? 1u : 0u;
    }
    return out;
}

// ----------------------------------------------------------------------------- the suite

bool SuiteResult::all_passed() const noexcept {
    return failures() == 0;
}

std::size_t SuiteResult::failures() const noexcept {
    std::size_t n = 0;
    for (const MetricResult& m : metrics) {
        n += (m.applicable && !m.passed) ? 1u : 0u;
    }
    return n;
}

SuiteResult run_suite(const TerrainField& field, const FlowNetwork& net, const AcceptanceInputs& in) {
    SuiteResult out;
    const auto add = [&](std::string name, double value, Band band, std::string note, bool applicable = true,
                         bool overridePass = false, bool passValue = false) {
        MetricResult m;
        m.name = std::move(name);
        m.value = value;
        m.band = band;
        m.applicable = applicable;
        m.passed = overridePass ? passValue : band.contains(value);
        m.note = std::move(note);
        out.metrics.push_back(std::move(m));
    };
    char buf[512];

    // The SHAPE tests read the final surface when one was supplied; the NETWORK tests always read
    // the macro field, because flow routing is a macro concept and the detail term is not routed
    // through. See AcceptanceInputs::final_surface for what running the shape tests on the wrong
    // one measured.
    const TerrainField& surface = in.final_surface != nullptr ? *in.final_surface : field;
    const char* surfaceName = in.final_surface != nullptr ? "final heights" : "macro field";

    const SlopeStats slope = slope_distribution(surface, true, in.sea_level);
    std::snprintf(buf, sizeof(buf), "on %s: mean slope %.1f deg over %zu land cells, %zu modes, unimodal=%s",
                  surfaceName, slope.mean_slope_deg, slope.samples, slope.modes,
                  slope.unimodal ? "yes" : "NO");
    // §9.1's testable claim is the SIGN of the skew against mean slope, plus unimodality -- not a
    // magnitude. See kSlopeSkewnessLowMean.
    const Band skewBand =
        slope.mean_slope_deg < kSlopeSkewPivotDeg ? kSlopeSkewnessLowMean : kSlopeSkewnessHighMean;
    add("1 slope skew sign", slope.skewness, skewBand, buf, slope.samples > 100, true,
        skewBand.contains(slope.skewness) && slope.unimodal);

    const SpectrumStats spec = power_spectrum(surface);
    std::snprintf(buf, sizeof(buf), "on %s: R2 %.3f, spurious peak=%s", surfaceName, spec.r_squared,
                  spec.spurious_peak ? "YES at " : "no");
    std::string specNote = buf;
    if (spec.spurious_peak) {
        std::snprintf(buf, sizeof(buf), "%.1f cells", spec.peak_wavelength_cells);
        specNote += buf;
    }
    add("2 spectral beta", spec.beta, kSpectralBeta, specNote, true, true,
        kSpectralBeta.contains(spec.beta) && !spec.spurious_peak);

    const Hypsometry hyp = hypsometry(surface);
    std::snprintf(buf, sizeof(buf),
                  "land %.1f%% (no band: planetary stat), median %.1f m, max %.1f m, HI %.3f",
                  100.0 * hyp.land_fraction, hyp.land_median_m, hyp.land_max_m, hyp.hypsometric_integral);
    add("3 hypsometry median/max", hyp.median_over_max, kHypsometricMedianOverMax, buf);

    const double density = drainage_density(field, in.channel_threshold_km2);
    std::snprintf(buf, sizeof(buf), "at A_c = %.3f km^2, 30 m-equivalent sampling", in.channel_threshold_km2);
    add("4 drainage density", density, kDrainageDensityKmPerKm2, buf);

    const ValleyStats valley = valley_cross_sections(field, net, in.channel_threshold_km2);
    // Test 5 stays on the macro field: it needs the flow network to find the channels its
    // transects cross, and the network is a macro object.
    std::snprintf(buf, sizeof(buf), "median b %.2f, V-index %.2f, over %zu transects", valley.median_exponent,
                  valley.mean_v_index, valley.transects);
    add("5 valley exponent b", valley.mean_exponent,
        in.expect_glacial_valleys ? kValleyExponentGlacial : kValleyExponentFluvial, buf,
        valley.transects >= 20);

    const DropTest drop = constant_drop(field, net, in.channel_threshold_km2);
    std::snprintf(buf, sizeof(buf), "first %.3f m (%zu cells), higher %.3f m (%zu)", drop.first_order_mean,
                  drop.first_cells, drop.higher_order_mean, drop.higher_cells);
    add("6 constant-drop |t|", std::abs(drop.t_statistic), kConstantDropT, buf,
        drop.first_cells >= 2 && drop.higher_cells >= 2);

    const SlopeAreaFit sa = slope_area(field, net, in.channel_threshold_km2, in.sea_level);
    std::snprintf(buf, sizeof(buf), "R2 %.3f over %zu bins, hillslope plateau=%s", sa.r_squared, sa.bins,
                  sa.hillslope_plateau ? "yes" : "NO");
    add("7 slope-area exponent", sa.exponent, kSlopeAreaExponent, buf, sa.bins >= 3, true,
        kSlopeAreaExponent.contains(sa.exponent) && sa.hillslope_plateau);

    const VariogramStats vg = variogram(surface, true, in.sea_level);
    std::snprintf(buf, sizeof(buf), "on %s: log-log R2 %.3f over %zu lags", surfaceName, vg.r_squared,
                  vg.lags);
    add("8 variogram Hurst", vg.hurst, kVariogramHurst, buf);

    const Coherence coh = hydrological_coherence(field, in.rivers);
    std::snprintf(buf, sizeof(buf), "%zu internal basins, %zu/%zu reaches terminate, %zu/%zu lakes spill",
                  coh.internal_basins, coh.reaches_terminating, coh.reaches, coh.lakes_spilling, coh.lakes);
    add("9 hydrological coherence", static_cast<double>(coh.internal_basins), Band{0.0, 0.0, "Part 7 §9.9"},
        buf, true, true, coh.passed());

    // §9.10 is INAPPLICABLE rather than passing until vegetation reads the biome field (goal 316).
    // A suite that silently scores an unimplemented system as green is worse than an honest gap.
    add("10 stems per hectare", in.stems_per_hectare, kStemsPerHectareTemperate,
        in.stems_per_hectare < 0.0 ? "not measured -- vegetation does not read the biome field yet (goal 316)"
                                   : "temperate biome",
        in.stems_per_hectare >= 0.0);
    return out;
}

std::string format_report(const SuiteResult& r) {
    std::string out;
    char line[768];
    for (const MetricResult& m : r.metrics) {
        if (!m.applicable) {
            std::snprintf(line, sizeof(line), "  %-26s     n/a   %s\n", m.name.c_str(), m.note.c_str());
        } else {
            std::snprintf(line, sizeof(line), "  %-26s %8.3f  [%.2f, %.2f] %-4s  %s\n", m.name.c_str(),
                          m.value, m.band.lo, m.band.hi, m.passed ? "PASS" : "FAIL", m.note.c_str());
        }
        out += line;
        std::snprintf(line, sizeof(line), "  %-26s           %s\n", "", std::string(m.band.citation).c_str());
        out += line;
    }
    std::snprintf(line, sizeof(line), "  %zu of %zu applicable metrics failed\n", r.failures(),
                  r.metrics.size());
    out += line;
    return out;
}

} // namespace world::generation::validation
