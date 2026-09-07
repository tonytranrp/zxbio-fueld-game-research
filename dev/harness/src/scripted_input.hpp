#pragma once

// The scripted FrameInput (Prompt 002 goal 211). It drives a dev::scenario::MotionDriver into the
// SAME step_player() call the keyboard reaches, at the SAME fixed timestep, through the SAME
// app::update_camera_phase -- which is the whole reason app/src/frame_input.hpp exists.

#include <utility>

#include "dev/scenario/motion_script.hpp"
#include "frame_input.hpp"

namespace dev::harness {

class ScriptedInput final : public app::FrameInput {
public:
    ScriptedInput(std::span<const scenario::Segment> segments, float stepSeconds)
        : driver_(segments, stepSeconds), step_(stepSeconds) {}

    [[nodiscard]] app::TickInput tick(const glm::vec3& position, float yawRadians, float pitchRadians,
                                      bool /*jumpEdge*/) override {
        scenario::Pose pose;
        pose.position = position;
        pose.yaw_deg = glm::degrees(yawRadians);
        pose.pitch_deg = glm::degrees(pitchRadians);
        const scenario::InputFrame frame = driver_.advance(pose);
        seconds_ += step_;
        app::TickInput out;
        out.intent = frame.intent;
        out.look_delta_pixels = frame.look_delta_pixels;
        return out;
    }

    // The script's own end. Note that this is checked at the TOP of the frame, so the last tick's
    // effects are still rendered and captured before the loop stops.
    [[nodiscard]] bool exhausted() const override { return driver_.finished(); }
    [[nodiscard]] float script_seconds() const override { return seconds_; }

    [[nodiscard]] std::size_t ticks_run() const noexcept { return ticks_; }

private:
    scenario::MotionDriver driver_;
    float step_ = 1.0f / 60.0f;
    float seconds_ = 0.0f;
    std::size_t ticks_ = 0;
};

} // namespace dev::harness
