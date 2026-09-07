#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>

#include "render/diligent/render_context.hpp"

namespace render::diligent {

// Auto-exposure, metered at the crosshair (Prompt 003 goal 237).
//
// `research/eye-camera-and-rendering.md` 5.7(b): the adaptation state that matters perceptually is
// pooled over roughly 6 degrees around fixation (Vangorp et al. 2015), so a bright sky at the top of
// the screen should not crush a dark valley the player is looking into. The pooling radius is
// computed from the live FOV and viewport, never from a pixel count.
//
// WHERE THE WORK HAPPENS, and why. The reduction is on the GPU (two passes, scene -> 64x64 -> 8x8)
// and the ADAPTATION is on the CPU. That looks backwards -- a GPU ping-pong would avoid a readback
// entirely -- but goal 237's check wants the exposure TRACE in the frame report, which needs the
// number on the CPU anyway. Given that, doing the adaptation there too removes a shader, removes a
// ping-pong pair of targets, and makes the whole two-timescale rule ordinary testable C++. The
// readback runs N frames behind through a ring of staging textures, so nothing ever stalls; at
// 120 fps that lag is ~25 ms against time constants of 0.1-1.0 s, which is under 3% of the fast
// one and invisible.
struct ExposureSettings {
    bool enabled = true;
    // Metering: true = the crosshair mask above, false = a flat frame average. The false case is
    // not a fallback, it is goal 237's A/B: the same pose captured both ways is how "is the mask
    // actually better" gets an answer from looking rather than from theory.
    bool crosshair_metering = true;
    // Half-angle of the pooling mask, degrees. 3.0 makes the 6-degree pool the research names.
    float pooling_half_angle_deg = 3.0f;
    // What the metered region is mapped TO: the exposure applied is `key / 2^adapted`.
    //
    // 0.36, NOT the photographic 0.18. Applying 18% grey here is a category error: this renderer's
    // scene values are authored artist colours sitting near 0.5, not physically scaled radiance, so
    // mapping them to a photographic middle grey is a global REGRADE rather than an exposure.
    // Measured: with 0.18 the shipped look darkened 28% everywhere (mean level 136 -> 98) at a pose
    // whose exposure should have been neutral.
    //
    // 0.36 is 2^-1.47, the luminance this world's typical daylight pose actually meters at, so the
    // multiplier is ~1.0 there and auto-exposure becomes what it should be: a RELATIVE correction
    // that opens up a dark valley and reins in a bright sky, leaving the reference scene alone.
    // Prompt 005 owns the look and may well want a different number; `--exposure-key` is the knob.
    float key = 0.36f;
    // The two timescales, seconds (5.7(b): "fast up, slow down", which accidentally mimics the
    // light/dark adaptation asymmetry at ~1000x speed). Brightening is the urgent direction -- you
    // walk out of a cave and need to see NOW -- and darkening is the one the eye takes minutes over.
    float adapt_up_seconds = 0.35f;
    float adapt_down_seconds = 1.20f;
    // Clamps on the adapted log2 luminance, so a frame of pure sky or pure cave cannot drive the
    // exposure somewhere it takes ten seconds to come back from.
    float min_log_luminance = -6.0f;
    float max_log_luminance = 4.0f;
    // Pin the adapted level to a fixed EV instead of measuring. Not a fallback and not a hack: it is
    // how goal 238's check gets the SAME bright source at two different adaptation levels, which is
    // otherwise impossible to arrange without also changing what is on screen. NaN = measure.
    float pinned_log2 = std::numeric_limits<float>::quiet_NaN();

    [[nodiscard]] bool pinned() const noexcept { return !std::isnan(pinned_log2); }
};

// What one frame measured, for the report and for the tests.
struct ExposureSample {
    bool valid = false;         // false until the first readback lands
    float measured_log2 = 0.0f; // the mask-weighted mean of log2(luminance) over the frame
    float adapted_log2 = 0.0f;  // the two-timescale state that follows it
    float exposure = 1.0f;      // key / 2^adapted, what the composite multiplies by
    bool brightening = false;   // which time constant was in effect
};

// The pure adaptation rule, exposed so a test can drive it without a GPU. Advances `adapted`
// toward `measured` with the up or down time constant, and clamps. Returns the new value.
[[nodiscard]] float adapt_log_luminance(float adapted, float measured, const ExposureSettings& settings,
                                        float dt) noexcept;

// The pooling mask's radius as a fraction of the half-height of the screen plane, from the vertical
// FOV. Separate and pure because "compute the pixel radius from the actual FOV and viewport" is the
// part of goal 237 most likely to be got wrong silently, and this is the piece a test can pin.
[[nodiscard]] float pooling_tangent(float poolingHalfAngleDeg) noexcept;

class AutoExposure {
public:
    explicit AutoExposure(RenderContext& context);
    ~AutoExposure();

    AutoExposure(const AutoExposure&) = delete;
    AutoExposure& operator=(const AutoExposure&) = delete;

    void set_settings(const ExposureSettings& settings) noexcept;
    [[nodiscard]] const ExposureSettings& settings() const noexcept;

    // Meter the scene target and advance the adaptation. Call before the composite pass, with the
    // camera's vertical FOV and the frame's own delta time.
    void measure(float verticalFovRadians, float dtSeconds);

    // The exposure multiplier the composite should apply. 1.0 while disabled or before the first
    // readback lands, so a frame is never black waiting for the pipeline to fill.
    [[nodiscard]] float exposure() const noexcept;
    [[nodiscard]] const ExposureSample& last_sample() const noexcept;

    struct Impl;

private:
    std::unique_ptr<Impl> impl_;
};

} // namespace render::diligent
