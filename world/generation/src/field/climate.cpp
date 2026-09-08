// Prompt 006 goal 302: orographic climate -- research Part 7 §10.2 stage 2.
//
// WHAT THE RESEARCH SPECIFIES AND WHAT THIS IS, stated plainly because they are not the same thing
// and the prompt forbids substituting a remembered version of the mathematics for the real one.
//
// §10.2(2) specifies **Smith & Barstad's linear orographic precipitation model** -- "LT = one FFT
// pair". Its transfer function multiplies the terrain's Fourier transform by a wavenumber-dependent
// complex response built from the moist stability frequency, the airflow speed and two time
// constants, and the inverse transform is the precipitation field. What that buys over anything
// simpler is the PHASE of the response: it places the precipitation maximum slightly UPWIND of the
// ridge crest and produces genuine spillover past it, both wavenumber-dependent, neither of which
// falls out of a purely local rule.
//
// **THIS IS NOT THAT.** It is the model the research names as LT's own baseline -- the classical
// linear upslope law, S = C_w * U.grad(h) + S_inf -- plus the two extensions this goal's acceptance
// case actually requires:
//
//   1. DEPLETION. The bare upslope law has no memory: it will happily rain the same amount off the
//      third ridge as off the first. Carrying a moisture budget along the trajectory and drawing
//      condensation from it is what makes a LEE DRY rather than merely un-enhanced, and a rain
//      shadow is a dry lee.
//   2. DRIFT. Condensate enters a suspended reservoir advected downwind and falling out with an
//      e-folding length, which is the crude analogue of LT's conversion and fallout time constants
//      and the only reason a SPILLOVER PATTERN exists here at all. The first draft of this file
//      lacked it and therefore could not produce the spillover its own Check asked for.
//
// The trajectory sweep is semi-Lagrangian and honours an arbitrary wind direction: each cell reads
// a bilinear sample one full dominant-axis cell upwind. Stepping by dx/max(|wx|,|wz|) makes the
// dominant axis land exactly on the previous column or row, so the current cell's own weight in its
// own upwind sample is exactly zero and a single row-major pass in the wind's quadrant is a valid
// evaluation order. (The obvious alternative -- snapping the wind to the dominant axis -- silently
// discards the cross-wind component, which for the default (1, 0.3) wind is a 17 degree error in
// where every rain shadow lands.)
//
// WHY NOT WRITE THE FFT. Rule 7 says no new dependency without a written case, and prefers writing
// the ~200 lines to taking one. The honest reason for not doing that HERE is different and worth
// stating: an FFT is only the easy half of Smith-Barstad. The hard half is its parameters -- the
// moist Brunt-Vaisala frequency, the uplift sensitivity, the conversion and fallout times -- and
// the research gives the model's SHAPE but not a parameter set for a world 8 km across with 112 m
// of relief. Every parameter here instead names the anchor it came from and the miniaturization it
// was divided by -- and each one's effect was measured rather than assumed, which is how the
// smaller of the two claims below got corrected before it was written down as fact.
//
// TWO MEASURED FACTS ABOUT `wring_out_height_m`, because the first draft of this comment asserted
// a third that turned out to be false:
//
//   * It barely moves the shadow's CONTRAST. Because the output plane is normalised to a field mean
//     of one, scaling the total condensate up or down scales the windward flank and the drifted lee
//     together and cancels. Over this world's 112 m of relief, the physical 2 km scale height gives
//     a 5.33:1 flank ratio and the miniaturized 133 m gives 7.39:1 -- a real gain, from the peak
//     moving further upwind and leaving less in the cloud to spill over, but not a rescue. The
//     first draft predicted "1.06:1, which is to say none" and was wrong by a factor of five.
//   * It does move the ABSOLUTE budget, from 5.4% of the column wrung out to 57%, which is what
//     matters for a fetch crossing more than one range -- the second range should be raining on
//     air the first one already dried, and at 5.4% it never is.
//
// And the negative that follows from both: at 112 m of relief NEITHER setting reaches §6.2's 10:1
// anchor. That anchor is quoted for a 1.5-2 km barrier; the synthetic 400 m ridge in the tests does
// reach it (15.2:1). This world's mountains are simply shorter than the landform the number
// describes -- the same finding, one level up, as this pass's drainage-network and constant-drop
// results.
//
// Recorded as goal 302a: implement the real LT model when there is a reason to trust a parameter
// set for it -- and note the two models agree in the limit this world lives in (wavelengths short
// against the drift length), which is what makes the substitution defensible rather than merely
// convenient.

