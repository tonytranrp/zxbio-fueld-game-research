#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <vector>

#include "world/wind/wind_field.hpp"

using namespace world::wind;

TEST_CASE("The wind field is a pure function of position and time", "[wind]") {
    // Prompt 001 B1's determinism check: same (pos, t) -> same wind, whatever order you ask in.
    const WindParams params;
    const std::vector<glm::vec3> points{
        {0.0f, 0.0f, 0.0f}, {13.5f, 2.0f, -71.25f}, {-400.0f, 60.0f, 9.0f}, {1e4f, 0.0f, 1e4f}};
    const std::vector<float> times{0.0f, 0.5f, 17.25f, 900.0f};

    std::vector<float> forward;
    for (const glm::vec3& p : points) {
        for (const float t : times) {
            forward.push_back(sample_wind(params, p, t).speed);
        }
    }
    // Same samples, evaluated back to front and interleaved with unrelated ones.
    std::size_t i = forward.size();
    for (auto p = points.rbegin(); p != points.rend(); ++p) {
        for (auto t = times.rbegin(); t != times.rend(); ++t) {
            (void)sample_wind(params, *p * 3.0f, *t + 1.0f); // noise between the reads
            REQUIRE(forward[--i] == sample_wind(params, *p, *t).speed);
        }
    }
    REQUIRE(i == 0);
}

TEST_CASE("Wind speed stays in the band the parameters describe", "[wind]") {
    const WindParams params;
    float lowest = 1e9f;
    float highest = -1e9f;
    for (int ix = -40; ix <= 40; ++ix) {
        for (int iz = -40; iz <= 40; ++iz) {
            for (int it = 0; it < 40; ++it) {
                const glm::vec3 p{static_cast<float>(ix) * 7.0f, 0.0f, static_cast<float>(iz) * 7.0f};
                const float s = sample_wind(params, p, static_cast<float>(it) * 0.25f).speed;
                lowest = std::min(lowest, s);
                highest = std::max(highest, s);
            }
        }
    }
    // The gust term is a sum of weights 0.55 + 0.30 + 0.15 = 1, so speed spans
    // base * (1 +/- gust_amplitude) at the extremes and never leaves it.
    const float span = params.base_speed * params.gust_amplitude;
    REQUIRE(lowest >= 0.0f);              // never reverses
    REQUIRE(lowest < params.base_speed);  // it does actually lull
    REQUIRE(highest > params.base_speed); // and does actually gust
    REQUIRE(highest <= params.base_speed + span + 1e-4f);
}

TEST_CASE("Gusts travel downwind rather than pulsing in place", "[wind]") {
    // The property that makes this a wind FIELD and not a wobble: the pattern seen at a point now
    // is the pattern seen upwind of it a moment ago.
    const WindParams params;
    const glm::vec3 dir = wind_direction(params);
    constexpr float kDt = 2.0f;
    const glm::vec3 here{123.0f, 0.0f, -45.0f};
    const glm::vec3 upwind = here - dir * (params.gust_scroll * kDt);

    const float nowHere = wind_gust(params, here, 10.0f + kDt);
    const float thenUpwind = wind_gust(params, upwind, 10.0f);
    REQUIRE_THAT(nowHere, Catch::Matchers::WithinAbs(thenUpwind, 1.0e-4));
}

TEST_CASE("The wind direction is horizontal and unit length", "[wind]") {
    for (float a = -3.0f; a < 3.0f; a += 0.37f) {
        WindParams params;
        params.base_angle_radians = a;
        const glm::vec3 d = wind_direction(params);
        REQUIRE_THAT(d.y, Catch::Matchers::WithinAbs(0.0, 1.0e-6));
        REQUIRE_THAT(glm::length(d), Catch::Matchers::WithinRel(1.0f, 1.0e-5f));
    }
}

TEST_CASE("still_wind makes every consumer provably static", "[wind]") {
    // B2's --no-wind check, at the source: not "each consumer checks a flag" but "the field itself
    // is zero", which is the only version that cannot be forgotten somewhere.
    const WindParams still = still_wind();
    for (int it = 0; it < 50; ++it) {
        const float t = static_cast<float>(it) * 0.31f;
        const glm::vec3 p{static_cast<float>(it) * 3.0f, 1.0f, -static_cast<float>(it)};
        REQUIRE(sample_wind(still, p, t).speed == 0.0f);
        REQUIRE(wind_flutter(still, p, t) == 0.0f);
    }
}

TEST_CASE("Flutter is bounded, fast, and spatially varying", "[wind]") {
    const WindParams params;
    // Bounded by its own weights.
    for (int i = 0; i < 500; ++i) {
        const float t = static_cast<float>(i) * 0.017f;
        const glm::vec3 p{static_cast<float>(i) * 0.13f, static_cast<float>(i) * 0.07f, 1.0f};
        const float f = wind_flutter(params, p, t);
        REQUIRE(f >= -1.0f - 1.0e-5f);
        REQUIRE(f <= 1.0f + 1.0e-5f);
    }
    // Neighbouring leaves must not shimmer in lockstep -- that is what makes a canopy read as many
    // leaves rather than one painted surface.
    const glm::vec3 leafA{10.0f, 5.0f, 10.0f};
    const glm::vec3 leafB = leafA + glm::vec3{0.6f, 0.0f, 0.0f};
    REQUIRE(std::abs(wind_flutter(params, leafA, 3.0f) - wind_flutter(params, leafB, 3.0f)) > 0.05f);
}

TEST_CASE("Wind varies over distance at the scale the gust frequency claims", "[wind]") {
    // A field whose gusts were kilometres wide would look identical everywhere a player can see.
    const WindParams params;
    const float here = wind_gust(params, {0.0f, 0.0f, 0.0f}, 0.0f);
    float mostDifferent = 0.0f;
    for (int i = 1; i <= 60; ++i) {
        const float d = static_cast<float>(i) * 2.0f; // out to 120 m
        mostDifferent = std::max(mostDifferent, std::abs(wind_gust(params, {d, 0.0f, 0.0f}, 0.0f) - here));
    }
    REQUIRE(mostDifferent > 0.5f); // a visible gust boundary inside view distance
}
