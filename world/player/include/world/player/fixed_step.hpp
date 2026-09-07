#pragma once

#include <algorithm>

namespace world::player {

// The fixed-timestep accumulator (Prompt 001 A1). Physics ran at render dt before this, which made
// every number in the controller -- jump apex, coyote window, the smoothing time constant --
// quietly framerate-dependent, and made "same inputs, same result" untrue between a 30 fps and a
// 144 fps machine.
//
// Standard accumulator, deliberately with no interpolation of its own: the caller asks for the
// tick count, runs exactly that many identical steps, and reads `alpha()` for the leftover if it
// wants to interpolate the render pose. Keeping the *state* here (and in PlayerState) rather than
// in the frame loop's locals is the part that makes it testable.
class FixedStepper {
public:
    // 60 Hz. Every tick uses this exact double, so the simulation is a pure function of the tick
    // COUNT -- irregular render cadence changes when ticks happen, never what they compute.
    static constexpr double kDefaultStep = 1.0 / 60.0;
    // A frame longer than this (a stall, a breakpoint, a world rebuild) is truncated instead of
    // spawning a hundred catch-up ticks that make the next frame longer still. The sim falls
    // behind wall-clock; it does not spiral.
    static constexpr double kDefaultMaxFrame = 0.25;

    constexpr FixedStepper() noexcept = default;
    constexpr explicit FixedStepper(double stepSeconds, double maxFrameSeconds = kDefaultMaxFrame) noexcept
        : step_(stepSeconds > 0.0 ? stepSeconds : kDefaultStep), max_frame_(maxFrameSeconds) {}

    // Adds one rendered frame's elapsed time and returns how many fixed ticks to run now.
    [[nodiscard]] int begin_frame(double frameSeconds) noexcept {
        accumulator_ += std::clamp(frameSeconds, 0.0, max_frame_);
        int ticks = 0;
        while (accumulator_ >= step_) {
            accumulator_ -= step_;
            ++ticks;
        }
        return ticks;
    }

    // Fraction of a tick left over, for interpolating the render pose between the previous and
    // current simulated poses. In [0, 1).
    [[nodiscard]] float alpha() const noexcept { return static_cast<float>(accumulator_ / step_); }

    [[nodiscard]] double step_seconds() const noexcept { return step_; }
    [[nodiscard]] float step_seconds_f() const noexcept { return static_cast<float>(step_); }

private:
    double step_ = kDefaultStep;
    double max_frame_ = kDefaultMaxFrame;
    double accumulator_ = 0.0;
};

} // namespace world::player
