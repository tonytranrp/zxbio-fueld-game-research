#pragma once

#include "engine/core/math.hpp"

namespace world::wind {

// ONE wind field, read by everything that moves (Prompt 001 Group B): grass blades, tree canopies,
// the water's wave directions, and whatever comes after. Nothing gets to invent its own wind again
// -- the mesh path's ad-hoc sine wobble in terrain.vsh.hlsl is what that looks like, and it could
// never agree with anything else because it was not a field, just a wiggle.
//
// The model is Ghost of Tsushima's, per research/grass-rendering-research.md: a CONSTANT direction
// with a noise-varied magnitude, which reads as gusts sweeping across a field rather than as the
// whole world swaying in lockstep.
//
// Why sines and not FastNoise2, given the rest of the project uses it: this function has to exist
// twice, once here and once in HLSL (render/diligent/shaders/wind.fxh), and produce the same
// numbers in both. A hash-based noise cannot promise that across two compilers and two float
// pipelines; a short sum of directional sines can, exactly, because it is only sin and dot. The
// CONSTANTS below are not duplicated at all -- they are compiled into the shaders as macros from
// this header (render/diligent/detail/wind_macros.hpp), so drift is not merely unlikely, it is
// unrepresentable.
struct WindParams {
    // Where the wind comes FROM, as an angle in the XZ plane. Constant by design.
    float base_angle_radians = 0.6f;
    float base_speed = 4.0f; // m/s; the brief's 3-6 band, and a real "fresh breeze"
    // The gust term: amplitude as a fraction of base_speed, spatial frequency, and how fast the
    // gust pattern travels downwind (gusts outrun the mean wind).
    float gust_amplitude = 0.45f;
    float gust_frequency = 0.05f; // radians per metre -> ~125 m between gust crests
    float gust_scroll = 6.0f;     // m/s the pattern moves along the wind direction
    // Leaf/blade flutter: the fast, small-scale shimmer that reads as individual leaves catching
    // the light (research/tree-motion-growth-and-appearance.md §6.2's 3-5 Hz sunfleck band).
    float flutter_hz = 4.0f;
    float flutter_frequency = 1.7f; // radians per metre: neighbouring leaves are out of phase

