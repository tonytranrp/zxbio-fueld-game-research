// Prompt 007 goal 335 = docs/goals.md goal 190's Check, verbatim: "step response and resonance near
// the predicted f0 ~ 0.26 Hz for sycamore-scale parameters; determinism; <= 0.5 ms/frame for all
// in-ring trees, measured with the attributor."
//
// The frame budget is measured in the app, with the attributor, and reported in
// research/view-distance-and-cover-log.md -- a wall-clock assertion in a ctest run under an unknown
// load is a flake, not a measurement. What lives here is everything that IS a property: the
// frequency, the free-decay damping, determinism, and the two structural claims the model is built
// on (the discretised beam reproduces the continuum cantilever; branches bleed trunk energy).
//
// Prompt 006's lesson applies with full force here -- the instrument was wrong more often than the
// subject. So the analytic prediction and the simulated measurement are computed by INDEPENDENT
// routes and checked against each other: `cantilever_frequency_hz` is a closed form, and the period
// measured below comes from counting zero crossings of an actual numerical integration. Neither is
// evidence on its own; agreeing is.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

#include "world/generation/tree_skeleton.hpp"
#include "world/generation/tree_sway.hpp"

using namespace world::generation;

namespace {

// The reference tree. §3.1's data point is a finite-element-simulated sycamore (Acer
// pseudoplatanus) whose fundamental came out at f0 = 0.26 Hz; §9 gives the wood constants the same
// simulations use. The GEOMETRY is what the formula then pins down, and it has to be a real tree or
// the agreement means nothing: 20 m tall, 25.2 cm dbh -- slenderness H/dbh = 79, squarely inside the
// 60-100 band forest-grown broadleaves occupy. An open-grown park sycamore would be stockier and
// sway faster; a forest one this slender is exactly the subject those studies measure.
constexpr float kSycamoreHeight = 20.0f;
constexpr float kSycamoreDbh = 0.252f;

// A straight vertical chain of `count` segments of radius `radius`, so the discretised beam can be
// compared against the continuum formula with no space-colonisation noise in between.
//
// `leafArea` is the crown it carries, m^2, hung on the topmost segment -- 80 m^2 is LAI 1.6 over the
// ~50 m^2 footprint of a 4 m crown radius, i.e. a real sycamore's canopy. It matters that this is
// non-zero: `cantilever_frequency_hz` includes the leaf mass correction, so comparing a leafy
// prediction against a leafless simulation is a 18.5% apples-to-oranges error, and that is exactly
// what the first version of this test did (it read 12.6% high).
[[nodiscard]] TreeSkeleton uniform_pole(float height, float radius, std::size_t count,
                                        float leafArea = 80.0f) {
    TreeSkeleton s;
    s.segments.reserve(count);
    const float ds = height / static_cast<float>(count);
    for (std::size_t i = 0; i < count; ++i) {
        SkeletonSegment seg;
        seg.parent = i == 0 ? -1 : static_cast<std::int32_t>(i) - 1;
        seg.start = glm::vec3(0.0f, ds * static_cast<float>(i), 0.0f);
        seg.end = glm::vec3(0.0f, ds * static_cast<float>(i + 1), 0.0f);
        seg.radius = radius;
        // `leaf_count` is DISTAL leaf area, so every segment of the pole carries the whole crown's,
        // and only the tip ends up owning any of it -- which is what the crown of a pole is.
        seg.leaf_count = leafArea;
        s.segments.push_back(seg);
    }
    return s;
}

// Release from a displaced rest and watch it ring: the period from zero crossings, and the damping
// ratio from the logarithmic decrement between successive peaks. Both are textbook single-DOF
// identification methods, applied to the SIMULATION's output rather than to its parameters.
struct FreeDecay {
    float frequency_hz = 0.0f;
    float damping_ratio = 0.0f;
    float first_peak = 0.0f;
    float last_peak = 0.0f;
    int peaks = 0;
};

[[nodiscard]] FreeDecay measure_free_decay(SwayState& state, const TreeSkeleton& skeleton, float dt,
                                           float seconds) {
    // Displace every joint by a uniform curvature and let go. A pure step release: no wind at all,
    // so what is measured is the structure and nothing else.
    for (std::size_t i = 0; i < state.size(); ++i) {
        state.angle[i] = glm::vec3(2.0e-4f, 0.0f, 0.0f);
        state.velocity[i] = glm::vec3(0.0f);
        state.accel[i] = glm::vec3(0.0f);
        state.reaction[i] = glm::vec3(0.0f);
        state.abs_accel[i] = glm::vec3(0.0f);
    }
    const world::wind::WindParams still = world::wind::still_wind();

    const auto steps = static_cast<int>(seconds / dt);
    std::vector<float> trace;
    trace.reserve(static_cast<std::size_t>(steps));
    for (int i = 0; i < steps; ++i) {
        step_sway(state, still, 0.0f, dt);
        trace.push_back(tip_displacement(state, skeleton).z);
    }

    FreeDecay out;
    // Zero crossings give the period without needing an FFT, and without an FFT's leakage.
    int crossings = 0;
    float firstCrossing = -1.0f;
    float lastCrossing = -1.0f;
    for (std::size_t i = 1; i < trace.size(); ++i) {
        if ((trace[i - 1] <= 0.0f) != (trace[i] <= 0.0f)) {
            const float t = dt * static_cast<float>(i);
            if (crossings == 0) {
                firstCrossing = t;
            }
            lastCrossing = t;
            ++crossings;
        }
    }
    if (crossings >= 3) {
        // (crossings - 1) half-periods between the first and the last.
        const float halfPeriod = (lastCrossing - firstCrossing) / static_cast<float>(crossings - 1);
        out.frequency_hz = 1.0f / (2.0f * halfPeriod);
    }

    // Peak magnitudes, for the logarithmic decrement.
    std::vector<float> peaks;
    for (std::size_t i = 1; i + 1 < trace.size(); ++i) {
        const float a = std::abs(trace[i]);
        if (a > std::abs(trace[i - 1]) && a >= std::abs(trace[i + 1]) && a > 1.0e-9f) {
            peaks.push_back(a);
        }
    }
    out.peaks = static_cast<int>(peaks.size());
    if (peaks.size() >= 2) {
        out.first_peak = peaks.front();
        out.last_peak = peaks.back();
        // delta = (1/n) ln(x0/xn) over n half-cycles; zeta = delta / sqrt(4 pi^2 + delta^2), with
        // the half-cycle count because both signs of the swing register as peaks here.
        const auto n = static_cast<float>(peaks.size() - 1);
        const float delta = 2.0f * std::log(peaks.front() / peaks.back()) / n;
        out.damping_ratio = delta / std::sqrt(4.0f * 3.14159265f * 3.14159265f + delta * delta);
    }
    return out;
}

} // namespace

