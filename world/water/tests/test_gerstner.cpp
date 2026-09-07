#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

#include "world/water/gerstner.hpp"

using namespace world::water;
using world::wind::WindParams;

TEST_CASE("The steepness budget is respected, so the surface cannot fold", "[water][gerstner]") {
    // research/water-physics-and-wave-simulation.md §8.2's warning: sum(Q*k*A) > 1 makes a Gerstner
    // surface self-intersect into loops. This is the check that keeps the field on the safe side of
    // it at every wind speed, not just the default one.
    for (float speed = 0.0f; speed <= 25.0f; speed += 0.5f) {
        WindParams wind;
        wind.base_speed = speed;
        const WaveField field = make_wave_field(wind);
        float total = 0.0f;
        for (const GerstnerWave& w : field.waves) {
            total += w.steepness * w.wavenumber * w.amplitude;
        }
        REQUIRE(total <= 1.0f);
        if (speed > 0.0f) {
            REQUIRE_THAT(total, Catch::Matchers::WithinAbs(0.60, 1.0e-4)); // the budget we target
        }
    }
}

TEST_CASE("No wind is glass", "[water][gerstner]") {
    WindParams wind;
    wind.base_speed = 0.0f;
    const WaveField field = make_wave_field(wind);
    for (float t = 0.0f; t < 20.0f; t += 0.7f) {
        REQUIRE(wave_height(field, 13.0f, -7.0f, t) == 0.0f);
        const WaveSurface s = wave_surface(field, 13.0f, -7.0f, t);
        REQUIRE_THAT(s.normal.y, Catch::Matchers::WithinAbs(1.0, 1.0e-6));
    }
}

TEST_CASE("More wind builds longer AND taller waves", "[water][gerstner]") {
    // The fetch-limited growth every sea-state table shows: a stronger wind does not just make the
    // same chop bigger, it opens the crest spacing out.
    const auto longestWavelength = [](const WaveField& f) {
        float longest = 0.0f;
        for (const GerstnerWave& w : f.waves) {
            if (w.wavenumber > 0.0f) {
                longest = std::max(longest, 6.283185307f / w.wavenumber);
            }
        }
        return longest;
    };
    const auto tallest = [](const WaveField& f) {
        float a = 0.0f;
        for (const GerstnerWave& w : f.waves) {
            a = std::max(a, w.amplitude);
        }
        return a;
    };

    WindParams calm;
    calm.base_speed = 0.5f;
    WindParams fresh;
    fresh.base_speed = 3.0f;
    WindParams gale;
    gale.base_speed = 8.0f;

    const WaveField a = make_wave_field(calm);
    const WaveField b = make_wave_field(fresh);
    const WaveField c = make_wave_field(gale);
    REQUIRE(longestWavelength(a) < longestWavelength(b));
    REQUIRE(longestWavelength(b) < longestWavelength(c));
    REQUIRE(tallest(a) < tallest(b));
    REQUIRE(tallest(b) < tallest(c));
}

TEST_CASE("Deep-water dispersion: longer waves travel faster", "[water][gerstner]") {
    // omega = sqrt(g*k), so phase speed c = omega/k = sqrt(g/k) grows with wavelength. This is why
    // a swell outruns the chop instead of the whole field sliding as one sheet.
    WindParams wind;
    wind.base_speed = 6.0f;
    const WaveField field = make_wave_field(wind);
    float previousSpeed = 1e9f;
    for (const GerstnerWave& w : field.waves) {
        REQUIRE(w.wavenumber > 0.0f);
        const float phaseSpeed = w.omega() / w.wavenumber;
        REQUIRE_THAT(phaseSpeed, Catch::Matchers::WithinRel(std::sqrt(kGravity / w.wavenumber), 1.0e-5f));
        REQUIRE(phaseSpeed < previousSpeed); // components are ordered longest-first
        previousSpeed = phaseSpeed;
    }
}

TEST_CASE("The analytic normal agrees with the surface it claims to describe", "[water][gerstner]") {
    // The reason the normal is derived from the same sum rather than differenced from the height:
    // check it against the ACTUAL displaced surface, which is what a finite difference of height
    // alone gets wrong (Gerstner moves points horizontally too).
    WindParams wind;
    wind.base_speed = 5.0f;
    const WaveField field = make_wave_field(wind);
    constexpr float kEps = 0.01f;
    for (float x = -30.0f; x <= 30.0f; x += 11.0f) {
        for (float z = -30.0f; z <= 30.0f; z += 13.0f) {
            const WaveSurface c = wave_surface(field, x, z, 4.25f);
            const WaveSurface px = wave_surface(field, x + kEps, z, 4.25f);
            const WaveSurface pz = wave_surface(field, x, z + kEps, 4.25f);
            const glm::vec3 tangentX = px.position - c.position;
            const glm::vec3 tangentZ = pz.position - c.position;
            const glm::vec3 geometric = glm::normalize(glm::cross(tangentZ, tangentX));
            // Within a couple of degrees of the surface's real geometric normal.
            REQUIRE(glm::dot(geometric, c.normal) > 0.999f);
        }
    }
}

TEST_CASE("Waves travel, and the field is a pure function of position and time", "[water][gerstner]") {
    WindParams wind;
    wind.base_speed = 5.0f;
    const WaveField field = make_wave_field(wind);
    // Deterministic.
    REQUIRE(wave_height(field, 3.0f, 4.0f, 1.5f) == wave_height(field, 3.0f, 4.0f, 1.5f));
    // And actually moving: the height at a fixed column changes over time.
    float lowest = 1e9f;
    float highest = -1e9f;
    for (int i = 0; i < 400; ++i) {
        const float h = wave_height(field, 3.0f, 4.0f, static_cast<float>(i) * 0.05f);
        lowest = std::min(lowest, h);
        highest = std::max(highest, h);
    }
    REQUIRE(highest - lowest > 0.1f); // metres of real vertical travel at a fixed point
}

TEST_CASE("Waves die out in shallow water", "[water][gerstner]") {
    // E3 / research §2.3: a wave cannot orbit in water thinner than about half its wavelength.
    WindParams wind;
    wind.base_speed = 6.0f;
    const WaveField field = make_wave_field(wind);
    REQUIRE(shore_fade(field, 0.0f) == 0.0f);   // at the waterline: flat
    REQUIRE(shore_fade(field, -1.0f) == 0.0f);  // above it: still flat, never negative
    REQUIRE(shore_fade(field, 0.2f) < 0.15f);   // ankle deep: nearly flat
    REQUIRE(shore_fade(field, 100.0f) == 1.0f); // open sea: full amplitude
    // Monotone, so the transition has no ripple of its own.
    float previous = -1.0f;
    for (float d = 0.0f; d <= 40.0f; d += 0.25f) {
        const float f = shore_fade(field, d);
        REQUIRE(f >= previous - 1.0e-6f);
        previous = f;
    }
}
