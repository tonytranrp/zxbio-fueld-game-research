#pragma once

// Prompt 006 goal 317: research Part 7 §9's ten acceptance tests, as a library.
//
// The research's framing, which this module takes literally:
//
//   "A generator is a statistical hypothesis about terrain; test it like one. Each test is one
//    histogram or one curve, computed on a 512²-or-larger patch of final (post-pipeline) heights,
//    with the measured reference band and source."
//
// So every band below is a NAMED CONSTANT WITH ITS CITATION IN A COMMENT BESIDE IT. That is the
// whole point of the file: a threshold whose provenance lives somewhere else becomes a magic number
// within one refactor, and this pass has already found three places where a number had drifted from
// the reason it was chosen.
//
// Each metric is also a SEPARATE FREE FUNCTION, because goal 317's Check requires the instrument to
// be tested before its reading is trusted: `test_acceptance.cpp` feeds each one a synthetic input
// with an analytically known answer (a plane, a cone, a sine, a known-fractal surface) and asserts
// the metric recovers it. This pass has twice been misled by a metric rather than by the terrain
// (§9's moiré-style envelope separation, §10's chord-based wavelength), and an untested instrument
// is how that happens.

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "world/generation/field/fluvial.hpp"
#include "world/generation/field/rivers.hpp"
#include "world/generation/field/terrain_field.hpp"

namespace world::generation::validation {

using field::FlowNetwork;
using field::RiverNetwork;
using field::TerrainField;

/// An acceptance band and where it came from. `citation` is the research section plus the source
/// the research itself cites, so a reading can be argued with without leaving the file.
struct Band {
    double lo;
    double hi;
    std::string_view citation;