TEST_CASE("the closed form reproduces the sycamore's measured fundamental", "[tree][sway]") {
    SwayParams p;
    const float f0 = cantilever_frequency_hz(kSycamoreDbh, kSycamoreHeight, p);
    INFO("20 m sycamore, " << kSycamoreDbh * 100.0f << " cm dbh -> " << f0 << " Hz");
    CHECK(f0 == Catch::Approx(0.26f).margin(0.01f));

    // And the geometry is a real tree, not one back-solved into absurdity.
    const float slenderness = kSycamoreHeight / kSycamoreDbh;
    INFO("slenderness H/dbh = " << slenderness);
    CHECK(slenderness > 60.0f);
    CHECK(slenderness < 100.0f);

    // The scaling law itself: f0 ~ dbh / H^2 (§3.1). Doubling the height at fixed diameter must
    // quarter the frequency, and doubling the diameter must double it.
    CHECK(cantilever_frequency_hz(kSycamoreDbh, 2.0f * kSycamoreHeight, p) ==
          Catch::Approx(0.25f * f0).epsilon(1e-4));
    CHECK(cantilever_frequency_hz(2.0f * kSycamoreDbh, kSycamoreHeight, p) ==
          Catch::Approx(2.0f * f0).epsilon(1e-4));
}

TEST_CASE("losing its leaves speeds a tree up by the measured 18-19 percent", "[tree][sway]") {
    // §3.4, measured by pull-and-release on the same trees in summer and winter. The constant in
    // `SwayParams` IS this number re-expressed, so this test is what stops it being re-tuned into
    // something else by accident.
    SwayParams leafy;
    SwayParams bare;
    bare.leaves = false;
    const float fLeafy = cantilever_frequency_hz(kSycamoreDbh, kSycamoreHeight, leafy);
    const float fBare = cantilever_frequency_hz(kSycamoreDbh, kSycamoreHeight, bare);
    const float shift = 100.0f * (fBare / fLeafy - 1.0f);
    INFO("leafy " << fLeafy << " Hz, bare " << fBare << " Hz -- " << shift << "% faster bare");
    CHECK(shift >= 18.0f);
    CHECK(shift <= 19.0f);

    // And the mechanism is mass, not damping: §3.4 checked that the aerodynamic contribution to the
    // frequency shift is under 1%. Here the damping ratio is not an input to the frequency at all.
    SwayParams undamped = leafy;
    undamped.damping_leaf_on = 0.0f;
    CHECK(cantilever_frequency_hz(kSycamoreDbh, kSycamoreHeight, undamped) ==
          Catch::Approx(fLeafy).epsilon(1e-6));
}

