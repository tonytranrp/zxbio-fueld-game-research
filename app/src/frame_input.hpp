#pragma once

// Where the frame loop's input comes from (Prompt 002 goal 211).
//
// This interface exists for exactly one reason: so `voxel_harness` can drive a scripted scenario
// through the SAME frame loop, the SAME fixed-step controller and the SAME step_player() call the
// player's keyboard reaches -- not through a copy of them. The previous pass shipped `--autofly` as
// a teleport applied outside the simulation it was testing, and it reported 74 ground violations
// that were the harness's own. A harness that runs a copy of the frame loop is worse than no
// harness, because the copy drifts and then lies.
//
// It is a small virtual interface rather than a template parameter on purpose: `run_svo` is one
// large function compiled once, and making it a template would instantiate the whole frame loop
// twice for a handful of calls per frame (compile-time-performance.md rule 14 -- reach for type
// erasure when a template parameter's only job is "some type with this shape").

#include "engine/core/math.hpp"
#include "world/player/player_state.hpp"

namespace app {

// What the script (or the keyboard) wants of ONE fixed tick.
struct TickInput {
    world::player::PlayerIntent intent{};
    // A scripted `look` turns at the fixed timestep; a mouse turns at render cadence. Both end up
    // in the same apply_look(), so the live path leaves this zero and fills take_look_delta(), and
    // the scripted path does the opposite.
    glm::vec2 look_delta_pixels{0.0f};
};

class FrameInput {
public:
    FrameInput() = default;
    virtual ~FrameInput() = default;
    FrameInput(const FrameInput&) = delete;
    FrameInput& operator=(const FrameInput&) = delete;
    FrameInput(FrameInput&&) = delete;
    FrameInput& operator=(FrameInput&&) = delete;

    // Once per frame, before anything reads it.
    virtual void begin_frame() {}

    [[nodiscard]] virtual bool quit_requested() const { return false; }
    [[nodiscard]] virtual bool take_walk_toggle() { return false; }
    [[nodiscard]] virtual bool take_screenshot() { return false; }
    // Render-cadence mouse look. A 144 Hz mouse must not be sampled at the sim's 60 Hz.
    [[nodiscard]] virtual glm::vec2 take_look_delta() { return glm::vec2{0.0f}; }

    // Once per fixed tick, given the pose the simulation is at BEFORE the tick (a scripted `goto`
    // is closed-loop and needs it). `jumpEdge` is the frame's take-once press, true on at most the
    // first tick of the frame -- the caller clears it, per world/player's contract.
    [[nodiscard]] virtual TickInput tick(const glm::vec3& position, float yawRadians, float pitchRadians,
                                         bool jumpEdge) = 0;

    // A scripted run ends when its script does; a live one ends when the window closes.
    [[nodiscard]] virtual bool exhausted() const { return false; }
    // How far into the script we are, in seconds. Live input reports 0; the harness uses it to
    // resolve `capture time T`.
    [[nodiscard]] virtual float script_seconds() const { return 0.0f; }
};

} // namespace app