    [[nodiscard]] constexpr bool contains(double v) const noexcept { return v >= lo && v <= hi; }
};

// ---------------------------------------------------------------------------------------------
// The bands. Part 7 §9, one per test.
// ---------------------------------------------------------------------------------------------

/// §9.1. Real slope distributions are UNIMODAL, and skewness runs positive at low mean slope to
/// negative at high mean slope across >10,000 sampled US landscapes (Wolinsky & Pratson 2005,
/// https://doi.org/10.1130/g21296.1).
///
/// **THE RESEARCH GIVES NO NUMERIC SKEW BAND**, and a first version of this file invented one
/// ([-1.5, 2.5]) which the shipped terrain then failed at 3.24. An invented band that a real
/// landscape fails is worse than no band: it reports a defect that was never established.
///
/// What §9.1 actually states testably is (a) UNIMODALITY and (b) the SIGN of the skew against mean
/// slope. So the band below is on the sign only -- positive skew is required at a low mean slope,
/// which is the regime this world is in -- and the magnitude is reported without judgement.
inline constexpr Band kSlopeSkewnessLowMean{0.0, 1.0e9, "Part 7 §9.1 -- sign only; Wolinsky & Pratson [90]"};
inline constexpr Band kSlopeSkewnessHighMean{-1.0e9, 0.0, "Part 7 §9.1 -- sign only; Wolinsky & Pratson [90]"};
/// Where §9.1's skew-vs-mean-slope trend crosses zero. Wolinsky & Pratson's own transition is
/// gradual and they do not name a pivot; 15 degrees is this project's reading of "low" versus
/// "high" mean slope and is FLAGGED as such rather than cited.
inline constexpr double kSlopeSkewPivotDeg = 15.0;

/// §9.2. 1D angle-integrated spectral slope. "beta in [1.6, 2.5], target ~2" -- and beta = 2H + 1,
/// so beta = 2 is Brownian motion (H = 0.5).
inline constexpr Band kSpectralBeta{1.6, 2.5, "Part 7 §9.2 -- [1][3][4]; beta = 2H+1 (Turcotte)"};

/// §9.3. The LAND half of the hypsometric curve: the peak sits near sea level with a thinning tail,
/// so median/max is well below the 0.5 a Gaussian field would give.
///
/// NOTE the deliberate departure: §9.3 also asks for ~29% land, which is a WHOLE-EARTH statistic.
/// This field is 8 km across -- 1.3e-7 of the planet -- and a random 8 km patch of Earth is almost
/// entirely land or almost entirely ocean. Land fraction is REPORTED but has no band, because a
/// patch cannot express a planetary number. Argued in full in research/earth-terrain-pipeline-log.md §6.
inline constexpr Band kHypsometricMedianOverMax{0.0, 0.40, "Part 7 §9.3 -- [10][11], land half only"};

/// §9.4. Drainage density at 30 m-equivalent resolution. "This one test catches both 'no rivers'
/// (~0) and 'noise gullies everywhere' (>>12)."
inline constexpr Band kDrainageDensityKmPerKm2{2.0, 12.0, "Part 7 §9.4 -- [45][46][47][48]"};

/// §9.5. Valley cross-section exponent from y = a x^b on ridge-to-ridge transects: fluvial terrain
/// gives b ~ 1 (V), glacial b -> 1.5-2 (parabolic U; Graf's Beartooth 1.5-2.0, Svensson's Lapporten
/// 2.0-2.2). The band is the FLUVIAL one, because that is what this pipeline produces; goal 310's
/// glacial stencil is judged against the glacial band below.
inline constexpr Band kValleyExponentFluvial{0.7, 1.4, "Part 7 §9.5 -- [92][93][94], fluvial V"};
inline constexpr Band kValleyExponentGlacial{1.5, 2.2, "Part 7 §9.5 -- Graf, Svensson [92][93]"};

/// §9.6. Tarboton's constant-drop t-test, the same one TauDEM runs on real DEMs.
inline constexpr Band kConstantDropT{0.0, 2.0, "Part 7 §9.6 -- TauDEM criterion [43][44]"};

/// §9.7. Log-log channel slope against drainage area. The hillslope plateau below the channel head
/// must also exist -- "its absence means your diffusion term is off or missing".
inline constexpr Band kSlopeAreaExponent{-0.6, -0.35, "Part 7 §9.7 -- [28][96]"};

/// §9.8. Local Hurst exponent from the variogram, 0.46-0.77 by landform setting.
inline constexpr Band kVariogramHurst{0.46, 0.77, "Part 7 §9.8 -- [3]; pyTopoComplexity [97]"};

/// §9.10. Stems per hectare, temperate. Part 6 §8.1's measured band.
inline constexpr Band kStemsPerHectareTemperate{400.0, 700.0, "Part 7 §9.10 / Part 6 §8.1 -- [82][83]"};

// ---------------------------------------------------------------------------------------------
// Individual metrics. Each is separately testable against a synthetic with a known answer.
// ---------------------------------------------------------------------------------------------

struct SlopeStats {
    double mean_slope_deg = 0.0;
    double skewness = 0.0;
    bool unimodal = false;
    std::size_t modes = 0; ///< peaks in the smoothed histogram; >1 is the failure §9.1 warns about
    std::size_t samples = 0;
};

/// §9.1. Slope from central differences on the analytic cell spacing, in degrees.
///
/// LAND ONLY when `landOnly`, and that is the default for a reason found by running it: with the
/// seabed included this field's mean slope reads 1.7 degrees, because two thirds of the cells are a
/// smooth submarine surface. Every one of §9's tests is a statement about LANDSCAPES, and none of
/// the sources behind their bands measured a sea floor.
[[nodiscard]] SlopeStats slope_distribution(const TerrainField& field, bool landOnly = true,
                                            float seaLevel = 0.0f);

struct SpectrumStats {
    double beta = 0.0;                  ///< the fitted 1D spectral slope, positive by convention
    double r_squared = 0.0;             ///< how log-log-linear the spectrum actually is
    bool spurious_peak = false;         ///< §9.2: "assert no spurious peaks (periodicity)"
    double peak_wavelength_cells = 0.0; ///< where the worst peak is, when there is one
};

/// §9.2. 1D power spectrum, averaged over every row and column.
///
/// The 1D form rather than a 2D radial average is deliberate: the research's band is quoted for the
/// 1D angle-integrated slope and states beta = 2H + 1, which is the 1D convention. A 2D radially
/// averaged PSD of the same surface goes as k^-(2H+2) and would read one unit high against this
/// band -- a units error that would look like a real result.
[[nodiscard]] SpectrumStats power_spectrum(const TerrainField& field);

struct Hypsometry {
    double land_fraction = 0.0;
    double median_over_max = 0.0;
    double hypsometric_integral = 0.0; ///< (mean - min) / (max - min), Pike & Wilson's estimator
    double land_median_m = 0.0;
    double land_max_m = 0.0;
};

/// §9.3. The land half of the area-elevation distribution.
[[nodiscard]] Hypsometry hypsometry(const TerrainField& field);

/// §9.4. Channel length per unit area, extracted at 30 m-EQUIVALENT resolution regardless of the
/// field's own cell size -- the band is quoted at that resolution, and measuring at 16 m would bias
/// it high and make the comparison meaningless.
[[nodiscard]] double drainage_density(const TerrainField& field, double channelThresholdKm2);

struct ValleyStats {
    double mean_exponent = 0.0; ///< b in y = a x^b
    double median_exponent = 0.0;
    double mean_v_index = 0.0; ///< A_x/A_v - 1: 0 is a perfect V, > 0 is a U
    std::size_t transects = 0;
};

/// §9.5. Ridge-to-ridge transects perpendicular to extracted channels, fitted in log-log.
[[nodiscard]] ValleyStats valley_cross_sections(const TerrainField& field, const FlowNetwork& net,
                                                double channelThresholdKm2);

struct DropTest {
    double t_statistic = 0.0;
    double first_order_mean = 0.0;
    double higher_order_mean = 0.0;
    std::size_t first_cells = 0;
    std::size_t higher_cells = 0;
};

/// §9.6. Tarboton's constant-drop property.
[[nodiscard]] DropTest constant_drop(const TerrainField& field, const FlowNetwork& net,
                                     double channelThresholdKm2);

struct SlopeAreaFit {
    double exponent = 0.0;
    double r_squared = 0.0;
    bool hillslope_plateau = false; ///< §9.7: its absence "means your diffusion term is off"
    std::size_t bins = 0;
};

/// §9.7. Channel slope against drainage area, fitted over the channelled bins only.
///
/// SUBMARINE CELLS ARE EXCLUDED. The stream-power law is a statement about subaerial channels, and
/// with the sea floor in the fit this field's slope-area relation read R² = 0.001 -- no relationship
/// at all -- because the ocean carries the largest contributing areas at near-zero slope and owns
/// the entire high-area end of the regression.
[[nodiscard]] SlopeAreaFit slope_area(const TerrainField& field, const FlowNetwork& net,
                                      double channelThresholdKm2, float seaLevel = 0.0f);

struct VariogramStats {
    double hurst = 0.0;
    double r_squared = 0.0; ///< §9.8 asks for log-log LINEARITY, so this is half the test
    std::size_t lags = 0;
};

/// §9.8. Semivariogram over a log-spaced lag ladder; H is half the log-log slope. Land-only for the
/// same reason as §9.1: a smooth sea floor is not a landform with a Hurst exponent.
[[nodiscard]] VariogramStats variogram(const TerrainField& field, bool landOnly = true,
                                       float seaLevel = 0.0f);

struct Coherence {
    std::size_t internal_basins = 0;
    std::size_t reaches = 0;
    std::size_t reaches_terminating = 0;
    std::size_t lakes = 0;
    std::size_t lakes_spilling = 0;
    [[nodiscard]] bool passed() const noexcept {
        return internal_basins == 0 && reaches == reaches_terminating && lakes == lakes_spilling;
    }
};

/// §9.9. The structural test: no internal basins, every river ends somewhere, every lake spills.
/// Pass an empty network to check only the basin count.
[[nodiscard]] Coherence hydrological_coherence(const TerrainField& field, const RiverNetwork& rivers);

// ---------------------------------------------------------------------------------------------
// The suite
// ---------------------------------------------------------------------------------------------

struct MetricResult {
    std::string name;
    double value = 0.0;
    Band band{0.0, 0.0, ""};
    bool applicable = true; ///< false when the subject does not exist yet (see stems/ha)
    bool passed = false;
    std::string note; ///< the supporting numbers, so a failure is diagnosable from the report alone
};

struct SuiteResult {
    std::vector<MetricResult> metrics;
    [[nodiscard]] bool all_passed() const noexcept;
    [[nodiscard]] std::size_t failures() const noexcept;
};

struct AcceptanceInputs {
    double channel_threshold_km2 = 0.01;
    /// Rivers and lakes for §9.9. Leave default-constructed when they have not been extracted.
    RiverNetwork rivers;
    /// §9.10's subject. Negative means "not measured yet", which is reported as INAPPLICABLE rather
    /// than as a pass -- a suite that silently scores an unimplemented system as green is worse than
    /// one that is honestly incomplete.
    double stems_per_hectare = -1.0;
    /// §9.5 judges against the glacial band instead when the terrain being tested was carved by ice.
    bool expect_glacial_valleys = false;

