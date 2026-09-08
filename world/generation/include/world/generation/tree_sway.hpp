#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "engine/core/math.hpp"
#include "world/generation/tree_skeleton.hpp"
#include "world/wind/wind_field.hpp"

namespace world::generation {

// Hierarchical spring sway (Prompt 007 goal 335 = docs/goals.md goal 190).
//
// WHAT THIS IS. A grown skeleton (goal 186) is decomposed into BRANCH CHAINS, and each chain is one
// damped oscillator hanging off the chain that carries it.
// `research/tree-motion-growth-and-appearance.md` §8 step 2 names exactly this architecture -- "let
// each major branch be its own semi-independent spring-mass sub-system hanging off the trunk's
// motion, rather than a single global sway applied uniformly" -- and §3.3 says why: multiple-
// resonance damping (Spatz & Theckes 2013) is what makes real tree motion read as alive, and it
// EMERGES from that structure rather than needing a hand-tuned damping constant.
//
// ---------------------------------------------------------------------------------------------
// THE DEGREE OF FREEDOM IS THE BRANCH, NOT THE SEGMENT -- and that was measured, not assumed
// ---------------------------------------------------------------------------------------------
//
// The first version of this file gave every SEGMENT its own oscillator, with the finite-difference
// beam stiffness `EI/ds` and the inertia of everything above it. That is a correct STATIC model --
// its compliances telescope to the textbook `F L^3 / 3EI` -- and a wrong DYNAMIC one, which the
// step-response test caught immediately: the pole rang at **1.03 Hz against a predicted 0.26**.
//
// The reason is worth writing down, because it is not obvious and it is the whole design. A serial
// chain's equations of motion are `M theta.. + K theta = Q` with K diagonal but **M dense**: joint i
// and joint j share every mass above both of them. Dropping the off-diagonal terms -- which is what
// "each joint is its own oscillator with the inertia above it" means -- leaves each joint ringing at
// `sqrt(k_i / J_i)`, and for the root of a 40-segment pole that is 3x the collective fundamental,
// because one joint of the chain is far stiffer than the chain. The coupling is not a refinement
// here; it is the mode. And the honest fix is not to solve a dense 300x300 system per tree per
// frame -- it is to have fewer, better coordinates.
//
// So: chains. Each chain follows the THICKEST CHILD at every branch point, which makes the trunk
// chain run the full height of the tree (that is what "trunk fundamental" means) and every side
// branch its own cantilever hanging off it. Within a chain, segment j takes a fixed share `w_j` of
// the chain's rotation, with `w_j` proportional to `(L - s_j) * ds_j` -- the static curvature under
// a tip load, which is the deflection shape a swaying cantilever actually takes.
//
// That single choice makes the generalized stiffness and inertia come out right ANALYTICALLY, which
// is why this version passes the test the first one failed. For a uniform beam:
//
//     K = sum_j k_j w_j^2 = 4EI / 3L        (with k_j = EI/ds_j)
//     Pbar = sum_j w_j P_j = L/3            (the weighted mean pivot)
//     lever = L - Pbar = 2L/3
//     tip stiffness = K / lever^2 = 3EI / L^3     <-- EXACTLY the cantilever
//     J = integral rho A (s - L/3)^2 ds = rho A L^3 / 9
//     omega^2 = K / J = 12 EI / (rho A L^4)
//
// against Rayleigh's `12.727 EI / (rho A L^4)` for the same beam -- 2.9% apart, from two independent
// shape functions. The test asserts they agree; identity would have meant one was computing the
// other.
//
// ---------------------------------------------------------------------------------------------
// THE OTHER THREE DECISIONS
// ---------------------------------------------------------------------------------------------
//
// 1. THE STATE IS A ROTATION VECTOR, BECAUSE THAT IS WHAT MAKES POSING O(1) PER SEGMENT.
//    A point p downstream of a joint at P moves by `w x (p - P)` to first order. Summed over the
//    ancestors of p that is
//
//        offset(p) = (sum_a w_a) x p  -  sum_a (w_a x P_a)
//
//    which is LINEAR IN p: two vec3 accumulators carried down a single forward pass pose the whole
//    tree -- no per-segment ancestor walk, no matrices. Tracking a scalar bend angle and a bend
//    direction instead would need the ancestor chain re-walked for every segment, ~40x the work at
//    this depth for the same first-order answer.
//
//    Each chain's rotation vector has the component ALONG ITS OWN AXIS projected out: that component
//    is torsion, which is the leaf-flutter degree of freedom (§4.3) -- it lives in the shading term
//    goal 185 already shipped, and simulating it here as well would double-count it. What is left is
//    bending in both transverse directions.
//
//    The first version projected onto the HORIZONTAL PLANE instead, on the reasoning that a trunk is
//    vertical so its torsion axis is Y. That is true of a trunk and false of everything else, and the
//    consequence was found by LOOKING AT A FRAME rather than by any test: a horizontal branch broadside
//    to the wind has a purely vertical bending torque, the horizontal projection deleted all of it,
//    and half the crown could not move sideways at all. `--verify-frame`-style numbers would never
//    have shown that; the picture did, immediately.
//
// 2. CHAINS ABOVE `rigid_above_hz` ARE QUASI-STATIC, NOT SIMULATED.
//    A twig chain is stiff and nearly massless, so its natural frequency runs into the hundreds of
//    Hz -- an explicit integrator would need a step nobody is going to pay for, and the motion it
//    produced would be far above what a 60 Hz frame can show. Above the cutoff a chain simply takes
//    its static deflection. That is not a dodge around a stability problem, it is the correct
//    treatment of a mode whose period is short against the timestep, and it leaves the fast shimmer
//    to `wind_flutter`, which is the term written for it.
//
// 3. THE CHILD PUSHES BACK. Without this, branches would move independently but no energy would ever
//    leave the trunk, and §3.3's headline claim would be decoration. The reaction torque a child
//    exerts on its parent while being angularly accelerated is `-J_c * alpha_c`, so
//
//        alpha_parent += -(J_c / J_parent) * alpha_c_relative
//
//    summed over child chains, taken from the PREVIOUS step (an explicit staggered coupling; a
//    simultaneous solve would be an implicit system per tree per frame). That term is the entire
//    tuned-mass-damper mechanism, and `test_tree_sway.cpp` MEASURES whether it bleeds trunk energy
//    rather than asserting that it does.
//
// Everything is a pure function of (skeleton, params, wind, time): same inputs, same bytes, on every
// thread and in every tool.

// --------------------------------------------------------------------------------- material & model

struct SwayParams {
    // research/tree-motion-growth-and-appearance.md §9, the FE-simulation defaults for green wood.
    float youngs_modulus_pa = 9.5e9f;
    float wood_density = 800.0f;

