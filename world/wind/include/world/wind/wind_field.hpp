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
};

inline constexpr WindParams kDefaultWind{};

// A wind with no wind: --no-wind sets this, and every consumer goes provably static because the
// gust term and the speed are both zero, not because each one checks a flag.
[[nodiscard]] constexpr WindParams still_wind() noexcept {
    WindParams p{};
    p.base_speed = 0.0f;
    p.gust_amplitude = 0.0f;
    p.flutter_hz = 0.0f;
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

inline constexpr float kTwoPi = 6.283185307179586f;

} // namespace detail

struct WindSample {
    glm::vec3 direction{1.0f, 0.0f, 0.0f}; // unit, horizontal
    float speed = 0.0f;                    // m/s at this point and time
    float gust = 0.0f;                     // the raw gust term in [-1, 1], for phase-coherent effects
};

// The field. Pure, allocation-free, and a function of (position, time) alone -- evaluate it in any
// order, on any thread, and get the same answer.
[[nodiscard]] WindSample sample_wind(const WindParams& params, const glm::vec3& position,
                                     float timeSeconds) noexcept;

// The raw gust term on its own, for callers that want the shape without the direction/speed.
[[nodiscard]] float wind_gust(const WindParams& params, const glm::vec3& position,
                              float timeSeconds) noexcept;

// High-frequency flutter in [-1, 1] for foliage shading. Deliberately separate from the gust: the
// gust moves whole canopies, the flutter only shimmers their surface.
[[nodiscard]] float wind_flutter(const WindParams& params, const glm::vec3& position,
                                 float timeSeconds) noexcept;

// The unit horizontal direction the wind blows TOWARD.
[[nodiscard]] glm::vec3 wind_direction(const WindParams& params) noexcept;

} // namespace world::wind
