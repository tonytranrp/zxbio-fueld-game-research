#pragma once

#include <algorithm>
#include <cmath>

#include "world/collision/solid_query.hpp"

namespace world::collision {

struct SweepParams {
    // Horizontal motion blocked at ground level may climb a ledge up to this high (0 = never; fly
    // mode). 0.55 m clears a one-voxel terrace of the 0.5 m mesh world and the 8-voxel steps a
    // steep 7.8 mm slope makes.
    float step_height = 0.55f;
    // Bisection refinement of a blocked axis: 12 halvings put the body within 1/4096 of the wanted
    // motion from the obstacle -- under a tenth of a millimeter for any sane per-frame step.
    int bisection_steps = 12;
    // The whole motion is applied in sub-steps no longer than this per axis. **0 means "derive it
    // from the body"** -- `substep_for()` below -- and that is the default, because a constant here
    // is only ever safe by accident. The old value was 0.25 m against a 0.3 m half-width: correct,
    // but for no stated reason, and silently wrong the moment anyone shrinks the body.
    float max_substep = 0.0f;
    // Contact tolerance for the "started inside" case below. A body resting exactly on a surface
    // reads as marginally INSIDE it, because the caller stores the camera eye and rebuilds the feet
    // as (eye - eye_height) every tick -- a float round trip worth ~1e-7, and a voxel world puts
    // surfaces at exact coordinates constantly. 1 mm is far under the 7.8 mm finest voxel and far
    // over any rounding.
    float skin = 0.001f;
};

// The sub-step rule, stated rather than assumed (goal 229).
//
// The sweep tests END POSITIONS only, so it sees an obstacle exactly when one of the tested boxes
// overlaps it. Place the boxes a distance `d` apart along the path: if `d` is less than the body's
// extent along that axis, consecutive boxes INTERSECT, so their union is a solid connected tube
// with no gap anywhere -- and anything inside that tube is inside at least one tested box. Tunnelling
// is then impossible for an obstacle of ANY thickness, down to a single 7.8 mm voxel. If `d` exceeds
// the extent the union develops gaps of width `d - extent`, and an obstacle thinner than that fits
// through one.
//
// So the rule is `d < extent`, per axis. Taking the SMALLEST extent covers every direction of travel
// at once, and halving it leaves margin for the step-up path (which moves the box up, sideways and
// down again, so the tested boxes are not colinear) and for float. For the 0.6 x 1.75 x 0.6 m body
// that is 0.30 m -- fewer sub-steps than the 0.25 m constant it replaces, and now for a reason.
//
// The sub-step COUNT is unbounded (`ceil(distance / d)`), so the guarantee does not weaken with
// speed: speed buys sub-steps, not risk. There is no "safe up to N m/s" -- there is a cost. At the
// 40 m/s fly speed a 1/60 s frame moves 0.67 m: 3 sub-steps. At the 160 m/s boost, 2.67 m: 9.
[[nodiscard]] inline float substep_for(const Aabb& body) noexcept {
    const glm::vec3 e = body.extent();
    return 0.5f * std::min({e.x, e.y, e.z});
}

struct SweepResult {
    glm::vec3 delta{0.0f}; // the motion actually applied (add it to the body)
    bool blocked_x = false;
    bool blocked_y = false;
    bool blocked_z = false;
    bool grounded = false;       // a downward move was stopped by something under the body
    bool stepped_up = false;     // horizontal motion succeeded by climbing a ledge
    bool started_inside = false; // the body began overlapping solid; nothing was blocked
};

namespace detail {

// Largest fraction f in [0, 1] of `wanted` along `axis` the box can move without overlapping,
// found by bisection after a full-move test. Returns the applied delta along the axis and whether
// the full move was refused.
template <SolidQuery Q>
float sweep_axis(const Q& query, const Aabb& box, int axis, float wanted, int bisectionSteps, bool& blocked) {
    blocked = false;
    if (wanted == 0.0f) {
        return 0.0f;
    }
    glm::vec3 d{0.0f};
    d[axis] = wanted;
    if (!query.overlaps_solid(box.translated(d))) {
        return wanted;
    }
    blocked = true;
    float lo = 0.0f; // known free
    float hi = 1.0f; // known blocked
    for (int i = 0; i < bisectionSteps; ++i) {
        const float mid = 0.5f * (lo + hi);
        d[axis] = wanted * mid;
        if (query.overlaps_solid(box.translated(d))) {
            hi = mid;
        } else {
            lo = mid;
        }
    }
    return wanted * lo;
}

} // namespace detail

// Axis-separated "move and slide" (the Minecraft/Quake family, docs/goals.md Group AA): the
// vertical axis first (gravity settles you onto the floor before you try to walk into it), then
// x, then z, each clipped by bisection against the query. Sliding along a wall is what you get for
// free: the blocked axis loses its motion, the others keep theirs. A body that already overlaps
// solid at the start moves unblocked -- never trap the player, let them walk out (the classic
// policy; also what keeps `--autofly`'s teleport-through-mountains smoke test meaningful).
namespace detail {

template <SolidQuery Q>
SweepResult move_and_slide_once(const Q& query, const Aabb& body, const glm::vec3& wanted,
                                const SweepParams& params);

} // namespace detail

template <SolidQuery Q>
SweepResult move_and_slide(const Q& query, const Aabb& body, const glm::vec3& wanted,
                           const SweepParams& params) {
    Aabb start = body;
    glm::vec3 lift{0.0f};
    if (query.overlaps_solid(start)) {
        // Depenetrate by the skin before giving up. Without this, a body standing exactly on a
        // voxel top takes the escape hatch below on the very tick it is resting -- which returns
        // the WHOLE wanted motion unclipped, so gravity walks it straight through the floor it was
        // standing on. Found by the step-up test in world/player: the body climbed a 4 cm lip and
        // then sank off it one tick later, with a single overlap query to show for the frame.
        const Aabb lifted = start.translated(glm::vec3{0.0f, params.skin, 0.0f});
        if (params.skin > 0.0f && !query.overlaps_solid(lifted)) {
            lift.y = params.skin;
            start = lifted;
        } else {
            // Genuinely embedded. The old policy here was "move unblocked until free", and it does
            // keep the player from being trapped -- but it never actually frees them, because
            // moving unblocked through solid ends every tick still inside. Measured: walk_hillside
            // logged 761 inside-solid ticks and ALL 761 were this branch. One bad tick became a
            // permanent state.
            //
            // So: climb out first. Search upward for the smallest lift that frees the box, in
            // exponentially growing steps to the body's own height. Upward is the right direction
            // for a body standing on terrain -- it is where the free space provably is, since the
            // body walked in from somewhere above the surface -- and bounding it by the body height
            // means a body genuinely buried deep still falls through to the unblocked move rather
            // than teleporting to the sky.
            float free = 0.0f;
            const float maxLift = body.extent().y;
            float probe = params.skin * 8.0f;
            for (; probe < maxLift; probe *= 2.0f) {
                if (!query.overlaps_solid(start.translated(glm::vec3{0.0f, probe, 0.0f}))) {
                    free = probe;
                    break;
                }
            }
            // The doubling ladder's last rung lands somewhere in [maxLift/2, maxLift), so a body
            // buried between the last rung and the cap would be declared unrecoverable by an
            // arithmetic accident. Measured: macro_ground buries the body 1.40 m, the ladder
            // reaches 1.024 m, and the cap is 1.75 m -- recoverable, and missed. Probe the cap.
            if (free == 0.0f && !query.overlaps_solid(start.translated(glm::vec3{0.0f, maxLift, 0.0f}))) {
                free = maxLift;
                probe = maxLift;
            }
            if (free > 0.0f) {
                // Bisect between the last blocked probe and the first free one, so the body is
                // lifted the least that works rather than to a power of two.
                float lo = free * 0.5f; // the last rung that was still blocked
                float hi = free;
                for (int i = 0; i < params.bisection_steps; ++i) {
                    const float mid = 0.5f * (lo + hi);
                    if (query.overlaps_solid(start.translated(glm::vec3{0.0f, mid, 0.0f}))) {
                        lo = mid;
                    } else {
                        hi = mid;
                    }
                }
                lift.y = hi;
                start = start.translated(glm::vec3{0.0f, hi, 0.0f});
            } else {
                // Buried deeper than the body is tall: nothing sensible to climb to. Move unblocked
                // so the player is never trapped, and SAY so -- the caller counts it.
                SweepResult r;
                r.started_inside = true;
                r.delta = wanted;
                return r;
            }
        }
    }
    // Sub-step so no single end-position test can jump an obstacle (see SweepParams::max_substep).
    const float longest = std::max(std::max(std::abs(wanted.x), std::abs(wanted.y)), std::abs(wanted.z));
    const float substep = params.max_substep > 0.0f ? params.max_substep : substep_for(start);
    const int substeps = substep > 0.0f ? std::max(1, static_cast<int>(std::ceil(longest / substep))) : 1;
    SweepResult total;
    total.delta = lift; // the depenetration is part of the motion, so the caller ends up outside
    Aabb box = start;
    const glm::vec3 piece = wanted / static_cast<float>(substeps);
    for (int i = 0; i < substeps; ++i) {
        const SweepResult r = detail::move_and_slide_once(query, box, piece, params);
        box = box.translated(r.delta);
        total.delta += r.delta;
        total.blocked_x = total.blocked_x || r.blocked_x;
        total.blocked_y = total.blocked_y || r.blocked_y;
        total.blocked_z = total.blocked_z || r.blocked_z;
        total.grounded = total.grounded || r.grounded;
        total.stepped_up = total.stepped_up || r.stepped_up;
    }
    // Sub-stepping is a search strategy, not a change to the answer: a motion that was never
    // blocked on an axis must apply EXACTLY the wanted delta on it. Summing `wanted/n` n times does
    // not -- 1.5 m in five 0.3 m pieces sums to 1.499999762, and the test that caught this was
    // passing before only because the old 0.25 m constant happened to divide 1.5 into six pieces
    // that are exact in binary. Snap the unblocked axes; leave the blocked ones as the sweep left
    // them, since there the partial sum IS the answer.
    //
    // The snap is guarded by an epsilon and NOT by `!blocked` alone, because an axis can move for a
    // reason the wanted motion never asked for: the step-up climbs on y with `wanted.y == 0` and
    // `blocked_y == false`, and an unguarded snap silently deleted the 0.4 m climb (caught by
    // "a low ledge is stepped up"). Only a difference small enough to BE rounding is rounding.
    constexpr float kRoundingSlack = 1.0e-4f;
    const glm::vec3 exact = lift + wanted;
    for (int axis = 0; axis < 3; ++axis) {
        const bool blocked = axis == 0 ? total.blocked_x : (axis == 1 ? total.blocked_y : total.blocked_z);
        if (!blocked && std::abs(total.delta[axis] - exact[axis]) < kRoundingSlack) {
            total.delta[axis] = exact[axis];
        }
    }
    return total;
}

namespace detail {

template <SolidQuery Q>
SweepResult move_and_slide_once(const Q& query, const Aabb& body, const glm::vec3& wanted,
                                const SweepParams& params) {
    SweepResult r;
    Aabb box = body;
    glm::vec3 applied{0.0f};

    applied.y = detail::sweep_axis(query, box, 1, wanted.y, params.bisection_steps, r.blocked_y);
    r.grounded = r.blocked_y && wanted.y < 0.0f;
    box = box.translated(glm::vec3{0.0f, applied.y, 0.0f});

    applied.x = detail::sweep_axis(query, box, 0, wanted.x, params.bisection_steps, r.blocked_x);
    box = box.translated(glm::vec3{applied.x, 0.0f, 0.0f});
    applied.z = detail::sweep_axis(query, box, 2, wanted.z, params.bisection_steps, r.blocked_z);
    box = box.translated(glm::vec3{0.0f, 0.0f, applied.z});

    // Step up: horizontal motion was refused and we are not moving upward -- try the same
    // horizontal motion from up to step_height higher, then settle back down onto the ledge.
    if ((r.blocked_x || r.blocked_z) && params.step_height > 0.0f && wanted.y <= 0.0f) {
        Aabb raised = body.translated(glm::vec3{0.0f, applied.y, 0.0f});
        bool upBlocked = false;
        const float up =
            detail::sweep_axis(query, raised, 1, params.step_height, params.bisection_steps, upBlocked);
        raised = raised.translated(glm::vec3{0.0f, up, 0.0f});
        bool bx = false;
        bool bz = false;
        const float sx = detail::sweep_axis(query, raised, 0, wanted.x, params.bisection_steps, bx);
        raised = raised.translated(glm::vec3{sx, 0.0f, 0.0f});
        const float sz = detail::sweep_axis(query, raised, 2, wanted.z, params.bisection_steps, bz);
        raised = raised.translated(glm::vec3{0.0f, 0.0f, sz});
        bool downBlocked = false;
        const float down = detail::sweep_axis(query, raised, 1, -up, params.bisection_steps, downBlocked);
        const float horizontalBefore = applied.x * applied.x + applied.z * applied.z;
        const float horizontalAfter = sx * sx + sz * sz;
        if (horizontalAfter > horizontalBefore + 1.0e-8f) {
            applied.x = sx;
            applied.z = sz;
            applied.y += up + down;
            r.blocked_x = bx;
            r.blocked_z = bz;
            r.stepped_up = true;
            r.grounded = r.grounded || downBlocked;
        }
    }
    r.delta = applied;
    return r;
}

} // namespace detail

} // namespace world::collision