    // Leaves are a MASS effect on frequency, not an aerodynamic one -- §3.4 checked that directly and
    // found the drag contribution to the frequency shift is under 1%. The measured shift is that a
    // tree sways 18-19% FASTER once its leaves fall, so leaves are the fraction of effective sway
    // mass that reproduces it: f_bare/f_leafy = 1/sqrt(1 - lambda) = 1.185 at lambda = 0.28782.
    //
    // This constant is therefore not authored. It IS the measured 18.5% shift, re-expressed.
    float leaf_mass_fraction = 0.28782f;
    bool leaves = true;

    // §3.4's other measured pair: damping ratio 8.6% in full leaf, 3.9% bare. Leaves roughly double
    // the damping. Any additional damping from branching is ON TOP of these and is measured, not
    // added here -- see `test_tree_sway.cpp`'s branch-damping case.
    float damping_leaf_on = 0.086f;
    float damping_leaf_off = 0.039f;

    // Drag on a reconfiguring crown: F ~ U^(2+V), the Vogel exponent (§4.2). V = 0 is rigid-body
    // quadratic drag; real plants measure -0.2 to -1.2. -0.7 is the middle of that measured band and
    // is stated as a choice within it, not as a measured value for any particular species. Per
    // species this is overridden from `SpeciesParams::flutter_response` by `sway_params_for`.
    float vogel_exponent = -0.7f;