TEST_CASE("the simulated chain rings at the frequency the closed form predicts", "[tree][sway]") {
    // THE INDEPENDENT CONFIRMATION. The left-hand side is an analytic Rayleigh estimate; the
    // right-hand side is a numerical integration of a 40-segment chain, released from a step and
    // counted. They share no code beyond the segment stiffness, which is the thing being checked.
    SwayParams bare;
    bare.leaves = false;
    bare.branch_reaction = false; // a bare pole has no branches to react; this isolates the beam

    // FIRST, LEAFLESS, where the two models describe the SAME object and the tolerance can be tight.
    // The only difference is the assumed deflection shape: this file's weighting integrates to
    // omega^2 = 12 EI / (rho A L^4) and Rayleigh's to 12.727, so the chain must land 2.9% low.
    {
        const TreeSkeleton pole = uniform_pole(kSycamoreHeight, 0.5f * kSycamoreDbh, 40, 0.0f);
        SwayState state = make_sway_state(pole, bare);
        REQUIRE(state.size() == 1); // a straight pole is ONE chain: nothing branches off it
        REQUIRE(state.segment_count() == 40);

        const float predicted = cantilever_frequency_hz(kSycamoreDbh, kSycamoreHeight, bare);
        const FreeDecay decay = measure_free_decay(state, pole, kSwayTick, 40.0f);
        INFO("bare pole: predicted " << predicted << " Hz, simulated " << decay.frequency_hz << " Hz over "
                                     << decay.peaks << " peaks (ratio " << decay.frequency_hz / predicted
                                     << ")");
        REQUIRE(decay.peaks >= 4);
        CHECK(decay.frequency_hz == Catch::Approx(predicted).epsilon(0.05));
        CHECK(decay.frequency_hz < predicted); // and low, in the direction the algebra says
    }

    // THEN, IN LEAF, which is the sycamore §3.1 measured -- and here the two models describe
    // genuinely different objects, so the tolerance is wider and the reason is written down rather
    // than absorbed.
    //
    // `cantilever_frequency_hz` folds the crown in as a fraction of the EFFECTIVE mass, i.e. weighted
    // like the beam's own distributed mass. The simulation puts it where the leaves actually are --
    // at the top -- and distal mass depresses a frequency more than the same mass spread down the
    // stem. With the crown mass M calibrated against root inertia the two come out at
    //
    //     omega^2_analytic = 9.06 EI / (rho A L^4)      omega^2_chain = 12 EI / (rho A L^4 + 4 M L^3)
    //
    // and for this tree that is a predicted ratio of 0.928. The simulation is the more physical of
    // the two; the closed form is the coarse estimate that the research quotes its 0.26 Hz from.
    {
        const TreeSkeleton pole = uniform_pole(kSycamoreHeight, 0.5f * kSycamoreDbh, 40);
        SwayParams leafy;
        leafy.branch_reaction = false;
        SwayState state = make_sway_state(pole, leafy);

        const float predicted = cantilever_frequency_hz(kSycamoreDbh, kSycamoreHeight, leafy);
        const FreeDecay decay = measure_free_decay(state, pole, kSwayTick, 40.0f);
        const float ratio = decay.frequency_hz / predicted;
        INFO("leafy pole: predicted " << predicted << " Hz, simulated " << decay.frequency_hz
                                      << " Hz over " << decay.peaks << " peaks (ratio " << ratio << ")");
        REQUIRE(decay.peaks >= 4);
        CHECK(ratio == Catch::Approx(0.928f).margin(0.04f));

        // "Near f0 ~ 0.26 Hz for sycamore-scale parameters", stated against the research's own number
        // rather than against our closed form, because that is what the Check asks.
        INFO("simulated fundamental " << decay.frequency_hz << " Hz vs the research's 0.26 Hz");
        CHECK(decay.frequency_hz > 0.22f);
        CHECK(decay.frequency_hz < 0.30f);
    }
}

