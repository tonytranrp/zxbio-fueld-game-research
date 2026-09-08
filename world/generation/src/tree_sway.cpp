#include "world/generation/tree_sway.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "render/lod/perceptual.hpp"

namespace world::generation {

namespace {

constexpr float kPi = 3.14159265358979323846f;

// Rayleigh's effective mass for a uniform cantilever vibrating in its first mode, using the static
// tip-load deflection shape: 33/140 of the beam's own mass acts at the tip. Textbook; the same
// coefficient every structural-dynamics text prints for this case.
constexpr float kRayleighCantilever = 33.0f / 140.0f;

// Breast height, the height the forestry literature measures trunk diameter at and the height
// §3.1's f0 ~ dbh/H^2 relationship is stated in.
constexpr float kBreastHeight = 1.3f;

// The tip deflection a tree in the default breeze is expected to reach, as a fraction of its height.
// Used ONLY by `sway_joint_budget` to decide what is too small to see; the actual deflection comes
// out of the simulation. A rough scale is all a visibility threshold needs.
constexpr float kTypicalSwayFraction = 0.015f;

[[nodiscard]] float second_moment(float radius) noexcept {
    const float r2 = radius * radius;
    return 0.25f * kPi * r2 * r2; // pi r^4 / 4
}

// Remove the component along the chain's own axis. That component is TORSION, which this model does
// not carry (see the header); what remains is bending in both transverse directions -- including the
// vertical-axis bending that swings a horizontal branch sideways, which is most of what a crown does.
[[nodiscard]] glm::vec3 project_bend(const glm::vec3& v, const glm::vec3& axis) noexcept {
    return v - axis * glm::dot(v, axis);
}

// Sum(m |p - P|^2) = Sum(m|p|^2) - 2 P . Sum(m p) + |P|^2 Sum(m). Three accumulated channels per
// mass kind are therefore enough to get any subtree's inertia about any pivot, which is what turns
// an O(n^2) moment computation into one backward pass.
[[nodiscard]] float inertia_about(const glm::vec3& pivot, float m, const glm::vec3& s, float q) noexcept {
    return std::max(0.0f, q - 2.0f * glm::dot(pivot, s) + glm::dot(pivot, pivot) * m);
}

// Wind pressure with the Vogel exponent: F = q * A with q = drag_pressure * U^(2+V) (§4.2). At V = 0
// this is the rigid quadratic law; at the tuliptree's V = -1 it is almost linear in wind speed.
[[nodiscard]] float wind_pressure(float dragPressure, float vogelExponent, float speed) noexcept {
    if (speed <= 0.0f) {
        return 0.0f;
    }
    return dragPressure * std::pow(speed, 2.0f + vogelExponent);
}

} // namespace

SwayParams sway_params_for(const SpeciesParams& species, const SwayParams& base) noexcept {
    SwayParams p = base;
    // `flutter_response` is the species' petiole-softness knob (§4.4): an aspen is springier, a
    // conifer stiffer. A springier, more mobile foliage reconfigures LESS as a bulk crown -- it
    // flutters instead of streamlining -- so it sits nearer the rigid V = 0 end of §4.2's measured
    // -0.2..-1.2 band, and a stiff-foliaged conifer sits nearer -1.2. Mapped across the measured band
    // and clamped to it, so no species can leave the range anyone actually measured.
    const float t = std::clamp(species.flutter_response, 0.0f, 2.0f) * 0.5f; // 0..1
    p.vogel_exponent = -1.2f + t * (1.2f - 0.2f);
    return p;
}

float cantilever_frequency_hz(float dbh_m, float height_m, const SwayParams& params) noexcept {
    if (dbh_m <= 0.0f || height_m <= 0.0f) {
        return 0.0f;
    }
    const float r = 0.5f * dbh_m;
    const float I = second_moment(r);
    const float area = kPi * r * r;
    const float k = 3.0f * params.youngs_modulus_pa * I / (height_m * height_m * height_m);
    float mass = kRayleighCantilever * params.wood_density * area * height_m;
    if (params.leaves) {
        // Leaves are `leaf_mass_fraction` OF THE EFFECTIVE MASS, so the bare mass is the remainder.
        mass /= std::max(1.0e-6f, 1.0f - params.leaf_mass_fraction);
    }
    return std::sqrt(k / mass) / (2.0f * kPi);
}

float fundamental_frequency_hz(const TreeSkeleton& skeleton, const SwayParams& params) {
    if (skeleton.empty()) {
        return 0.0f;
    }
    const float baseY = skeleton.segments.front().start.y;
    float top = baseY;
    for (const SkeletonSegment& s : skeleton.segments) {
        top = std::max(top, std::max(s.start.y, s.end.y));
    }
    // Diameter at breast height: the segment whose midpoint sits nearest 1.3 m above the base,
    // falling back to the root segment on a tree shorter than that.
    float radius = skeleton.segments.front().radius;
    float bestDelta = std::numeric_limits<float>::max();
    for (const SkeletonSegment& s : skeleton.segments) {
        const float mid = 0.5f * (s.start.y + s.end.y) - baseY;
        const float delta = std::abs(mid - kBreastHeight);
        if (delta < bestDelta) {
            bestDelta = delta;
            radius = s.radius;
        }
    }
    return cantilever_frequency_hz(2.0f * radius, top - baseY, params);
}

SwayState make_sway_state(const TreeSkeleton& skeleton, const SwayParams& params) {
    SwayState state;
    const std::size_t n = skeleton.segments.size();
    if (n == 0) {
        return state;
    }
    state.base = skeleton.segments.front().start;
    state.drag_pressure = params.drag_pressure;
    state.vogel_exponent = params.vogel_exponent;
    state.branch_reaction = params.branch_reaction;

    state.chain_of.assign(n, -1);
    state.weight.assign(n, 0.0f);
    state.pivot.assign(n, glm::vec3(0.0f));
    state.parent.resize(n);

    // --- per-segment primitives, in the tree-local frame -----------------------------------------
    std::vector<float> segLength(n, 0.0f);
    std::vector<float> segStiffness(n, 0.0f); // k_j = E I_j / ds_j, the discretised beam's joint
    std::vector<float> woodMass(n, 0.0f);
    std::vector<float> leafArea(n, 0.0f); // this segment's OWN leaf area, m^2
    std::vector<float> dragArea(n, 0.0f);
    std::vector<glm::vec3> centre(n, glm::vec3(0.0f));

    for (std::size_t i = 0; i < n; ++i) {
        const SkeletonSegment& s = skeleton.segments[i];
        state.parent[i] = s.parent;
        state.pivot[i] = s.start - state.base;
        centre[i] = 0.5f * (s.start + s.end) - state.base;

        const float ds = std::max(1.0e-4f, glm::length(s.end - s.start));
        const float r = std::max(1.0e-4f, s.radius);
        segLength[i] = ds;
        segStiffness[i] = params.youngs_modulus_pa * second_moment(r) / ds;
        woodMass[i] = params.wood_density * kPi * r * r * ds;
        dragArea[i] = 2.0f * r * ds; // projected wood, so a bare tree still catches the wind
    }
    // `leaf_count` is DISTAL leaf area, so a segment's own leaf area is what its children do not
    // already account for. Tips keep all of theirs.
    for (std::size_t i = 0; i < n; ++i) {
        leafArea[i] = skeleton.segments[i].leaf_count;
    }
    for (std::size_t i = n; i-- > 1;) {
        const std::int32_t p = state.parent[i];
        if (p >= 0) {
            leafArea[static_cast<std::size_t>(p)] -= skeleton.segments[i].leaf_count;
        }
    }
    for (std::size_t i = 0; i < n; ++i) {
        leafArea[i] = std::max(0.0f, leafArea[i]);
        if (params.leaves) {
            dragArea[i] += leafArea[i];
        }
    }

    // --- one backward pass accumulates every subtree moment ---------------------------------------
    std::vector<float> mW(n, 0.0f), qW(n, 0.0f), mL(n, 0.0f), qL(n, 0.0f), aD(n, 0.0f);
    std::vector<glm::vec3> sW(n, glm::vec3(0.0f)), sL(n, glm::vec3(0.0f)), sD(n, glm::vec3(0.0f));
    for (std::size_t i = 0; i < n; ++i) {
        const glm::vec3 c = centre[i];
        const float c2 = glm::dot(c, c);
        mW[i] = woodMass[i];
        sW[i] = woodMass[i] * c;
        qW[i] = woodMass[i] * c2;
        mL[i] = leafArea[i];
        sL[i] = leafArea[i] * c;
        qL[i] = leafArea[i] * c2;
        aD[i] = dragArea[i];
        sD[i] = dragArea[i] * c;
    }
    for (std::size_t i = n; i-- > 1;) {
        const std::int32_t pi = state.parent[i];
        if (pi < 0) {
            continue;
        }
        const auto p = static_cast<std::size_t>(pi);
        mW[p] += mW[i];
        sW[p] += sW[i];
        qW[p] += qW[i];
        mL[p] += mL[i];
        sL[p] += sL[i];
        qL[p] += qL[i];
        aD[p] += aD[i];
        sD[p] += sD[i];
    }

    // --- leaf mass per unit leaf area, derived rather than authored --------------------------------
    // §3.4's 18-19% bare-vs-leafy shift was measured on the TRUNK's fundamental, so the constraint is
    // on the whole tree: leaf inertia about the root must be `leaf_mass_fraction` of the total.
    // Solving that for the surface density makes the shift come out at the measured value by
    // construction, and puts the leaf mass where the leaves actually are -- distal, on high lever
    // arms, which is where it depresses a branch's own frequency more than the trunk's.
    float leafMassPerArea = 0.0f;
    if (params.leaves) {
        const glm::vec3 rootPivot = state.pivot[0];
        const float jWood = inertia_about(rootPivot, mW[0], sW[0], qW[0]);
        const float jLeafPerSigma = inertia_about(rootPivot, mL[0], sL[0], qL[0]);
        if (jLeafPerSigma > 1.0e-9f) {
            const float lambda = std::clamp(params.leaf_mass_fraction, 0.0f, 0.95f);
            leafMassPerArea = (lambda / (1.0f - lambda)) * jWood / jLeafPerSigma;
        }
    }

    // --- chain decomposition: follow the thickest child ---------------------------------------------
    // The thickest child continues the axis, so the TRUNK chain runs the whole height of the tree
    // (which is what "the trunk's fundamental" means) and every side branch becomes its own cantilever
    // hanging off whatever carries it. A chain start is the root, or any segment that is not its
    // parent's thickest child. Iterating segments in order gives chain indices with parent < self,
    // because a chain start's parent segment is always earlier and so its chain already exists.
    std::vector<std::int32_t> thickestChild(n, -1);
    for (std::size_t i = 1; i < n; ++i) {
        const std::int32_t pi = state.parent[i];
        if (pi < 0) {
            continue;
        }
        const auto p = static_cast<std::size_t>(pi);
        const std::int32_t best = thickestChild[p];
        if (best < 0 ||
            skeleton.segments[i].radius > skeleton.segments[static_cast<std::size_t>(best)].radius) {
            thickestChild[p] = static_cast<std::int32_t>(i);
        }
    }

    std::vector<std::vector<std::int32_t>> chainSegments;
    for (std::size_t i = 0; i < n; ++i) {
        const std::int32_t pi = state.parent[i];
        const bool isStart =
            pi < 0 || thickestChild[static_cast<std::size_t>(pi)] != static_cast<std::int32_t>(i);
        if (!isStart) {
            continue;
        }
        const auto chain = static_cast<std::int32_t>(chainSegments.size());
        chainSegments.emplace_back();
        for (std::int32_t s = static_cast<std::int32_t>(i); s >= 0;
             s = thickestChild[static_cast<std::size_t>(s)]) {
            state.chain_of[static_cast<std::size_t>(s)] = chain;
            chainSegments.back().push_back(s);
        }
        state.chain_parent.push_back(pi < 0 ? -1 : state.chain_of[static_cast<std::size_t>(pi)]);
    }

    const std::size_t chains = chainSegments.size();
    state.angle.assign(chains, glm::vec3(0.0f));
    state.velocity.assign(chains, glm::vec3(0.0f));
    state.accel.assign(chains, glm::vec3(0.0f));
    state.reaction.assign(chains, glm::vec3(0.0f));
    state.abs_accel.assign(chains, glm::vec3(0.0f));
    state.omega.assign(chains, 0.0f);
    state.damping.assign(chains, 0.0f);
    state.stiffness.assign(chains, 0.0f);
    state.inertia.assign(chains, 0.0f);
    state.drag_area.assign(chains, 0.0f);
    state.axis.assign(chains, glm::vec3(0.0f, 1.0f, 0.0f));
    state.mean_pivot.assign(chains, glm::vec3(0.0f));
    state.drag_centroid.assign(chains, glm::vec3(0.0f));
    state.quasi_static.assign(chains, 0u);

    const float zeta = params.leaves ? params.damping_leaf_on : params.damping_leaf_off;
    const float omegaMax = 2.0f * kPi * params.rigid_above_hz;
    const float omegaMin = 2.0f * kPi * params.min_hz;

    for (std::size_t c = 0; c < chains; ++c) {
        const std::vector<std::int32_t>& segs = chainSegments[c];

        // Arclength along the chain to each segment's START, and the chain's total length.
        float total = 0.0f;
        for (const std::int32_t s : segs) {
            total += segLength[static_cast<std::size_t>(s)];
        }
        total = std::max(total, 1.0e-4f);

        // w_j proportional to (L - s_j) * ds_j -- the static curvature under a tip load, which is the
        // deflection shape a swaying cantilever actually takes, and (see the header) the weighting
        // that makes K/lever^2 come out at exactly 3EI/L^3.
        float arc = 0.0f;
        float weightSum = 0.0f;
        for (const std::int32_t s : segs) {
            const auto j = static_cast<std::size_t>(s);
            const float w = (total - arc) * segLength[j];
            state.weight[j] = w;
            weightSum += w;
            arc += segLength[j];
        }
        weightSum = std::max(weightSum, 1.0e-12f);

        glm::vec3 meanPivot(0.0f);
        float stiffness = 0.0f;
        for (const std::int32_t s : segs) {
            const auto j = static_cast<std::size_t>(s);
            state.weight[j] /= weightSum;
            meanPivot += state.weight[j] * state.pivot[j];
            stiffness += segStiffness[j] * state.weight[j] * state.weight[j];
        }
        state.mean_pivot[c] = meanPivot;
        state.stiffness[c] = stiffness;

        const glm::vec3 span = skeleton.segments[static_cast<std::size_t>(segs.back())].end -
                               skeleton.segments[static_cast<std::size_t>(segs.front())].start;
        const float spanLen = glm::length(span);
        state.axis[c] = spanLen > 1.0e-6f ? span / spanLen : glm::vec3(0.0f, 1.0f, 0.0f);

        // Everything downstream of the chain's base is the subtree of its first segment -- already
        // accumulated above, so the inertia and the drag centroid are one lookup each.
        const auto root = static_cast<std::size_t>(segs.front());
        const float j = inertia_about(meanPivot, mW[root], sW[root], qW[root]) +
                        leafMassPerArea * inertia_about(meanPivot, mL[root], sL[root], qL[root]);
        state.inertia[c] = std::max(j, 1.0e-9f);
        state.drag_area[c] = aD[root];
        state.drag_centroid[c] = aD[root] > 1.0e-9f ? sD[root] / aD[root] : centre[root];

        float omega = std::sqrt(std::max(0.0f, stiffness / state.inertia[c]));
        if (omega > omegaMax) {
            state.quasi_static[c] = 1u;
            omega = omegaMax;
        }
        omega = std::max(omega, omegaMin);
        state.omega[c] = omega;
        state.damping[c] = 2.0f * zeta * omega;
    }
    return state;
}

namespace {

void integrate(SwayState& state, const glm::vec3& force, float dt, std::size_t count,
               bool branchReaction) noexcept {
    const std::size_t n = std::min(count, state.size());

    for (std::size_t c = 0; c < n; ++c) {
        const std::int32_t pc = state.chain_parent[c];
        const glm::vec3 baseAlpha = pc < 0 ? glm::vec3(0.0f) : state.abs_accel[static_cast<std::size_t>(pc)];

        // Generalized force: with the chain's coordinate applied through the weights, a downstream
        // point p moves by `phi x (p - Pbar)`, so the work-conjugate force is simply the torque of
        // the downstream drag about Pbar. Flattened because this model bends, it does not twist.
        const glm::vec3 lever = state.drag_centroid[c] - state.mean_pivot[c];
        const glm::vec3 torque =
            project_bend(glm::cross(lever, force * state.drag_area[c]), state.axis[c]);
        const glm::vec3 staticAngle =
            state.stiffness[c] > 0.0f ? torque / state.stiffness[c] : glm::vec3(0.0f);

        if (state.quasi_static[c] != 0u) {
            state.angle[c] = staticAngle;
            state.velocity[c] = glm::vec3(0.0f);
            state.accel[c] = glm::vec3(0.0f);
            state.abs_accel[c] = baseAlpha;
            continue;
        }

        const float w2 = state.omega[c] * state.omega[c];
        glm::vec3 a = w2 * (staticAngle - state.angle[c]) - state.damping[c] * state.velocity[c] - baseAlpha;
        if (branchReaction) {
            a += state.reaction[c];
        }
        a = project_bend(a, state.axis[c]);
        state.accel[c] = a;
        // Semi-implicit (symplectic) Euler: velocity first, then position from the NEW velocity.
        // Stable for omega*dt < 2, which `rigid_above_hz` guarantees, and it does not pump energy
        // into a lightly damped oscillator the way explicit Euler does.
        state.velocity[c] += a * dt;
        state.angle[c] += state.velocity[c] * dt;
        state.abs_accel[c] = baseAlpha + a;
    }

    // The child's reaction on its parent, for the NEXT step. Zeroed and rebuilt here so a partial
    // step cannot leave a stale torque behind on a chain it did not touch.
    std::fill(state.reaction.begin(), state.reaction.end(), glm::vec3(0.0f));
    if (!branchReaction) {
        return;
    }
    for (std::size_t c = n; c-- > 1;) {
        const std::int32_t pc = state.chain_parent[c];
        if (pc < 0) {
            continue;
        }
        const auto p = static_cast<std::size_t>(pc);
        state.reaction[p] -= (state.inertia[c] / state.inertia[p]) * state.accel[c];
    }
}

} // namespace

void step_sway(SwayState& state, const wind::WindParams& windParams, float windTime, float dt) noexcept {
    step_sway_partial(state, windParams, windTime, dt, state.size());
}

void step_sway_partial(SwayState& state, const wind::WindParams& windParams, float windTime, float dt,
                       std::size_t chainCount) noexcept {
    if (state.empty() || chainCount == 0 || dt <= 0.0f) {
        return;
    }
    // ONE wind sample per tree. A tree is far smaller than a gust (gust crests are ~125 m apart), so
    // sampling per chain would buy a sub-millimetre difference for many times the cost -- and it
    // would make the motion depend on how many chains happened to be simulated this frame, which is
    // exactly the kind of LOD-visible seam `sway_joint_budget` exists to avoid.
    const wind::WindSample sample = wind::sample_wind(windParams, state.base, windTime);
    const glm::vec3 force =
        sample.direction * wind_pressure(state.drag_pressure, state.vogel_exponent, sample.speed);
    integrate(state, force, dt, chainCount, state.branch_reaction);
}

void pose_skeleton(const SwayState& state, const TreeSkeleton& rest, std::vector<glm::vec3>& outStart,
                   std::vector<glm::vec3>& outEnd) {
    const std::size_t n = rest.segments.size();
    outStart.resize(n);
    outEnd.resize(n);
    if (n == 0 || state.segment_count() != n) {
        for (std::size_t i = 0; i < n; ++i) {
            outStart[i] = rest.segments[i].start;
            outEnd[i] = rest.segments[i].end;
        }
        return;
    }

    // offset(p) = A x p - B, with A the summed rotation vectors of p's ancestors and B the summed
    // (w x P). Both accumulate in ONE forward pass because a parent is always earlier in the list.
    std::vector<glm::vec3> accumA(n, glm::vec3(0.0f));
    std::vector<glm::vec3> accumB(n, glm::vec3(0.0f));

    for (std::size_t i = 0; i < n; ++i) {
        const std::int32_t pi = state.parent[i];
        const glm::vec3 aPar = pi < 0 ? glm::vec3(0.0f) : accumA[static_cast<std::size_t>(pi)];
        const glm::vec3 bPar = pi < 0 ? glm::vec3(0.0f) : accumB[static_cast<std::size_t>(pi)];

        const glm::vec3 localStart = rest.segments[i].start - state.base;
        const glm::vec3 localEnd = rest.segments[i].end - state.base;

        outStart[i] = rest.segments[i].start + glm::cross(aPar, localStart) - bPar;

        // This segment's own share of its chain's rotation.
        const glm::vec3 own = state.weight[i] * state.angle[static_cast<std::size_t>(state.chain_of[i])];
        const glm::vec3 aSelf = aPar + own;
        const glm::vec3 bSelf = bPar + glm::cross(own, state.pivot[i]);
        accumA[i] = aSelf;
        accumB[i] = bSelf;

        outEnd[i] = rest.segments[i].end + glm::cross(aSelf, localEnd) - bSelf;
    }
}

glm::vec3 tip_displacement(const SwayState& state, const TreeSkeleton& rest) noexcept {
    const std::size_t n = rest.segments.size();
    if (n == 0 || state.segment_count() != n) {
        return glm::vec3(0.0f);
    }
    std::size_t top = 0;
    float bestY = rest.segments[0].end.y;
    for (std::size_t i = 1; i < n; ++i) {
        if (rest.segments[i].end.y > bestY) {
            bestY = rest.segments[i].end.y;
            top = i;
        }
    }
    // Walk that one segment's ancestor chain rather than posing the tree: O(depth), and it allocates
    // nothing, which is what lets this stay noexcept and be called from a measurement loop.
    const glm::vec3 localEnd = rest.segments[top].end - state.base;
    glm::vec3 offset(0.0f);
    for (std::int32_t i = static_cast<std::int32_t>(top); i >= 0;
         i = state.parent[static_cast<std::size_t>(i)]) {
        const auto idx = static_cast<std::size_t>(i);
        const glm::vec3 own = state.weight[idx] * state.angle[static_cast<std::size_t>(state.chain_of[idx])];
        offset += glm::cross(own, localEnd - state.pivot[idx]);
    }
    return offset;
}

std::size_t sway_joint_budget(std::size_t total, float distance_m, float treeHeight_m,
                              float marArcMin) noexcept {
    if (total == 0 || treeHeight_m <= 0.0f) {
        return 0;
    }
    if (distance_m <= 0.0f) {
        return total;
    }
    // The motion a dropped chain would have contributed. For a uniform cantilever a joint at
    // arclength s contributes to tip deflection in proportion to (L - s)^2, so keeping the first
    // m of N leaves a residual of (1 - m/N)^3 of the total sway. Require that residual to fall below
    // one minute of arc at this distance and solve for m.
    const float amplitude = kTypicalSwayFraction * treeHeight_m;
    const auto resolvable = static_cast<float>(
        render::lod::resolvable_distance_m(static_cast<double>(amplitude), static_cast<double>(marArcMin)));
    if (distance_m >= resolvable) {
        return 0; // the whole tree's sway is smaller than the eye resolves
    }
    const float ratio = distance_m / resolvable; // = (smallest visible motion) / amplitude
    const float keep = 1.0f - std::cbrt(ratio);
    const auto m = static_cast<std::size_t>(std::ceil(keep * static_cast<float>(total)));
    return std::min(total, std::max<std::size_t>(m, 1));
}

} // namespace world::generation
