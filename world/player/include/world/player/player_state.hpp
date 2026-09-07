#pragma once

#include <cstdint>

#include "engine/core/math.hpp"
#include "world/player/tuning.hpp"

namespace world::player {

// Fly is the spectator (free vertical strafe, no gravity); Walk is the body (gravity, jump,
// swimming). Collision applies in BOTH -- --noclip is the only way past it.
enum class MoveMode : std::uint8_t { Fly, Walk };

// What the body is doing, as a real state rather than a per-frame sweep result (Prompt 001 A2).
// The distinction matters because coyote time, jump buffering, head-bob and the landing dip all
// need to know what the PREVIOUS tick's stance was, which a bool recomputed each frame cannot say.
enum class Stance : std::uint8_t { Grounded, Airborne, Swimming };

// The player's own persistent physics state. Everything the fixed-step simulation carries between
// ticks lives here -- deliberately NOT in the render loop's locals (A1), so a test can drive it.
struct PlayerState {
    // WALK, not Fly (Prompt 003 goal 231). The free camera is a TOOL, and tools belong to the
    // harness and to --dev; shipping it as the default is why the owner played a spectator for
    // three passes. --fly and --noclip still exist, behind --dev.
    MoveMode mode = MoveMode::Walk;
    Stance stance = Stance::Airborne;
    float vertical_velocity = 0.0f; // world units/s, negative = falling

    // Jump timing (A2). Both count DOWN in seconds; both are pure consequences of edges and dt.
    float coyote_remaining = 0.0f; // > 0 means "still counts as grounded for a jump"
    float jump_buffer_remaining = 0.0f;

    // Eye smoothing (A3). The offset the RENDERED eye sits at relative to the physical eye; the
    // physical position is never touched by it. Negative = the view is still catching up upward.
    float eye_smooth_offset = 0.0f;

    // View polish (A6), all render-only like the above.
    float bob_distance = 0.0f; // metres walked, drives the bob phase (paces, not wall-clock)
    float landing_dip = 0.0f;  // current one-shot dip, decaying
    float fov_kick = 0.0f;     // current boost FOV offset, radians

    [[nodiscard]] bool grounded() const noexcept { return stance == Stance::Grounded; }
    [[nodiscard]] bool swimming() const noexcept { return stance == Stance::Swimming; }
};

// One tick's worth of player wishes, in the controller's own vocabulary. Deliberately not
// engine::input::InputState: world/ does not depend on the input module, and a test builds these
// directly. The app maps one to the other.
struct PlayerIntent {
    bool forward = false;
    bool back = false;
    bool left = false;
    bool right = false;
    bool up = false;   // fly: rise; swim: kick for the surface. Inert while walking on land.
    bool down = false; // fly: sink; swim: dive.
    bool boost = false;
    // An EDGE, not a level: true on exactly the tick that consumes the key press. The caller sets
    // it on the first sub-tick of the frame that saw the press and clears it for the rest --
    // holding Space must not re-arm the buffer every tick.
    bool jump_pressed = false;
};

// What the world says about the column the body is standing in. `water_surface_y` is a parameter
// rather than kSeaLevelWorld so Group E's Gerstner surface can be handed in unchanged.
struct WorldSense {
    float ground_height = 0.0f; // analytic terrain surface under the body
    float water_surface_y = kSeaLevelWorld;
};

// What one tick did, for the caller's telemetry and for the view polish that reacts to it.
struct StepResult {
    bool landed = false; // Airborne/Swimming -> Grounded on this tick
    bool jumped = false;
    bool stepped_up = false;
    bool shore_popped = false; // the swim-to-shore assist fired (A5)
    float impact_speed = 0.0f; // |downward velocity| at the moment of landing, else 0
};

// ---- pure pieces, unit-tested on their own ------------------------------------------------------

// Horizontal wish direction for this tick, already normalized and scaled to metres for `dt`.
// Fly mode moves along the full view axes (pitch included); walk mode follows YAW only, so looking
// at your feet must not slow you down.
[[nodiscard]] glm::vec3 wish_velocity(const PlayerIntent& intent, const PlayerTuning& tuning, MoveMode mode,
                                      float yawRadians, float pitchRadians, float moveSpeed) noexcept;

// The jump/coyote/buffer state machine, as a pure function of (state, edges, dt). Advances both
// timers, decides whether this tick jumps, and applies the jump impulse if so. Called once per
// fixed tick, BEFORE the sweep -- a jump is an initial velocity, not a displacement.
struct JumpOutcome {
    bool jumped = false;
};
JumpOutcome update_jump(PlayerState& state, const PlayerTuning& tuning, bool jumpPressed, float dt) noexcept;

// Buoyancy + drag + direct swim input for one tick, given how deep the feet are. Split out because
// it is exactly the piece Group E's wave surface changes.
void integrate_swim(PlayerState& state, const PlayerTuning& tuning, const PlayerIntent& intent,
                    float submersion, float dt) noexcept;

// Follow the physical eye with the smoothed one (A3). `physicalEyeY` is the truth; the returned
// offset is what the renderer adds. Smoothing is skipped (and the offset collapsed) whenever the
// body is not grounded -- a fall must not leave the view trailing behind the body.
void update_eye_smoothing(PlayerState& state, const PlayerTuning& tuning, float physicalEyeY,
                          float previousPhysicalEyeY, float dt) noexcept;

} // namespace world::player