TEST_CASE("free decay reproduces the damping ratio it was given", "[tree][sway]") {
    // The instrument check: if the log-decrement measurement cannot recover a damping ratio the
    // model was explicitly handed, no damping number measured with it later means anything.
    const TreeSkeleton pole = uniform_pole(kSycamoreHeight, 0.5f * kSycamoreDbh, 40);
    for (const float zeta : {0.039f, 0.086f}) {
        SwayParams p;
        p.branch_reaction = false;
        p.damping_leaf_on = zeta;
        p.damping_leaf_off = zeta;
        SwayState state = make_sway_state(pole, p);
        const FreeDecay decay = measure_free_decay(state, pole, kSwayTick, 60.0f);
        INFO("asked for zeta = " << zeta << ", measured " << decay.damping_ratio << " over " << decay.peaks
                                 << " peaks");
        REQUIRE(decay.peaks >= 4);
        CHECK(decay.damping_ratio == Catch::Approx(zeta).epsilon(0.20));
    }
}

TEST_CASE("a branched tree damps its trunk faster than an equally massive pole", "[tree][sway]") {
    // §3.3's claim, tested rather than asserted: branches with their own natural frequencies act as
    // tuned mass dampers and bleed energy out of the trunk mode. The A/B is the SAME tree with the
    // child-reaction term on and off, so mass, stiffness and the material damping ratio are all
    // identical between the two runs and the only difference is whether a branch is allowed to push
    // back on what carries it.
    TreeSkeleton tree = grow_skeleton(1337, glm::vec3(0.0f), species_params(TreeSpecies::RoundBroadleaf));
    REQUIRE(tree.segments.size() > 50);
    apply_pipe_model(tree);

    SwayParams withReaction;
    SwayParams withoutReaction;
    withoutReaction.branch_reaction = false;

    SwayState a = make_sway_state(tree, withReaction);
    SwayState b = make_sway_state(tree, withoutReaction);
    const FreeDecay withB = measure_free_decay(a, tree, kSwayTick, 60.0f);
    const FreeDecay withoutB = measure_free_decay(b, tree, kSwayTick, 60.0f);

    INFO("branch reaction on:  zeta_eff = " << withB.damping_ratio << " at " << withB.frequency_hz
                                            << " Hz over " << withB.peaks << " peaks");
    INFO("branch reaction off: zeta_eff = " << withoutB.damping_ratio << " at " << withoutB.frequency_hz
                                            << " Hz over " << withoutB.peaks << " peaks");
    REQUIRE(withB.peaks >= 4);
    REQUIRE(withoutB.peaks >= 4);
    // The claim is directional: branching adds damping. The MAGNITUDE is reported in the log, not
    // asserted here, because it is a property of this particular grown tree and not of the model.
    CHECK(withB.damping_ratio > withoutB.damping_ratio);
}