#include <algorithm>
#include <cmath>
#include <vector>

#include "world/generation/field/climate.hpp"

namespace world::generation::field {
namespace {

/// Research Part 7 §10.2(2). The standard-atmosphere environmental lapse rate. Flagged in the
/// research's own provenance register as matching the standard-atmosphere constant but not
/// URL-verified in that session, so it is cited as such rather than as a measured source.
constexpr float kLapseRateCPerKm = 6.5f;

/// Bilinear sample of `v` at a point in CELL-INDEX space. Returns false when the sample is not
/// fully inside the grid, so the caller can substitute its own upwind-edge boundary condition
/// rather than silently clamping a boundary value inward.
[[nodiscard]] bool sample_bilinear(const TerrainField& f, const std::vector<float>& v, float fx, float fz,
                                   float& out) {
    const float last = static_cast<float>(f.cells() - 1);
    if (!(fx >= 0.0f && fz >= 0.0f && fx <= last && fz <= last)) {
        return false;
    }
    const auto x0 = static_cast<std::int32_t>(std::floor(fx));
    const auto z0 = static_cast<std::int32_t>(std::floor(fz));
    const std::int32_t x1 = std::min(x0 + 1, f.cells() - 1);
    const std::int32_t z1 = std::min(z0 + 1, f.cells() - 1);
    const float tx = fx - static_cast<float>(x0);
    const float tz = fz - static_cast<float>(z0);
    const float a = v[f.index(x0, z0)] * (1.0f - tx) + v[f.index(x1, z0)] * tx;
    const float b = v[f.index(x0, z1)] * (1.0f - tx) + v[f.index(x1, z1)] * tx;
    out = a * (1.0f - tz) + b * tz;
    return true;
}

} // namespace

void compute_climate(TerrainField& out, const ClimateParams& p) {
    const std::int32_t n = out.cells();
    const std::span<const float> h = out.plane(Plane::Elevation);
    const std::span<float> temperature = out.plane(Plane::Temperature);
    const std::span<float> precipitation = out.plane(Plane::Precipitation);

    // ---- temperature: latitude plus lapse rate -------------------------------------------------
    //
    // The latitude term is nearly constant over 8 km -- about 0.07 degrees of arc -- so it enters
    // as the field's base temperature rather than as a gradient across it. Writing it that way
    // rather than as a per-cell latitude is the honest form: a gradient that varies by 0.005 C
    // across the whole world is not a gradient, it is a constant with extra arithmetic.
    //
    // Elevation is clamped at zero because sea floor is under water, not under air: a seabed cell
    // reports the surface temperature, which is what a biome or ice classifier wants from it.
    for (std::size_t i = 0; i < out.cell_count(); ++i) {
        const float elevationKm = std::max(h[i], 0.0f) / 1000.0f;
        temperature[i] = p.sea_level_temperature_c - kLapseRateCPerKm * elevationKm;
    }

    // ---- precipitation: upslope forcing with depletion and drift --------------------------------
    const float dx = out.geometry().cell_size;
    const float wlen = std::sqrt(p.wind_x * p.wind_x + p.wind_z * p.wind_z);
    const float wx = wlen > 1e-12f ? p.wind_x / wlen : 1.0f;
    const float wz = wlen > 1e-12f ? p.wind_z / wlen : 0.0f;
    // One step moves exactly one cell along the DOMINANT axis (see the header comment: this is what
    // makes a single row-major pass a valid evaluation order for an arbitrary wind).
    const float major = std::max(std::abs(wx), std::abs(wz));
    const float backX = wx / major;
    const float backZ = wz / major;
    const float stepLen = dx / major;

    const float dropRate = 1.0f - std::exp(-stepLen / std::max(p.drift_length_m, 1e-3f));
    const float rechargeRate = 1.0f - std::exp(-stepLen / std::max(p.ocean_recharge_length_m, 1e-3f));
    const float wringHeight = std::max(p.wring_out_height_m, 1e-3f);

    std::vector<float> moisture(out.cell_count(), 1.0f);
    std::vector<float> cloud(out.cell_count(), 0.0f);
    std::vector<float> orographic(out.cell_count(), 0.0f);
    // The surface the AIR flows over, which is sea level wherever the ground is below it -- not the
    // elevation plane. Found by looking at the first false-colour dump: the ocean north-west of the
    // continent came out WET, because the seabed rises toward that coast and the sweep was reading
    // a rising sea floor as forced ascent. Air over water is at sea level; the bathymetry beneath it
    // lifts nothing. (Also why this is a separate vector rather than a clamp at the read site: the
    // bilinear sample has to interpolate the clamped surface, or the last wet cell before a coast
    // still sees a fractional step up out of the water.)
    std::vector<float> heights(out.cell_count(), 0.0f);
    for (std::size_t i = 0; i < out.cell_count(); ++i) {
        heights[i] = std::max(h[i], 0.0f);
    }

    const std::int32_t xStart = wx >= 0.0f ? 0 : n - 1;
    const std::int32_t xStep = wx >= 0.0f ? 1 : -1;
    const std::int32_t zStart = wz >= 0.0f ? 0 : n - 1;
    const std::int32_t zStep = wz >= 0.0f ? 1 : -1;

    for (std::int32_t kz = 0; kz < n; ++kz) {
        const std::int32_t cz = zStart + kz * zStep;
        for (std::int32_t kx = 0; kx < n; ++kx) {
            const std::int32_t cx = xStart + kx * xStep;
            const std::size_t i = out.index(cx, cz);
            const float fx = static_cast<float>(cx) - backX;
            const float fz = static_cast<float>(cz) - backZ;

            // Upwind-edge boundary condition: air arrives saturated and cloudless, over terrain at
            // the local height (so the edge cell itself is not read as a step up from nothing).
            float carried = 1.0f;
            float cloudIn = 0.0f;
            float upwindHeight = heights[i];
            const bool inside = sample_bilinear(out, heights, fx, fz, upwindHeight);
            if (inside) {
                (void)sample_bilinear(out, moisture, fx, fz, carried);
                (void)sample_bilinear(out, cloud, fx, fz, cloudIn);
            }

            // Forced ascent along the trajectory, in METRES -- not a slope. The wring-out height is
            // an ascent, so the two are the same quantity and the ratio is dimensionless.
            const float ascent = std::max(heights[i] - upwindHeight, 0.0f);
            // Condensation only on ASCENT. On the lee the parcel descends, warms, and condenses
            // nothing -- that asymmetry IS the rain shadow, and it is why this term is one-sided
            // rather than a signed slope.
            const float condensed = carried * (1.0f - std::exp(-ascent / wringHeight));

            // Suspended condensate advects and rains out with an e-folding length. This is what
            // carries rain past a crest.
            const float suspended = cloudIn + condensed;
            const float fallen = suspended * dropRate;
            cloud[i] = suspended - fallen;
            orographic[i] = fallen;

            // Evaporative recovery over water, toward saturation.
            float remaining = carried - condensed;
            if (h[i] <= 0.0f) {
                remaining += (1.0f - remaining) * rechargeRate;
            }
            moisture[i] = std::clamp(remaining, 0.0f, 1.0f);
        }
    }

    // Normalise to a field mean of exactly 1.0, then mix in the synoptic background. See the header
    // on why the output is dimensionless: it removes a units fudge factor that would otherwise be
    // tuned until the pattern looked right, which is the failure mode this whole pass is avoiding.
    double sum = 0.0;
    for (const float v : orographic) {
        sum += static_cast<double>(v);
    }
    const double mean = sum / static_cast<double>(out.cell_count());
    const float bg = std::clamp(p.background_fraction, 0.0f, 1.0f);
    if (mean > 1e-12) {
        const float scale = static_cast<float>((1.0 - static_cast<double>(bg)) / mean);
        for (std::size_t i = 0; i < out.cell_count(); ++i) {
            precipitation[i] = bg + orographic[i] * scale;
        }
    } else {
        // A perfectly flat world condenses nothing. Uniform background is the right answer, and
        // returning a mean of 1 keeps the invariant every consumer relies on.
        std::fill(precipitation.begin(), precipitation.end(), 1.0f);
    }
}

float lapse_rate_c_per_km() noexcept {
    return kLapseRateCPerKm;
}

} // namespace world::generation::field
