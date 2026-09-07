#pragma once

#include "world/materials/materials.hpp"

namespace world::player {

// Every number the player controller has, in one struct (Prompt 001 A6: "all parameters constants
// in one place"). Defaults are the shipped feel; the app overrides a few from CLI flags
// (--step-height, --no-view-polish) and the two renderer paths differ in exactly one field.
//
// A plain struct passed by const&, not a policy template parameter: these are art-direction values
// a capture session wants to sweep from the command line, not axes the type system should fix.
struct PlayerTuning {
    // ---- body (the box the sweep moves; the eye sits eye_height above the feet) ----------------
    float eye_height = 1.7f;
    float body_half_width = 0.3f;
    float body_height = 1.75f;

    // ---- motion --------------------------------------------------------------------------------
    float gravity = -32.0f;          // world units/s^2, voxel-scale (not Earth's 9.81)
    float walk_speed_factor = 0.25f; // walking is deliberately slower than flying
    float boost_factor = 4.0f;       // Shift

    // Horizontal motion refused at ground level may climb a ledge up to this high. 0.55 m is the
    // MESH world's relic (1 m blocks, half-metre terraces). On the svo path this is a *smoothing
    // budget* instead (A3): at 7.8 mm voxels every natural slope is sub-cm stairs, so the failure
    // mode is micro-jitter, not blocked stairs -- see kSvoStepHeight below.
    float step_height = 0.55f;

    // ---- jump (A2) ------------------------------------------------------------------------------
    // apex = v0^2 / (2|g|). 8.5 m/s against -32 m/s^2 gives 1.129 m, inside the brief's 1.0-1.25 m
    // band; the test pins that arithmetic so a gravity change can't silently move the apex.
    float jump_speed = 8.5f;
    float coyote_time = 0.10f;      // grounded credit that survives walking off a ledge
    float jump_buffer_time = 0.10f; // a press this early still fires on landing

    // ---- swimming (A5) --------------------------------------------------------------------------
    float swim_speed = 3.5f;        // direct vertical velocity while Space/Ctrl held under water
    float shore_pop_impulse = 5.0f; // upward kick when swimming into a climbable lip
    float shore_pop_probe = 0.6f;   // how far above the blocked body to look for air

    // ---- eye smoothing (A3) ---------------------------------------------------------------------
    // The FEET stay exact (collision, the walk-violation counter, everything mechanical). Only the
    // rendered eye lags, with this time constant, and only while grounded.
    float eye_smooth_tau = 0.10f;     // seconds; 0 disables smoothing entirely
    float eye_smooth_max_lag = 0.25f; // hard clamp, so a bug can never detach the view from the body

    // ---- view polish (A6) -------------------------------------------------------------------------
    float bob_amplitude = 0.025f;         // metres of vertical sway at full walk speed
    float bob_frequency = 1.9f;           // cycles per metre travelled (paces, not seconds)
    float landing_dip_per_speed = 0.010f; // metres of dip per m/s of impact
    float landing_dip_max = 0.06f;
    float landing_dip_tau = 0.12f;
    float fov_kick_radians = 0.075f; // added to the lens while boosting
    float fov_kick_tau = 0.15f;
    // The whole polish budget: A6's Check is that polish never moves the eye more than 5 cm from
    // the physical pose, and this is the constant that enforces it rather than hoping the sum of
    // the terms above stays small.
    float polish_max_offset = 0.05f;
};

inline constexpr PlayerTuning kDefaultTuning{};

// The svo path's step allowance (A3). 0.04 m is ~5 voxels at the default 7.8 mm finest edge --
// enough to absorb the sub-centimetre staircase a natural slope makes, far too small to climb
// anything a player would read as a step.
inline constexpr float kSvoStepHeight = 0.04f;

// Where the sea is. A world constant, not a material property: the material says how you swim
// (below), the world says where the water is.
inline constexpr float kSeaLevelWorld = 0.0f;

// The Water material's own swim numbers -- the camera asks the registry, it does not restate them.
inline constexpr materials::LiquidPhysics kWaterPhysics =
    materials::properties_of(materials::MaterialID::Water).liquid;

} // namespace world::player