TEST_CASE("the whole crown leans DOWNWIND, and by a plausible amount", "[tree][sway]") {
    // Direction is the one property a picture is genuinely bad at judging -- a skeleton drawn over
    // its own rest pose is busy enough that "which way did it go" is a guess. So it is asserted, on
    // the crown's mean position rather than on one tip, and the MAGNITUDE is asserted too, because a
    // tree that leans the right way by ten metres is not correct either.
    TreeSkeleton tree = grow_skeleton(1337, glm::vec3(0.0f), species_params(TreeSpecies::RoundBroadleaf));
    apply_pipe_model(tree);
    SwayState state = make_sway_state(tree, SwayParams{});

    world::wind::WindParams wind = world::wind::kDefaultWind;
    wind.base_speed = 8.0f;
    wind.base_angle_radians = 0.0f; // straight down +X, so the answer has one component
    const glm::vec3 downwind = world::wind::wind_direction(wind);

    for (int i = 0; i < 2400; ++i) { // 20 s
        step_sway(state, wind, static_cast<float>(i) * kSwayTick, kSwayTick);
    }
    std::vector<glm::vec3> starts;
    std::vector<glm::vec3> ends;
    pose_skeleton(state, tree, starts, ends);

    float along = 0.0f;
    float weight = 0.0f;
    for (std::size_t i = 0; i < tree.segments.size(); ++i) {
        const float w = tree.segments[i].leaf_count + 0.01f;
        along += w * glm::dot(ends[i] - tree.segments[i].end, downwind);
        weight += w;
    }
    along /= weight;

    const world::generation::TreeBounds b = tree.bounds();
    const float height = b.max.y - b.min.y;
    INFO("crown mean moved " << along << " m downwind on a " << height << " m tree ("
                             << 100.0f * along / height << "% of height)");
    CHECK(along > 0.0f);
    // A fresh-to-strong breeze bends a tree by a few percent of its height, not by a tenth of it and
    // not by a millimetre. This is the bound that would catch a drag_pressure recalibration going
    // somewhere absurd.
    CHECK(100.0f * along / height > 0.3f);
    CHECK(100.0f * along / height < 10.0f);
}

TEST_CASE("a horizontal branch bends sideways, not only up", "[tree][sway]") {
    // The bug a PICTURE found, pinned so it cannot come back. The first version projected every
    // rotation vector onto the horizontal plane, reasoning that a trunk is vertical and its torsion
    // axis is therefore Y. For a HORIZONTAL branch broadside to the wind the bending torque is purely
    // vertical, so that projection deleted all of it and half a crown could not move sideways at all.
    //
    // Built as a bare horizontal beam so there is nothing else it could be: wind along +Z, branch
    // along +X. The lateral (Z) deflection must dominate.
    TreeSkeleton branch;
    for (int i = 0; i < 20; ++i) {
        SkeletonSegment seg;
        seg.parent = i == 0 ? -1 : i - 1;
        seg.start = glm::vec3(0.25f * static_cast<float>(i), 5.0f, 0.0f);
        seg.end = glm::vec3(0.25f * static_cast<float>(i + 1), 5.0f, 0.0f);
        seg.radius = 0.03f;
        seg.leaf_count = 2.0f;
        branch.segments.push_back(seg);
    }
    SwayState state = make_sway_state(branch, SwayParams{});
    REQUIRE(state.size() == 1);

    world::wind::WindParams wind = world::wind::still_wind();
    wind.base_speed = 8.0f;
    wind.base_angle_radians = 1.5707963f; // straight down +Z, square onto the branch
    for (int i = 0; i < 600; ++i) {
        step_sway(state, wind, static_cast<float>(i) * kSwayTick, kSwayTick);
    }

    std::vector<glm::vec3> starts;
    std::vector<glm::vec3> ends;
    pose_skeleton(state, branch, starts, ends);
    const glm::vec3 tipOffset = ends.back() - branch.segments.back().end;
    INFO("tip moved (" << tipOffset.x << ", " << tipOffset.y << ", " << tipOffset.z << ") m");
    CHECK(std::abs(tipOffset.z) > 0.01f);
    CHECK(std::abs(tipOffset.z) > 4.0f * std::abs(tipOffset.y));
}

