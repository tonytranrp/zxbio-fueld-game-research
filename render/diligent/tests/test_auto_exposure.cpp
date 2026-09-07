// Goal 237's arithmetic, tested without a GPU. The metering passes need a device; the POOLING
// GEOMETRY and the TWO-TIMESCALE ADAPTATION do not, and those are the two parts most likely to be
// wrong in a way a picture would not show.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "render/diligent/auto_exposure.hpp"

using render::diligent::adapt_log_luminance;
using render::diligent::ExposureSettings;
using render::diligent::pooling_tangent;

TEST_CASE("The pooling mask is an ANGLE, not a pixel count", "[exposure]") {
    // The research's number is 6 degrees of visual angle around fixation, so the shipped default is
    // a 3 degree half-angle. What the shader needs is its tangent, because the screen is a plane.
    CHECK(pooling_tangent(3.0f) == Catch::Approx(std::tan(3.0 * 3.14159265 / 180.0)).margin(1e-6));
    // Monotone and positive over any sane input, including the degenerate one -- a zero or negative
    // pooling angle must not produce a zero divisor in the shader.
    CHECK(pooling_tangent(0.0f) > 0.0f);
    CHECK(pooling_tangent(-5.0f) > 0.0f);
    CHECK(pooling_tangent(6.0f) > pooling_tangent(3.0f));

    // The consequence worth pinning: at a 70 degree vertical FOV the 3 degree pool is a small
    // fraction of the half-height, which is what makes it a CROSSHAIR mask rather than most of the
    // frame. tan(3)/tan(35) = 0.0524/0.7002.
    const float fraction = pooling_tangent(3.0f) / std::tan(35.0f * 3.14159265f / 180.0f);
    CHECK(fraction > 0.05f);
    CHECK(fraction < 0.10f);
}

TEST_CASE("Adaptation is fast up and slow down", "[exposure]") {
    // 5.7(b): "keep two-timescale adaptation (fast up, slow down) as engines do -- it accidentally
    // mimics the light-adaptation/dark-adaptation asymmetry". This is the assertion that the two
    // constants are actually used in the right directions, which a single-tau bug would pass
    // every visual check.
    ExposureSettings settings;
    REQUIRE(settings.adapt_up_seconds < settings.adapt_down_seconds);

    constexpr float kStep = 1.0f; // one second, one EV of error
    const float brightened = adapt_log_luminance(0.0f, 1.0f, settings, kStep);
    const float darkened = adapt_log_luminance(0.0f, -1.0f, settings, kStep);
    // Same magnitude of error, same elapsed time: the brightening one must have covered MORE of it.
    CHECK(std::abs(brightened - 0.0f) > std::abs(darkened - 0.0f));
}

TEST_CASE("Adaptation approaches its target and never overshoots", "[exposure]") {
    // An exponential approach, chosen partly because it cannot overshoot at any dt -- the huge-dt
    // case that has broken two other integrators in this pass.
    const ExposureSettings settings;
    for (const float dt : {1.0f / 240.0f, 1.0f / 60.0f, 0.5f, 10.0f, 1000.0f}) {
        const float up = adapt_log_luminance(0.0f, 2.0f, settings, dt);
        CHECK(up >= 0.0f);
        CHECK(up <= 2.0f);
        const float down = adapt_log_luminance(0.0f, -2.0f, settings, dt);
        CHECK(down <= 0.0f);
        CHECK(down >= -2.0f);
    }
    // And it does converge: a hundred ticks of the slow constant gets essentially all the way.
    float adapted = 0.0f;
    for (int i = 0; i < 100; ++i) {
        adapted = adapt_log_luminance(adapted, -3.0f, settings, 0.1f);
    }
    CHECK(adapted == Catch::Approx(-3.0).margin(0.02));
}

TEST_CASE("Adaptation is clamped so one frame cannot strand the exposure", "[exposure]") {
    // A frame of pure sky or pure cave must not drive the state somewhere it takes ten seconds to
    // walk back from.
    ExposureSettings settings;
    settings.min_log_luminance = -6.0f;
    settings.max_log_luminance = 4.0f;
    float adapted = 0.0f;
    for (int i = 0; i < 500; ++i) {
        adapted = adapt_log_luminance(adapted, 100.0f, settings, 0.1f);
    }
    CHECK(adapted <= settings.max_log_luminance + 1e-4f);
    for (int i = 0; i < 500; ++i) {
        adapted = adapt_log_luminance(adapted, -100.0f, settings, 0.1f);
    }
    CHECK(adapted >= settings.min_log_luminance - 1e-4f);
}

TEST_CASE("A zero time step holds the state rather than snapping it", "[exposure]") {
    // dt == 0 happens on a paused or first frame. Snapping there would make the exposure jump
    // whenever the clock reports nothing elapsed, which is exactly when nothing should change.
    const ExposureSettings settings;
    CHECK(adapt_log_luminance(0.5f, -2.0f, settings, 0.0f) == Catch::Approx(0.5).margin(1e-5));
    // A zero TIME CONSTANT is the opposite instruction and does snap -- "adapt instantly" is a
    // legitimate setting, and it is not the same statement as "no time has passed".
    ExposureSettings instant = settings;
    instant.adapt_down_seconds = 0.0f;
    CHECK(adapt_log_luminance(0.5f, -2.0f, instant, 0.016f) == Catch::Approx(-2.0).margin(1e-5));
}
