#pragma once

#include <array>
#include <cstddef>

#include "engine/core/math.hpp"
#include "world/wind/wind_field.hpp"

namespace world::water {

// The visual wave field (Prompt 001 Group E, research/water-physics-and-wave-simulation.md §8.2).
// Gerstner waves: the exact nonlinear solution for deep-water gravity waves, and the one every
// real-time engine uses, because water particles move in circles rather than up and down -- which
// is what sharpens crests and flattens troughs instead of producing a sine that reads as jelly.
//
// SCOPE, stated so it is not re-litigated: this is the wave FIELD and the player's interaction with
// it. It is deliberately NOT currents, shallow-water equations, foam advection or breaking physics
// -- the research's §5.3/§9 pipeline (shoaling, reef dissipation, radiation stress) is a future arc
// of its own magnitude. The complaint that motivated it was "the water is static"; a travelling
// surface with a surface the swimmer rides answers that.
//
// Like world/wind, this is the CPU reference the HLSL mirrors, and for the same reason: it has to
// produce the same numbers in both, and it is only sin, cos and sqrt.

inline constexpr std::size_t kWaveCount = 4;
inline constexpr float kGravity = 9.81f; // the waves' g, not the player's -32 game gravity

struct GerstnerWave {
    glm::vec2 direction{1.0f, 0.0f}; // unit, horizontal
    float amplitude = 0.0f;          // metres, crest to still level
    float wavenumber = 0.0f;         // k = 2*pi / wavelength
    float steepness = 0.0f;          // Q: 0 = a rolling sine, higher = sharper crests
    // Deep-water dispersion, omega = sqrt(g*k) (research §2.2). Our sea is deep relative to these
    // wavelengths, so the shallow-water correction does not apply and longer waves travel faster --
    // which is why a swell outruns a chop rather than the whole field moving as one.
    [[nodiscard]] float omega() const noexcept;
};

struct WaveField {
    std::array<GerstnerWave, kWaveCount> waves{};
};

// Period of the LARGEST-amplitude component, seconds (T = 2*pi/omega). The field is a sum of several
// waves and has no single period, but a body floating on it bobs mostly at the biggest one -- and a
// flicker count is only readable against a period ("31 transitions" means nothing until you know the
// run saw 25 crests). Goal 236 is the reason this exists.
[[nodiscard]] float dominant_period(const WaveField& field) noexcept;

// Build the field from the wind (Group B). Wind and waves share one direction and one strength,
// which is the physically right coupling and also why --no-wind gives glass.
//
// The steepness budget: sum(Q_i * k_i * A_i) <= 1 or the surface self-intersects into loops
// (research §8.2's warning). We target 0.6, leaving headroom, and the test pins it.
[[nodiscard]] WaveField make_wave_field(const wind::WindParams& wind) noexcept;

// Vertical displacement of the surface at a world column, at time t. This is what the swimmer
// rides (E2) -- the horizontal Gerstner displacement is a shading/rendering concern, but the
// height is physics.
[[nodiscard]] float wave_height(const WaveField& field, float x, float z, float timeSeconds) noexcept;

// The full displaced surface point and its analytic normal, derived from the same sum -- never
// from finite differences of the height, which would disagree with the crests at exactly the
// steepnesses that matter.
struct WaveSurface {
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
};
[[nodiscard]] WaveSurface wave_surface(const WaveField& field, float x, float z, float timeSeconds) noexcept;

// E3: a wave cannot orbit in water thinner than about half its wavelength (research §2.3), so its
// amplitude has to die out as the bottom comes up. Returns a [0, 1] scale for `depth` metres of
// water under the still surface.
[[nodiscard]] float shore_fade(const WaveField& field, float depth) noexcept;

} // namespace world::water