TEST_CASE("the sway is deterministic", "[tree][sway]") {
    // Same skeleton, same wind, same steps -- and, because a frame may step a partial joint list,
    // stepping in two halves of a tick must land where one whole tick lands.
    TreeSkeleton tree =
        grow_skeleton(9001, glm::vec3(11.0f, 3.0f, -7.0f), species_params(TreeSpecies::Aspen));
    apply_pipe_model(tree);
    const SwayParams p = sway_params_for(species_params(TreeSpecies::Aspen));

    SwayState a = make_sway_state(tree, p);
    SwayState b = make_sway_state(tree, p);
    const world::wind::WindParams wind = world::wind::kDefaultWind;

    for (int i = 0; i < 240; ++i) {
        const float t = static_cast<float>(i) * kSwayTick;
        step_sway(a, wind, t, kSwayTick);
        step_sway(b, wind, t, kSwayTick);
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        REQUIRE(a.angle[i].x == b.angle[i].x);
        REQUIRE(a.angle[i].z == b.angle[i].z);
        REQUIRE(a.velocity[i].x == b.velocity[i].x);
    }
    // ...and the tree actually moved, so "identical" is not "identically zero".
    const glm::vec3 tip = tip_displacement(a, tree);
    INFO("tip displacement after 2 s of default wind: " << glm::length(tip) << " m");
    CHECK(glm::length(tip) > 1.0e-4f);
}

TEST_CASE("stepping a truncated joint list is the LOD, and it agrees at full count", "[tree][sway]") {
    TreeSkeleton tree = grow_skeleton(4242, glm::vec3(0.0f), species_params(TreeSpecies::Conifer));
    apply_pipe_model(tree);
    const SwayParams p;

    SwayState full = make_sway_state(tree, p);
    SwayState partial = make_sway_state(tree, p);
    const world::wind::WindParams wind = world::wind::kDefaultWind;
    for (int i = 0; i < 120; ++i) {
        const float t = static_cast<float>(i) * kSwayTick;
        step_sway(full, wind, t, kSwayTick);
        step_sway_partial(partial, wind, t, kSwayTick, partial.size());
    }
    for (std::size_t i = 0; i < full.size(); ++i) {
        REQUIRE(full.angle[i].x == partial.angle[i].x);
    }

    // The budget itself: nearer means more chains, and past the point where the whole sway is under
    // a minute of arc, none at all. A 20 m tree swaying ~1.5% of its height is a 0.3 m motion, which
    // 20/20 vision resolves to ~1 km, so the cutoff is a long way out.
    //
    // Note it does NOT return every chain even at 2 m: the outermost ~12% of a chain list contributes
    // less than a minute of arc of tip motion even from arm's length, which is the point of measuring
    // the budget against a perceptual criterion instead of a distance band.
    const std::size_t n = full.size();
    const std::size_t atArmsLength = sway_joint_budget(n, 2.0f, 20.0f);
    const std::size_t nearby = sway_joint_budget(n, 60.0f, 20.0f);
    const std::size_t distant = sway_joint_budget(n, 400.0f, 20.0f);
    INFO("chains of " << n << " -- at 2 m: " << atArmsLength << ", at 60 m: " << nearby << ", at 400 m: "
                      << distant);
    CHECK(atArmsLength > nearby);
    CHECK(nearby > distant);
    CHECK(distant >= 1);
    CHECK(sway_joint_budget(n, 2000.0f, 20.0f) == 0);
}

TEST_CASE("species differ in how their crowns reconfigure", "[tree][sway]") {
    // §4.2's Vogel exponent, mapped across the measured -0.2..-1.2 band by the species' own
    // petiole-stiffness knob. The assertion is that the mapping stays INSIDE the band and orders the
    // species -- the exact value for a given species is a choice, and is labelled as one.
    const SwayParams aspen = sway_params_for(species_params(TreeSpecies::Aspen));
    const SwayParams conifer = sway_params_for(species_params(TreeSpecies::Conifer));
    INFO("aspen V = " << aspen.vogel_exponent << ", conifer V = " << conifer.vogel_exponent);
    for (const float v : {aspen.vogel_exponent, conifer.vogel_exponent}) {
        CHECK(v <= -0.2f);
        CHECK(v >= -1.2f);
    }
    CHECK(aspen.vogel_exponent > conifer.vogel_exponent);
}

