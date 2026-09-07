#include "world/water/gerstner.hpp"

#include <algorithm>
#include <cmath>

namespace world::water {

namespace {

constexpr float kTwoPi = 6.283185307179586f;

// How the field is derived from one wind speed. Art direction with a physical shape: a stronger
// wind builds LONGER waves as well as taller ones (the fetch-limited growth every sea state table
// shows), which is why the crest spacing opens up between --wind-speed 0.5 and 8 rather than the
// same chop just getting bigger.
constexpr float kBaseWavelengthPerSpeedSq = 0.80f; // metres per (m/s)^2
constexpr float kMinWavelength = 1.5f;
constexpr float kMaxWavelength = 60.0f;
constexpr float kAmplitudePerWavelength = 0.011f; // crest height as a fraction of wavelength
// The total steepness the field is allowed to spend (sum of Q*k*A). 1.0 is where a Gerstner
// surface starts folding through itself; 0.6 keeps real headroom.
constexpr float kSteepnessBudget = 0.60f;
// Each successive component is shorter and turned further off the wind. The spread is what stops
// the sea reading as corduroy.
//
// The RANGE here was set by a viewed capture, not by taste. A first version scaled the components
// 1.00/0.61/0.37/0.19, which at --wind-speed 8 makes a field of 51 m down to 9.7 m waves -- all
// swell, no chop, and the water read flatter than the fixed ripple lattice it replaced, because
// from a shoreline you see barely two wavelengths of it. A real sea carries long swell and short
// chop AT THE SAME TIME (that is what a spectrum is), so the range spans a factor of ~14 instead
// of ~5: at 8 m/s that is 51, 23, 9.2 and 3.6 m together.
constexpr std::array<float, kWaveCount> kLengthScale{1.00f, 0.45f, 0.18f, 0.07f};
// Amplitude follows each component's OWN wavelength, with only a mild falloff -- short chop is
// steep in reality, and a scale that fell as fast as the length did made the short components
// invisible (0.6 cm at 8 m/s) while still spending their share of the steepness budget.
constexpr std::array<float, kWaveCount> kAmplitudeScale{1.00f, 0.80f, 0.60f, 0.45f};
constexpr std::array<float, kWaveCount> kAngleOffset{0.00f, 0.61f, -0.48f, 1.02f}; // radians

} // namespace

float GerstnerWave::omega() const noexcept {
    return std::sqrt(kGravity * wavenumber);
}

WaveField make_wave_field(const wind::WindParams& wind) noexcept {
    WaveField field;
    const float speed = std::max(0.0f, wind.base_speed);
    if (speed <= 0.0f) {
        return field; // glass: every amplitude stays zero, so --no-wind is still water by construction
    }
    const float baseLength =
        std::clamp(kBaseWavelengthPerSpeedSq * speed * speed, kMinWavelength, kMaxWavelength);

    float totalSteepness = 0.0f;
    for (std::size_t i = 0; i < kWaveCount; ++i) {
        GerstnerWave& w = field.waves[i];
        const float angle = wind.base_angle_radians + kAngleOffset[i];
        w.direction = glm::vec2{std::cos(angle), std::sin(angle)};
        const float wavelength = baseLength * kLengthScale[i];
        w.wavenumber = kTwoPi / wavelength;
        w.amplitude = wavelength * kAmplitudePerWavelength * kAmplitudeScale[i];
        totalSteepness += w.wavenumber * w.amplitude;
    }
    // Distribute the budget: every component gets the same Q, chosen so the SUM of Q*k*A is the
    // budget. Doing it per-component instead would let the short waves fold on their own.
    const float q = totalSteepness > 0.0f ? kSteepnessBudget / totalSteepness : 0.0f;
    for (GerstnerWave& w : field.waves) {
        w.steepness = q;
    }
    return field;
}

float wave_height(const WaveField& field, float x, float z, float timeSeconds) noexcept {
    float y = 0.0f;
    for (const GerstnerWave& w : field.waves) {
        if (w.amplitude <= 0.0f) {
            continue;
        }
        const float phase = w.wavenumber * (w.direction.x * x + w.direction.y * z) - w.omega() * timeSeconds;
        y += w.amplitude * std::sin(phase);
    }
    return y;
}

WaveSurface wave_surface(const WaveField& field, float x, float z, float timeSeconds) noexcept {
    WaveSurface out;
    out.position = glm::vec3{x, 0.0f, z};
    // Normal accumulators, from the analytic derivative of the same sum: the standard Gerstner
    // formulation, never a finite difference of the height -- that disagrees with the crests at
    // exactly the steepnesses that make crests worth having.
    float nx = 0.0f;
    float nz = 0.0f;
    float ny = 1.0f;
    for (const GerstnerWave& w : field.waves) {
        if (w.amplitude <= 0.0f) {
            continue;
        }
        const float phase = w.wavenumber * (w.direction.x * x + w.direction.y * z) - w.omega() * timeSeconds;
        const float s = std::sin(phase);
        const float c = std::cos(phase);
        const float qa = w.steepness * w.amplitude;
        const float wa = w.wavenumber * w.amplitude;
        out.position.x += qa * w.direction.x * c;
        out.position.z += qa * w.direction.y * c;
        out.position.y += w.amplitude * s;
        nx -= w.direction.x * wa * c;
        nz -= w.direction.y * wa * c;
        ny -= w.steepness * wa * s;
    }
    out.normal = glm::normalize(glm::vec3{nx, std::max(ny, 0.05f), nz});
    return out;
}

float shore_fade(const WaveField& field, float depth) noexcept {
    if (depth <= 0.0f) {
        return 0.0f;
    }
    // Judged against the LONGEST component, because that is the one that touches bottom first: a
    // swell feels a shelf that a ripple crosses without noticing.
    float longest = 0.0f;
    for (const GerstnerWave& w : field.waves) {
        if (w.amplitude > 0.0f && w.wavenumber > 0.0f) {
            longest = std::max(longest, kTwoPi / w.wavenumber);
        }
    }
    if (longest <= 0.0f) {
        return 0.0f;
    }
    // Full amplitude in water deeper than half a wavelength (research §2.3's own criterion), fading
    // smoothly to flat at the waterline. smoothstep rather than a linear ramp so the transition has
    // no visible edge where it starts.
    const float t = std::clamp(depth / (0.5f * longest), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

} // namespace world::water
