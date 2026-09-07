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

    // ---- motion: REALISTIC SCALE, chosen (goals 232, 233) ----------------------------------------
    // The whole point of 7.8 mm voxels is human scale. A body that crosses the world at 10 m/s --
    // which is what `move_speed 40 x walk_speed_factor 0.25` used to give, a number nobody chose --
    // makes those voxels a blur and the world a diorama. So: every ground number below is the
    // measured human one, and the departures are named where they occur.
    // Sources: research/locomotion-biomechanics-physics.md 2.1, research/player-movement-in-games.md
    // 5.2.1 and 5.7(a), which recommends exactly these three speeds FOR THIS ENGINE.
    float walk_speed = 1.4f;   // m/s. Self-selected human walking speed: 1.39, band 1.3-1.5.
    float sprint_speed = 7.0f; // m/s. Fit-human sprint band 6-8; NOT Bolt's 12.32.
    // Directional penalties, ARMA's shipped values, which sit inside the measured human 70-80%
    // band. Applied anisotropically to the wish direction, not as four discrete states.
    float back_speed_factor = 0.75f;
    float lateral_speed_factor = 0.8f;
    // Ground acceleration, m/s^2. The research boxes elite human sprint acceleration at ~10 m/s^2
    // and shipped games at 10-60; 10 is the top of reality and the bottom of the games band, which
    // is the honest place for a game that has just chosen realistic speeds. Braking is faster than
    // accelerating because it is: you can plant a foot.
    // DECIDED AGAINST: the research's own critically-damped velocity spring. At sprint speed a
    // 0.25-0.5 s time constant implies a PEAK acceleration of v/tau = 14-28 m/s^2 -- above the
    // 10 m/s^2 the same document boxes as the human limit. A constant cap sits exactly on it, and
    // makes "time to sprint" a number the tuning declares (v/a) rather than an asymptote.
    float ground_accel = 10.0f;
    float ground_decel = 14.0f;
    // Fly is the dev tool and keeps its old feel exactly: `move_speed` x this on Shift.
    float fly_boost_factor = 4.0f;

    // ---- the walkable slope, as a friction cone (goal 235) --------------------------------------
    // Above this the body SLIDES instead of climbing. 40 degrees, and the band it was chosen from:
    //   * Minetti's gradient measurements span +/-45% grade = **+/-24.2 degrees**, but that is the
    //     limit of a treadmill protocol, not of human capability -- people walk up steeper scree
    //     than any treadmill will tilt to. Taking 24 degrees as a hard limit would over-read the
    //     source, and it would refuse the 31 degree slope `walk_hillside` is named for.
    //   * Mountain paths statistically optimise at 20-30% grade (11-17 degrees) -- that is the
    //     comfortable gradient, not the possible one.
    //   * Shipped engines default near 45 (Unreal 44.765, Source 45.57), a number that exists for
    //     level design with 45 degree ramps. This terrain has no ramps; it has hills at ~31 and
    //     cliffs at 75-79.
    // 40 degrees sits above every hill the generator makes and below every cliff, which is the
    // behaviour that reads as "I can walk up that, I cannot walk up THAT".
    float max_walk_slope_radians = 0.6981317f; // 40 degrees
    // A slide is not faster than a run. Terminal speed for the friction-cone slide below.
    float max_slide_speed = 7.0f;

    // Earth gravity, deliberately (goal 233). The old -32.0 was 3.26x Earth with an 8.5 m/s jump --
    // a game-convention choice that was never recorded AS a choice. Having just made the ground
    // speeds real, keeping a 3.26x gravity would be incoherent: the body would walk like a human
    // and fall like a stone. Earth scale also *helps* collision, because it caps fall speed lower
    // (60 m drop: 34 m/s at 9.81, 62 m/s at 32), and 60 m/s bodies are what stress the sweep.
    float gravity = -9.81f;
    // Apex = v0^2 / (2|g|) = 3.43^2 / 19.62 = 0.600 m. A real standing vertical is 0.4-0.5 m at a
    // ~3 m/s takeoff, so this is a deliberate ~20-50% departure upward: a 0.45 m jump does not read
    // on screen. 3.43 m/s is still within 15% of the measured human takeoff velocity.
    float jump_speed = 3.43f;

    // Horizontal motion refused at ground level may climb a ledge up to this high. 0.55 m is the
    // MESH world's relic (1 m blocks, half-metre terraces). On the svo path this is a *smoothing
    // budget* instead (A3): at 7.8 mm voxels every natural slope is sub-cm stairs, so the failure
    // mode is micro-jitter, not blocked stairs -- see kSvoStepHeight below.
    float step_height = 0.55f;

    // ---- jump timing (A2) -------------------------------------------------------------------------
    // `jump_speed` and `gravity` live above, together, because they are ONE decision (goal 233);
    // tests pin both the apex and the fall time so neither can move silently.
    float coyote_time = 0.10f;      // grounded credit that survives walking off a ledge
    float jump_buffer_time = 0.10f; // a press this early still fires on landing

    // ---- swimming (A5) --------------------------------------------------------------------------
    // You WADE before you swim, and the two thresholds differ (goal 236). Without this the
    // `inWater` predicate is a bare comparison against a surface that moves, so a body standing at
    // the waterline crosses it twice per wave: MEASURED at 25 stance transitions in 30 s at
    // x=111.5 on the shoreline, against a ~4 s dominant wave period. Hysteresis, with a reading:
    // you start swimming once you are thigh-deep and stop once only your shins are under.
    float swim_enter_depth = 0.6f;  // feet this far below the surface before Swimming begins
    float swim_exit_depth = 0.2f;   // and it ends once less than this remains
    float swim_speed = 3.5f;        // direct vertical velocity while Space/Ctrl held under water
    float shore_pop_impulse = 5.0f; // upward kick when swimming into a climbable lip
    float shore_pop_probe = 0.6f;   // how far above the blocked body to look for air

    // ---- eye smoothing (A3) ---------------------------------------------------------------------
    // The FEET stay exact (collision, the walk-violation counter, everything mechanical). Only the
    // rendered eye lags, with this time constant, and only while grounded.
    //
    // Goal 240's verdict: PERCEPTIBLE AND INTENDED, and the thing it hides is worse than the lag it
    // adds. Walking a 30 degree slope at 1.4 m/s climbs 0.7 m/s, which over 7.8 mm voxels is a
    // ~90 Hz staircase; the eye velocity that sawtooth injects is orders of magnitude past the
    // 2.13 cm/s vertical detection threshold. A 0.10 s time constant is itself well above any
    // latency threshold and therefore is not free -- but it is buying the removal of a larger
    // artefact, which is the trade this constant exists to make.
    float eye_smooth_tau = 0.10f; // seconds; 0 disables smoothing entirely
    // 0.05, not 0.25. The old clamp was 32 voxels of lag for an effect whose entire job is hiding a
    // 7.8 mm staircase, and goal 240's captured landing sequence caught what that cost: the total
    // render-only eye offset peaked at **+0.1117 m** two frames after a landing -- the eye floating
    // 11 cm ABOVE the body while the landing dip was pulling it 3.4 cm DOWN. The smoothing swamped
    // the dip by 3x, in the opposite direction, so a landing read as the view floating rather than
    // absorbing. 0.05 m is 6 voxels: plenty for the staircase, and it makes the dip the dominant
    // term on a landing, which is what it was written to be.
    //
    // It also makes the two clamps agree about the same thing. `polish_max_offset` bounds the
    // polish and this bounds the smoothing, and "the eye is not where the body is" is ONE
    // phenomenon -- two independent budgets that stack to 0.30 m was nobody's decision.
    float eye_smooth_max_lag = 0.05f;

    // ---- view polish (A6), re-derived against perception thresholds (goal 240) --------------------
    //
    // Every number here is now checked against `research/human-movement-and-perception-research.md`
    // Part 2: vertical translation detection ~2.13 cm/s (median, 2AFC); retinal slip tolerated to
    // ~4 deg/s for acuity, degrading rapidly past ~6.
    //
    // BOB FREQUENCY. This is cycles per METRE, so its temporal frequency is speed-dependent: at the
    // old 10 m/s walk it ran at 19 Hz, which is not a bob, it is a flicker. Goal 232's 1.4 m/s made
    // it 2.66 Hz by accident. 1.36 cycles/m puts it at **1.9 Hz** at walking pace -- the measured
    // human step frequency (Moore et al.), which is what a head bob is imitating.
    float bob_frequency = 1.36f;
    // BOB AMPLITUDE, derived rather than chosen. Peak vertical head velocity is 2*pi*f*A; the
    // constraint is that the gaze perturbation while fixating the ground ~4 m ahead stays inside
    // the 4 deg/s acuity threshold: A <= tan(4 deg) * 4 m / (2*pi*1.9 Hz) = 0.0234 m. The shipped
    // 0.025 at the resulting 2.66 Hz gave 5.97 deg/s -- past the acuity threshold and into the
    // 6 deg/s rapid-degradation band. 0.020 m lands at 3.4 deg/s with margin, and is still 19x the
    // 2.13 cm/s detection threshold (24 cm/s peak), so it is emphatically visible, just not
    // acuity-costing. PERCEPTIBLE AND INTENDED.
    float bob_amplitude = 0.020f;
    // LANDING DIP. A landing from the 0.6 m jump apex arrives at 3.43 m/s, giving a 3.4 cm dip that
    // decays with tau 0.12 s -- a peak eye velocity near 29 cm/s, 13x the detection threshold.
    // PERCEPTIBLE AND INTENDED.
    float landing_dip_per_speed = 0.010f;
    // 0.045, not 0.06: the old maximum EXCEEDED `polish_max_offset` below, so the budget clamp
    // truncated the tail of a hard landing and the two constants disagreed about what was allowed.
    // 0.045 leaves room for the bob inside the same 5 cm budget.
    float landing_dip_max = 0.045f;
    float landing_dip_tau = 0.12f;
    // 0.075 rad = 4.3 deg vertical, which at 16:9 is ~7.5 deg horizontal -- inside the +5-8 deg
    // hFOV-on-sprint band `research/player-movement-in-games.md` 5.7(c) recommends, and in the
    // direction the perception literature is unambiguous about (wider reads as faster).
    // PERCEPTIBLE AND INTENDED.
    float fov_kick_radians = 0.075f; // added to the lens while sprinting (goal 234: by SPEED)
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