TEST_CASE("stiff twigs are quasi-static rather than unstable", "[tree][sway]") {
    // The model's third decision. A grown tree's outermost joints have natural frequencies in the
    // hundreds of Hz; simulating them explicitly at any affordable timestep would blow up. Assert
    // both halves: some joints ARE marked quasi-static on a real tree, and nothing anywhere in a
    // long run goes non-finite.
    TreeSkeleton tree = grow_skeleton(77, glm::vec3(0.0f), species_params(TreeSpecies::RoundBroadleaf));
    apply_pipe_model(tree);
    SwayState state = make_sway_state(tree, SwayParams{});

    std::size_t rigid = 0;
    for (const std::uint8_t q : state.quasi_static) {
        rigid += q;
    }
    INFO(rigid << " of " << state.size() << " joints are above the 8 Hz cutoff");
    CHECK(rigid > 0);
    CHECK(rigid < state.size()); // ...but not all of them, or nothing would ever sway

    world::wind::WindParams gale = world::wind::kDefaultWind;
    gale.base_speed = 25.0f; // a storm, well outside anything the look was tuned at
    for (int i = 0; i < 3600; ++i) {
        step_sway(state, gale, static_cast<float>(i) * kSwayTick, kSwayTick);
    }
    for (std::size_t i = 0; i < state.size(); ++i) {
        REQUIRE(std::isfinite(state.angle[i].x));
        REQUIRE(std::isfinite(state.angle[i].z));
        REQUIRE(std::isfinite(state.velocity[i].x));
    }
    const glm::vec3 tip = tip_displacement(state, tree);
    INFO("tip displacement in a 25 m/s gale: " << glm::length(tip) << " m");
    CHECK(std::isfinite(glm::length(tip)));
}

TEST_CASE("posing a tree with no deflection returns it unchanged", "[tree][sway]") {
    TreeSkeleton tree = grow_skeleton(5, glm::vec3(3.0f, 1.0f, 2.0f), species_params(TreeSpecies::Shrub));
    apply_pipe_model(tree);
    SwayState state = make_sway_state(tree, SwayParams{});

    std::vector<glm::vec3> starts;
    std::vector<glm::vec3> ends;
    pose_skeleton(state, tree, starts, ends);
    REQUIRE(starts.size() == tree.segments.size());
    for (std::size_t i = 0; i < tree.segments.size(); ++i) {
        REQUIRE(starts[i] == tree.segments[i].start);
        REQUIRE(ends[i] == tree.segments[i].end);
    }

    // And a posed tree stays CONNECTED: a child's start must land on its parent's end, or the
    // hierarchy composition is wrong in a way a picture would show as a tree coming apart.
    const world::wind::WindParams wind = world::wind::kDefaultWind;
    for (int i = 0; i < 300; ++i) {
        step_sway(state, wind, static_cast<float>(i) * kSwayTick, kSwayTick);
    }
    pose_skeleton(state, tree, starts, ends);
    float worst = 0.0f;
    for (std::size_t i = 0; i < tree.segments.size(); ++i) {
        const std::int32_t p = tree.segments[i].parent;
        if (p < 0) {
            continue;
        }
        const auto pi = static_cast<std::size_t>(p);
        // Measured against the gap the REST skeleton already had, so this tests the posing and not
        // the grower's own joint tolerance.
        const float restGap = glm::length(tree.segments[i].start - tree.segments[pi].end);
        worst = std::max(worst, std::abs(glm::length(starts[i] - ends[pi]) - restGap));
    }
    INFO("worst change in the parent-end to child-start gap after posing: " << worst << " m");
    CHECK(worst < 1.0e-4f);
}