    /// The FINAL heights, if they are not the macro field's own.
    ///
    /// Research §9 says its tests run "on a 512²-or-larger patch of FINAL (post-pipeline) heights".
    /// On this project the macro field is not that: `height_at` is macro + an analytic detail term,
    /// and the macro field alone has no energy below its own ~100 m feature scale. Running the
    /// shape tests on it measured spectral beta at 5.30 against a band of [1.6, 2.5] and a Hurst
    /// exponent of 0.84 against [0.46, 0.77] -- both reporting "too smooth", both measuring a
    /// surface the player never sees.
    ///
    /// So the five SHAPE tests (1, 2, 3, 5, 8) run on this when it is supplied, and the four
    /// NETWORK tests (4, 6, 7, 9) always run on the macro field, because flow routing is a macro
    /// concept and the detail term is not routed through. The report says which surface each used.
    const TerrainField* final_surface = nullptr;
    float sea_level = 0.0f;
};

/// Runs all ten on a field that has ALREADY been filled and accumulated.
[[nodiscard]] SuiteResult run_suite(const TerrainField& field, const FlowNetwork& net,
                                    const AcceptanceInputs& in);

/// One line per metric, aligned, with the band and citation -- what `terrain_dump` and the nightly
/// suite both print, so the two can never disagree about formatting or about the numbers.
[[nodiscard]] std::string format_report(const SuiteResult& r);

// ---------------------------------------------------------------------------------------------
// Exposed for its own unit test: the FFT §9.2 needs.
// ---------------------------------------------------------------------------------------------

/// In-place iterative radix-2 Cooley-Tukey FFT. `real` and `imag` must be the same power-of-two
/// length.
///
/// WRITTEN RATHER THAN DEPENDED ON, per this pass's rule 7 ("no new dependencies without a written
/// case ... a single 2D real FFT over a 1-4k grid is a well-understood ~200-line problem"). Only a
/// 1D transform is needed here, which is 60 lines; adding a dependency for that -- and taking its
/// build-system and licence surface -- would be the more expensive choice, not the cheaper one.
void fft_in_place(std::span<double> real, std::span<double> imag);

} // namespace world::generation::validation