    // 0.5 * rho_air * C_d * (self-shelter factor), all folded into one number.
    //
    // THIS IS THE ONE AUTHORED CONSTANT IN THE FILE and it is worth being blunt about that: the
    // research gives the drag LAW (the exponent above) but no absolute drag calibration for a whole
    // crown, and a crown's own leaves shelter each other by an amount nobody measured for this tree.
    // Calibrated so the reference sycamore leans about 1.5% of its height into the default 4 m/s
    // breeze -- a visible but gentle lean, which is what a fresh breeze does to a real tree.
    float drag_pressure = 0.30f;

    // Above this natural frequency a chain is treated as quasi-static (decision 2 above). 8 Hz at the
    // 120 Hz sway tick leaves omega*dt = 0.42, comfortably inside semi-implicit Euler's stability
    // bound of 2, and 8 Hz is already above the 3-5 Hz band §6.2 measures for leaf flutter.
    float rigid_above_hz = 8.0f;
    // ...and a floor, so a numerically degenerate chain (zero radius, zero downstream mass) cannot
    // produce a NaN or a mode with a multi-minute period.
    float min_hz = 0.02f;

    bool branch_reaction = true; // decision 3; off is the A/B that measures what it is worth
};

// Species flavour, so an aspen and a conifer do not sway identically. `flutter_response` is already
// the species' petiole-stiffness knob (§4.4); a springier species also reconfigures less, so it sits
// nearer the rigid V = 0 end of the Vogel band.
[[nodiscard]] SwayParams sway_params_for(const SpeciesParams& species, const SwayParams& base = {}) noexcept;

// ------------------------------------------------------------------------------------------- state

// One entry per CHAIN for the dynamics, one per SEGMENT for the pose. Structure-of-arrays because
// the step loop touches four chain arrays and nothing else.
struct SwayState {
    // --- per chain (the degrees of freedom) ---
    std::vector<glm::vec3> angle; // the chain's rotation vector, horizontal
    std::vector<glm::vec3> velocity;
    std::vector<glm::vec3> accel;         // relative angular acceleration from the LAST step (decision 3)
    std::vector<float> omega;             // rad/s
    std::vector<float> damping;           // 2*zeta*omega, pre-multiplied
    std::vector<float> stiffness;         // K = sum_j k_j w_j^2, N m / rad
    std::vector<float> inertia;           // J about the chain's weighted mean pivot, kg m^2
    std::vector<float> drag_area;         // downstream leaf area + projected wood area, m^2
    std::vector<glm::vec3> axis;          // unit direction of the chain, base to tip
    std::vector<glm::vec3> mean_pivot;    // Pbar, tree-local
    std::vector<glm::vec3> drag_centroid; // drag-weighted centroid of everything downstream
    std::vector<std::int32_t> chain_parent;
    std::vector<std::uint8_t> quasi_static; // 1 = above rigid_above_hz, no dynamics

    // Step scratch, owned by the state so a frame allocates nothing: the accumulated child reaction
    // torque (decision 3) and each chain's ABSOLUTE angular acceleration, which is what a child sees
    // as base excitation.
    std::vector<glm::vec3> reaction;
    std::vector<glm::vec3> abs_accel;

    // --- per segment (the pose) ---
    std::vector<std::int32_t> chain_of; // which chain a segment belongs to
    std::vector<float> weight;          // its share w_j of that chain's rotation; sums to 1 per chain
    std::vector<glm::vec3> pivot;       // segment start, tree-local
    std::vector<std::int32_t> parent;   // segment parent, copied for the pose walk

    glm::vec3 base{0.0f}; // world position the local frame is relative to

