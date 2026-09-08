#pragma once

// Prompt 007 goal 326: every distance constant in this renderer, derived from a stated perceptual
// criterion, with its citation beside it.
//
// WHY THIS FILE EXISTS. The region size (512 m), the LOD radius (4 m), the fog density (0.0030) and
// the far plane were all round numbers somebody chose. `research/human-eye-and-vision-research.md`
// Part 1 §8 answers "how far can you see, and why do distant things fade" quantitatively, and none
// of its numbers appeared anywhere in the code. This is the header the rest of the pass cites, and
// the reason the next pass cannot quietly put a round number back.
//
// Everything here is `constexpr` free functions over `double` and depends on nothing. It is
// deliberately not in `render/diligent`: `world/svo`'s LOD ladder needs it too, and a perceptual
// criterion is not a rendering-backend concern.

#include <cmath>
#include <numbers>

namespace render::lod {

// ---------------------------------------------------------------------------------- angular limits

/// Minimum angle of resolution for 20/20 vision, in arcminutes. 20/20 IS the 1-arcmin MAR by
/// definition, which is 30 cycles/degree and therefore **60 pixels per degree** to sample it.
///
/// Aesthetics research §9.1 flags the arithmetic error to avoid: 20/20 = 1' MAR = 30 c/deg = 60 ppd,
/// while Campbell & Green's 60 c/deg = 120 ppd. **Conflating them moves a budget by 2x.**
inline constexpr double kMar20_20ArcMin = 1.0;
inline constexpr double kPixelsPerDegree20_20 = 60.0;

/// The behavioural ceiling for young observers: 94 ppd achromatic, measured by Ashraf et al. 2025
/// (Nature Communications) -- 89 ppd red-green, 53 yellow-violet. 94 ppd is 0.638 arcmin.
/// Eye research Part 1 §8.1, §3; aesthetics research §9.2.
inline constexpr double kPixelsPerDegreeCeiling = 94.0;
inline constexpr double kMarCeilingArcMin = 60.0 / kPixelsPerDegreeCeiling; // 0.6383'

[[nodiscard]] constexpr double arcmin_to_radians(double arcmin) noexcept {
    return arcmin * (std::numbers::pi / (180.0 * 60.0));
}
[[nodiscard]] constexpr double radians_to_arcmin(double radians) noexcept {
    return radians * (180.0 * 60.0 / std::numbers::pi);
}

/// The distance at which a feature of size `size_m` stops mattering: d = s / tan(MAR).
///
/// Eye research §8.1's worked values, which `test_perceptual.cpp` asserts this reproduces:
///   * 1 cm at 1 arcmin  -> **34 m**   (and ~54 m at the 94-ppd ceiling)
///   * 18 cm at 1 arcmin -> **619 m**  (a face-sized blob, not an expression)
[[nodiscard]] inline double resolvable_distance_m(double size_m, double marArcMin = kMar20_20ArcMin) {
    return size_m / std::tan(arcmin_to_radians(marArcMin));
}

/// The inverse: the angular size, in arcminutes, of `size_m` at `distance_m`. This is the quantity
/// the LOD ladder should be expressed in -- it means the same thing at any resolution, which
/// "1.5 pixels" does not.
[[nodiscard]] inline double angular_size_arcmin(double size_m, double distance_m) {
    return distance_m <= 0.0 ? 1.0e30 : radians_to_arcmin(std::atan(size_m / distance_m));
}

// ------------------------------------------------------------------------------ atmospheric fading

/// Which contrast threshold defines "visibility". These are two CONVENTIONS for one physical
/// extinction coefficient, not two models, and mixing them is a 30% error in the fog rate.
///
/// Aesthetics research §9.6 obtained and read the real WMO CIMO Guide (2023, Chapter 9): it prints
/// **P = (1/sigma)·ln(1/0.05) ~ 3/sigma**, and **the constant 3.912 appears nowhere in it**. Within
/// WMO's framework 0.05 is *definitional* for meteorological optical range, not an alternative.
/// 3.912 is Koschmieder's historical C_t = 0.02, which is what the eye research's own transmission
/// table was computed with -- so both are here, each named after its authority.
enum class VisibilityConvention {
    Wmo,         ///< C_t = 0.05, V = 3.0/sigma. WMO-No. 8 CIMO Guide eq. 9.6. Definitional for MOR.
    Koschmieder, ///< C_t = 0.02, V = 3.912/sigma. Koschmieder 1924; the eye research's table.
};

inline constexpr double kWmoContrastThreshold = 0.05;
inline constexpr double kKoschmiederContrastThreshold = 0.02;
/// ln(1/0.05) = 2.9957, which WMO prints as ~3. ln(1/0.02) = 3.9120.
inline constexpr double kWmoRateConstant = 2.99573227355399;
inline constexpr double kKoschmiederRateConstant = 3.91202300542815;

[[nodiscard]] constexpr double rate_constant(VisibilityConvention c) noexcept {
    return c == VisibilityConvention::Wmo ? kWmoRateConstant : kKoschmiederRateConstant;
}

/// Extinction coefficient (per metre) from an authored meteorological visibility (metres).
///
/// **Visibility is the authored parameter and sigma is the derived one**, which is the whole point:
/// "how far can you see" is a number a person can hold, and 0.0030 was not.
[[nodiscard]] constexpr double
extinction_from_visibility(double visibility_m,
                           VisibilityConvention c = VisibilityConvention::Koschmieder) noexcept {
    return visibility_m <= 0.0 ? 0.0 : rate_constant(c) / visibility_m;
}

[[nodiscard]] constexpr double
visibility_from_extinction(double sigma_per_m,
                           VisibilityConvention c = VisibilityConvention::Koschmieder) noexcept {
    return sigma_per_m <= 0.0 ? 0.0 : rate_constant(c) / sigma_per_m;
}

/// Koschmieder's law: the fraction of an object's original contrast that survives `distance_m`.
///
/// Eye research §8.2's worked table, which the test asserts:
///   V = 10 km: 0.5 km -> 82%, 1 km -> 68%, 2 km -> 46%
///   V = 20 km: 0.5 km -> 91%, 1 km -> 82%
///   V = 50 km: 2 km -> 85%, 5 km -> 68%
///   V = 100 km: 1 km -> 96%
[[nodiscard]] inline double
contrast_transmission(double distance_m, double visibility_m,
                      VisibilityConvention c = VisibilityConvention::Koschmieder) {
    return std::exp(-extinction_from_visibility(visibility_m, c) * distance_m);
}

/// The named visibility bands, from the eye research §8.2 and aesthetics §9.6. These are what
/// `--visibility` should be chosen from rather than invented.
inline constexpr double kVisibilityLightHazeM = 10000.0;
inline constexpr double kVisibilityClearM = 20000.0;
inline constexpr double kVisibilityVeryClearM = 50000.0;
inline constexpr double kVisibilityExceptionalM = 100000.0;

/// **The model is outside its own validity domain past ~200 km** -- "inaccurate for very clean
/// atmospheres where the curvature of the earth becomes a factor". And ECMWF's operational forecast
/// model caps visibility at 100 km "as this reflects the extinction coefficient of clean air".
/// Two independent domains converge on 100 km as the practical atmospheric far bound.
inline constexpr double kVisibilityMaxUsefulM = 100000.0;

/// The US EPA's authoring rule, `[C]` in aesthetics §9.6: "noticeable degradation of scenic
/// appearance (including the disappearance of some features) occurs on some objects as near as
/// **within 10 percent of the visual range**." At clear-air V ~ 39 km that is ~3.9 km -- fog should
/// be doing perceptible work from the first few kilometres, and can therefore CARRY the LOD
/// transition rather than fight it.
[[nodiscard]] constexpr double perceptible_haze_distance_m(double visibility_m) noexcept {
    return 0.10 * visibility_m;
}

// -------------------------------------------------------------------------------------- horizon

/// Geometric horizon coefficient, km per sqrt(metre). The spread across authorities is ~10% and
/// each is named after its own, per aesthetics research §9.5:
///   * 3.56959 -- pure geometric, sqrt(2R)/1000 exactly
///   * 3.856   -- the 7/6-Earth-radius standard refraction correction
///   * 3.9215  -- Bowditch / navigational practice
/// Young's caveat travels with all three: "occasionally, they will be wildly off, particularly if
/// superior mirages are visible."
inline constexpr double kHorizonGeometric = 3.56959;
inline constexpr double kHorizonRefracted = 3.856;
inline constexpr double kHorizonBowditch = 3.9215;

/// **The form that actually sets a far plane**: D = c(sqrt(h) + sqrt(H)) in kilometres, for an eye
/// at `eyeHeight_m` and a target of height `targetHeight_m`.
///
/// This is why a single far plane is the wrong shape (aesthetics §9.5): at 1.7 m eye height a 1 m
/// detail vanishes at ~8.9 km, but a **1000 m massif stays geometrically visible to ~127 km** and a
/// 4000 m peak to ~249 km. A 4.65 km far plane cuts off geometry the player can physically see.
[[nodiscard]] inline double visible_distance_km(double eyeHeight_m, double targetHeight_m,
                                                double coefficient = kHorizonRefracted) {
    return coefficient * (std::sqrt(std::max(eyeHeight_m, 0.0)) + std::sqrt(std::max(targetHeight_m, 0.0)));
}

/// Own-horizon distance: the target-height form with H = 0.
[[nodiscard]] inline double horizon_km(double eyeHeight_m, double coefficient = kHorizonRefracted) {
    return visible_distance_km(eyeHeight_m, 0.0, coefficient);
}

// ------------------------------------------------------------------------------ screen and pixels

/// The angle one pixel subtends at the CENTRE of the screen, radians.
///
/// **Use the centre-pixel spread angle, not deg/px** -- aesthetics research §9.4. A frame-average
/// `fov / width` under-states the centre pixel's angle: with a perspective projection the centre
/// pixel subtends **10.3% more** angle than the average, and using the average under-selects the
/// octree level exactly at screen centre, which is where the player is looking.
[[nodiscard]] inline double centre_pixel_angle_rad(double horizontalFovRad, double widthPixels) {
    if (widthPixels <= 0.0) {
        return 0.0;
    }
    // Half the screen spans tan(fov/2) in normalised units; one pixel at the centre spans
    // 2·tan(fov/2)/width of that, and its angle is the arctangent of that half-width doubled.
    const double halfExtent = std::tan(0.5 * horizontalFovRad);
    return 2.0 * std::atan(halfExtent / widthPixels);
}

/// The size of a feature that subtends exactly one pixel at `distance_m`.
[[nodiscard]] inline double pixel_footprint_m(double pixelAngleRad, double distance_m) {
    return 2.0 * distance_m * std::tan(0.5 * pixelAngleRad);
}

/// Is this cube below the EYE's limit, or merely below the SCREEN's? The two differ whenever the
/// display is not exactly 60 ppd, and conflating them is how a quality setting stops meaning
/// anything.
[[nodiscard]] inline bool below_eye_limit(double size_m, double distance_m,
                                          double marArcMin = kMar20_20ArcMin) {
    return angular_size_arcmin(size_m, distance_m) < marArcMin;
}

} // namespace render::lod
