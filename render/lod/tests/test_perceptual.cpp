// Prompt 007 goal 326's Check, verbatim: "a unit test asserts the worked values from the research
// reproduce: a 1 cm feature is resolvable to 34 m at 1 arcmin and ~54 m at 0.64 arcmin; an 18 cm
// feature to 619 m; contrast transmission at 1 km in V = 20 km is 82% and in V = 10 km is 68%.
// Those are the research's own `pwsh`-worked numbers -- **if your code disagrees with them, your
// code is wrong.**"
//
// That last sentence is the reason this file is worth writing. Prompt 006 spent most of its
// corrections on instruments that were wrong rather than terrain that was, and the defence it
// arrived at -- test the instrument against an analytically known answer before trusting its
// reading -- applies exactly here. These are the known answers.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <numbers>

#include "render/lod/perceptual.hpp"

using namespace render::lod;

TEST_CASE("the resolvability distances reproduce the research's worked table", "[lod][perceptual]") {
    // Eye research Part 1 §8.1.
    INFO("1 cm at 20/20 -> " << resolvable_distance_m(0.01) << " m");
    CHECK(resolvable_distance_m(0.01) == Catch::Approx(34.0).margin(0.5));

    // "For the 94-ppd observer the same 1 cm detail extends to ~54 m."
    INFO("1 cm at the 94-ppd ceiling -> " << resolvable_distance_m(0.01, kMarCeilingArcMin) << " m");
    CHECK(resolvable_distance_m(0.01, kMarCeilingArcMin) == Catch::Approx(54.0).margin(1.0));

    // "an 18 cm face out to 619 m"
    INFO("18 cm at 20/20 -> " << resolvable_distance_m(0.18) << " m");
    CHECK(resolvable_distance_m(0.18) == Catch::Approx(619.0).margin(2.0));

    // And the table's angular sizes, the other way round.
    CHECK(angular_size_arcmin(0.01, 10.0) == Catch::Approx(3.4).margin(0.05));
    CHECK(angular_size_arcmin(0.18, 100.0) == Catch::Approx(6.2).margin(0.05));
    CHECK(angular_size_arcmin(0.18, 1000.0) == Catch::Approx(0.62).margin(0.02));
}

// NAME IS PLAIN ASCII ON PURPOSE. A section sign in a TEST NAME is mangled by the time ctest
// passes it back as a filter argument, and the test then fails under ctest while passing when run
// by hand -- the same class of defect CLAUDE.md records for names beginning with "--". Section
// references belong in comments, where they are.
TEST_CASE("20/20 is 60 ppd, not 120 -- the arithmetic error in section 9.1", "[lod][perceptual]") {
    // Aesthetics research §9.1: "20/20 = 1' MAR = 30 c/deg = 60 ppd, while Campbell & Green's
    // 60 c/deg = 120 ppd; conflating them moves a budget by 2x." Pinned so it cannot drift back.
    CHECK(kMar20_20ArcMin == Catch::Approx(1.0));
    CHECK(kPixelsPerDegree20_20 == Catch::Approx(60.0));
    CHECK(60.0 / kMar20_20ArcMin == Catch::Approx(kPixelsPerDegree20_20));
    // And the ceiling: Ashraf et al. 2025's 94 ppd is 0.638 arcmin, which the prompt rounds to 0.64.
    CHECK(kMarCeilingArcMin == Catch::Approx(0.6383).margin(0.001));
}

TEST_CASE("contrast transmission reproduces the research's table", "[lod][perceptual]") {
    // Eye research §8.2, worked with e^(-3.912 d / V) -- Koschmieder's C_t = 0.02 convention, which
    // is the one that table was computed in.
    const auto pct = [](double d, double v) {
        return 100.0 * contrast_transmission(d, v, VisibilityConvention::Koschmieder);
    };

    // V = 10 km (light haze)
    CHECK(pct(500.0, kVisibilityLightHazeM) == Catch::Approx(82.0).margin(0.6));
    CHECK(pct(1000.0, kVisibilityLightHazeM) == Catch::Approx(68.0).margin(0.6));
    CHECK(pct(2000.0, kVisibilityLightHazeM) == Catch::Approx(46.0).margin(0.6));
    // V = 20 km (clear)
    CHECK(pct(500.0, kVisibilityClearM) == Catch::Approx(91.0).margin(0.6));
    CHECK(pct(1000.0, kVisibilityClearM) == Catch::Approx(82.0).margin(0.6));
    // V = 50 km (very clear)
    CHECK(pct(2000.0, kVisibilityVeryClearM) == Catch::Approx(85.0).margin(0.6));
    CHECK(pct(5000.0, kVisibilityVeryClearM) == Catch::Approx(68.0).margin(0.6));
    // V = 100 km (exceptional)
    CHECK(pct(1000.0, kVisibilityExceptionalM) == Catch::Approx(96.0).margin(0.6));
}