    // The three driving constants, copied off `SwayParams` at bake time so a step needs only the
    // state and the wind -- a species exponent that lived only in the params would otherwise be
    // silently replaced by the default the moment anyone called `step_sway` without them.
    float drag_pressure = 0.30f;
    float vogel_exponent = -0.7f;
    bool branch_reaction = true;

    /// The number of DEGREES OF FREEDOM -- chains, not segments. This is what the step loop costs
    /// and what `sway_joint_budget` budgets.
    [[nodiscard]] std::size_t size() const noexcept { return angle.size(); }
    [[nodiscard]] std::size_t segment_count() const noexcept { return chain_of.size(); }
    [[nodiscard]] bool empty() const noexcept { return angle.empty(); }
};

// Bake the constants. The skeleton must already have been through `apply_pipe_model` -- radii and
// leaf areas are the whole input to the mass and stiffness model, and a skeleton without them would
// silently produce a tree made entirely of the `min_radius` twig.
[[nodiscard]] SwayState make_sway_state(const TreeSkeleton& skeleton, const SwayParams& params);

// The natural frequency of the tree's fundamental mode, Hz -- the Rayleigh estimate for the whole
// trunk-to-tip cantilever, which is the number §3.1's f0 = 0.26 Hz sycamore is quoted as. This is
// the ANALYTIC prediction; `test_tree_sway.cpp` measures the simulated chain's own period and checks
// the two agree, which is the independent confirmation that makes either worth believing.
[[nodiscard]] float fundamental_frequency_hz(const TreeSkeleton& skeleton, const SwayParams& params);

// The same estimate straight from geometry, for the closed-form check and for callers that have a
// height and a diameter but no skeleton. `dbh` is diameter at breast height (1.3 m), metres.
[[nodiscard]] float cantilever_frequency_hz(float dbh_m, float height_m, const SwayParams& params) noexcept;

// ------------------------------------------------------------------------------------------ driving

// Advance one fixed step. `dt` should be `kSwayTick` or a divisor of it; the caller owns the
// accumulator. `windTime` is the wind field's own clock, so two trees stepped in different orders
// still see the same gust.
void step_sway(SwayState& state, const wind::WindParams& wind, float windTime, float dt) noexcept;

// The recommended fixed tick. 120 Hz: fast enough that `rigid_above_hz` can sit at 8 Hz (above the
// leaf-flutter band) with omega*dt = 0.42, and cheap enough that two sub-steps cover a 60 Hz frame.
inline constexpr float kSwayTick = 1.0f / 120.0f;

// Apply the state to the skeleton, writing posed endpoints. `outStart`/`outEnd` are resized to the
// skeleton's segment count. Single forward pass, O(1) per segment (decision 1).
void pose_skeleton(const SwayState& state, const TreeSkeleton& rest, std::vector<glm::vec3>& outStart,
                   std::vector<glm::vec3>& outEnd);

// Horizontal displacement of the highest point of the tree, metres -- the scalar a step-response
// test watches, and the scalar the renderer will eventually want per tree.
[[nodiscard]] glm::vec3 tip_displacement(const SwayState& state, const TreeSkeleton& rest) noexcept;

// ------------------------------------------------------------------------------- level of detail

// How many CHAINS are worth simulating on a tree `distance_m` away, given that a deflection smaller
// than one minute of arc is not resolvable at all (render/lod/perceptual.hpp). Chains are ordered
// root-first and the trunk carries the largest motion, so truncating the list keeps the sway that
// matters and drops the sway that cannot be seen.
//
// Returns `total` when everything is worth stepping, and 0 when the whole tree's sway is invisible.
[[nodiscard]] std::size_t sway_joint_budget(std::size_t total, float distance_m, float treeHeight_m,
                                            float marArcMin = 1.0f) noexcept;

// Step only the first `chainCount` chains; the rest hold their last pose. Same integrator, same
// determinism, and identical results to `step_sway` when `chainCount == state.size()`.
void step_sway_partial(SwayState& state, const wind::WindParams& wind, float windTime, float dt,
                       std::size_t chainCount) noexcept;

} // namespace world::generation