    // BUFFETING: the turbulence in the band where trees resonate (Prompt 007 goal 335). The field
    // had a ~0.05 Hz gust term and a 4 Hz flutter term and NOTHING BETWEEN THEM -- and a tree's
    // fundamental sits at 0.26-1.0 Hz, exactly in that hole. The consequence was visible the moment
    // a sway sequence was actually looked at: the tree took its static lean and drifted with the
    // gusts, because nothing in the wind was moving at a rate it could resonate with.
    //
    // `turbulence_intensity` is sigma_u / U, the longitudinal turbulence intensity. 0.20 is a
    // mid-range value for open terrain near the ground in the wind-engineering literature; the tree
    // research does not give one, so it is labelled here as the engineering value it is.
    float turbulence_intensity = 0.20f;
    float buffet_frequency = 0.35f; // radians per metre: trees ~18 m apart buffet out of phase
};

inline constexpr WindParams kDefaultWind{};

// A wind with no wind: --no-wind sets this, and every consumer goes provably static because the
// gust term and the speed are both zero, not because each one checks a flag.
[[nodiscard]] constexpr WindParams still_wind() noexcept {
    WindParams p{};
    p.base_speed = 0.0f;
    p.gust_amplitude = 0.0f;
    p.flutter_hz = 0.0f;
    p.turbulence_intensity = 0.0f;
    return p;
}

// The wave shape itself. In the header rather than the .cpp because these are exactly the numbers
// render/diligent/detail/wind_macros.hpp compiles into every wind-aware shader: one definition,
// read by both sides, so the HLSL mirror cannot drift from this one even in principle.
//
// Incommensurate on purpose -- with rational frequency ratios the sum repeats on a visible
// lattice, which is the artefact the old per-feature sine wobble had.
namespace detail {

inline constexpr float kGustK0x = 1.000f;
inline constexpr float kGustK0z = 0.370f;
inline constexpr float kGustK1x = -0.630f;
inline constexpr float kGustK1z = 1.410f;
inline constexpr float kGustK2x = 2.170f;
inline constexpr float kGustK2z = -1.830f;
inline constexpr float kGustW0 = 0.55f;
inline constexpr float kGustW1 = 0.30f;
inline constexpr float kGustW2 = 0.15f;
inline constexpr float kGustP1 = 1.70f; // phase offsets, so the waves do not all peak at the origin
inline constexpr float kGustP2 = 4.10f;

inline constexpr float kFlutterF0x = 1.000f;
inline constexpr float kFlutterF0y = 0.610f;
inline constexpr float kFlutterF0z = -0.790f;
inline constexpr float kFlutterF1x = -0.470f;
inline constexpr float kFlutterF1y = 1.230f;
inline constexpr float kFlutterF1z = 0.880f;
inline constexpr float kFlutterW0 = 0.60f;
inline constexpr float kFlutterW1 = 0.40f;
inline constexpr float kFlutterRatio = 1.37f; // the second flutter wave's rate, relative to the first

// Buffeting waves. Three rates spanning 0.17-1.9 Hz -- the band that contains every tree
// fundamental this engine grows (0.26 Hz for a 20 m sycamore, ~0.94 Hz for an 8 m broadleaf) -- with
// incommensurate ratios so the sum never repeats on a visible period. Amplitudes fall with rate,
// which is the shape of a real turbulence spectrum rather than white noise.
inline constexpr float kBuffetR0 = 0.170f; // Hz
inline constexpr float kBuffetR1 = 0.590f;
inline constexpr float kBuffetR2 = 1.870f;
inline constexpr float kBuffetW0 = 0.50f;
inline constexpr float kBuffetW1 = 0.33f;
inline constexpr float kBuffetW2 = 0.17f;
inline constexpr float kBuffetK0x = 1.000f;
inline constexpr float kBuffetK0z = 0.430f;
inline constexpr float kBuffetK1x = -0.710f;
inline constexpr float kBuffetK1z = 1.190f;
inline constexpr float kBuffetK2x = 1.630f;
inline constexpr float kBuffetK2z = -0.970f;

inline constexpr float kTwoPi = 6.283185307179586f;

} // namespace detail

struct WindSample {
    glm::vec3 direction{1.0f, 0.0f, 0.0f}; // unit, horizontal
    float speed = 0.0f;                    // m/s at this point and time
    float gust = 0.0f;                     // the raw gust term in [-1, 1], for phase-coherent effects
    float buffet = 0.0f;                   // the turbulent term in [-1, 1], already folded into speed
};

// The field. Pure, allocation-free, and a function of (position, time) alone -- evaluate it in any
// order, on any thread, and get the same answer.
[[nodiscard]] WindSample sample_wind(const WindParams& params, const glm::vec3& position,
                                     float timeSeconds) noexcept;

// The raw gust term on its own, for callers that want the shape without the direction/speed.
[[nodiscard]] float wind_gust(const WindParams& params, const glm::vec3& position,
                              float timeSeconds) noexcept;

// Buffeting in [-1, 1]: the turbulent fluctuation in the 0.17-1.9 Hz band, which is what actually
// EXCITES a tree's sway mode. Deliberately a third term rather than a widening of the gust: the gust
// is a coherent patch of fast air sweeping across a landscape and every consumer reads it that way,
// while buffeting is small-scale turbulence that differs between two trees a stand apart.
//
// No HLSL mirror yet. `wind.fxh` gains one when a shader needs it (goal 337's canopy warp is the
// first candidate); until then this is CPU-only and the two sides cannot have drifted, because one
// of them does not exist.
[[nodiscard]] float wind_buffet(const WindParams& params, const glm::vec3& position,
                                float timeSeconds) noexcept;

// High-frequency flutter in [-1, 1] for foliage shading. Deliberately separate from the gust: the
// gust moves whole canopies, the flutter only shimmers their surface.
[[nodiscard]] float wind_flutter(const WindParams& params, const glm::vec3& position,
                                 float timeSeconds) noexcept;

// The unit horizontal direction the wind blows TOWARD.
[[nodiscard]] glm::vec3 wind_direction(const WindParams& params) noexcept;

} // namespace world::wind