TEST_CASE("the two visibility conventions are one model with two thresholds", "[lod][perceptual]") {
    // §9.6's correction: WMO prints ~3/sigma and 3.912 appears nowhere in it; 0.05 is DEFINITIONAL
    // for MOR, not an alternative. Both are here, each named after its authority, and the ratio
    // between them is the conversion the prompt says to state if you switch.
    CHECK(kWmoRateConstant == Catch::Approx(-std::log(kWmoContrastThreshold)).margin(1e-9));
    CHECK(kKoschmiederRateConstant == Catch::Approx(-std::log(kKoschmiederContrastThreshold)).margin(1e-9));
    CHECK(kKoschmiederRateConstant / kWmoRateConstant == Catch::Approx(1.3059).margin(0.001));

    // At the SAME authored visibility the two disagree by that ratio in extinction -- which is why
    // mixing them is a 30% error in the fog rate rather than a rounding difference.
    const double v = 20000.0;
    CHECK(extinction_from_visibility(v, VisibilityConvention::Koschmieder) /
              extinction_from_visibility(v, VisibilityConvention::Wmo) ==
          Catch::Approx(1.3059).margin(0.001));

    // Round-tripping must be exact in either convention.
    for (const auto c : {VisibilityConvention::Wmo, VisibilityConvention::Koschmieder}) {
        CHECK(visibility_from_extinction(extinction_from_visibility(v, c), c) ==
              Catch::Approx(v).margin(1e-6));
    }
    // And by construction, contrast at exactly V equals the convention's own threshold.
    CHECK(contrast_transmission(v, v, VisibilityConvention::Wmo) ==
          Catch::Approx(kWmoContrastThreshold).margin(1e-9));
    CHECK(contrast_transmission(v, v, VisibilityConvention::Koschmieder) ==
          Catch::Approx(kKoschmiederContrastThreshold).margin(1e-9));
}

TEST_CASE("the horizon form is target-height, not eye-height alone", "[lod][perceptual]") {
    // Aesthetics research §9.5, and the reason a single far plane is the wrong shape.
    //
    // "At 1.7 m eye height, 1 m detail vanishes at ~8.9 km -- but a 1000 m massif stays
    // geometrically visible to ~127 km and a 4000 m peak to ~249 km."
    INFO("own horizon at 1.7 m: " << horizon_km(1.7) << " km");
    CHECK(horizon_km(1.7) == Catch::Approx(5.03).margin(0.1));

    INFO("1 m detail from 1.7 m: " << visible_distance_km(1.7, 1.0) << " km");
    CHECK(visible_distance_km(1.7, 1.0) == Catch::Approx(8.89).margin(0.15));

    INFO("1000 m massif: " << visible_distance_km(1.7, 1000.0) << " km");
    CHECK(visible_distance_km(1.7, 1000.0) == Catch::Approx(127.0).margin(2.0));

    INFO("4000 m peak: " << visible_distance_km(1.7, 4000.0) << " km");
    CHECK(visible_distance_km(1.7, 4000.0) == Catch::Approx(249.0).margin(3.0));

    // The coefficient spread across authorities is ~10% and each is named. sqrt(2R)/1000 = 3.56959.
    CHECK(kHorizonGeometric < kHorizonRefracted);
    CHECK(kHorizonRefracted < kHorizonBowditch);
    CHECK(kHorizonBowditch / kHorizonGeometric == Catch::Approx(1.0985).margin(0.005));
}

TEST_CASE("the centre pixel subtends more angle than the frame average", "[lod][perceptual]") {
    // Aesthetics research §9.4's second correction: "use the centre-pixel spread angle, not deg/px
    // -- the centre pixel subtends 10.3% more angle, and the frame-average value under-selects the
    // octree level exactly at screen centre."
    //
    // Asserted as the INEQUALITY plus its direction, because the exact 10.3% depends on the FOV the
    // research quoted it at, and the property that matters is that naive deg/px is an underestimate
    // of the centre pixel.
    constexpr double kFov = 60.0 * std::numbers::pi / 180.0;
    constexpr double kWidth = 1280.0;
    const double centre = centre_pixel_angle_rad(kFov, kWidth);
    const double naive = kFov / kWidth;
    INFO("centre-pixel " << radians_to_arcmin(centre) << "' vs frame-average " << radians_to_arcmin(naive)
                         << "'");
    CHECK(centre > naive);
    CHECK(centre / naive == Catch::Approx(1.103).margin(0.02));

    // A pixel footprint grows linearly with distance to first order.
    CHECK(pixel_footprint_m(centre, 200.0) / pixel_footprint_m(centre, 100.0) ==
          Catch::Approx(2.0).margin(1e-6));
}

TEST_CASE("the eye limit and the screen limit are different questions", "[lod][perceptual]") {
    // A 7.8 mm voxel -- this engine's finest -- stops being resolvable by the EYE at 26.8 m.
    // Goal 328 is built on this number, so it is pinned here.
    const double finestVoxel = 0.0078125;
    INFO("7.8 mm voxel resolvable to " << resolvable_distance_m(finestVoxel) << " m");
    CHECK(resolvable_distance_m(finestVoxel) == Catch::Approx(26.85).margin(0.3));

    CHECK(below_eye_limit(finestVoxel, 100.0));
    CHECK_FALSE(below_eye_limit(finestVoxel, 10.0));
}

TEST_CASE("the EPA authoring rule puts perceptible haze in the first few kilometres", "[lod][perceptual]") {
    // Aesthetics §9.6: "noticeable degradation of scenic appearance occurs on some objects as near
    // as within 10 percent of the visual range." At clear-air V ~ 39 km that is ~3.9 km, which is
    // why fog can CARRY the LOD transition rather than fight it.
    CHECK(perceptible_haze_distance_m(39000.0) == Catch::Approx(3900.0));
    CHECK(perceptible_haze_distance_m(kVisibilityClearM) == Catch::Approx(2000.0));
}
