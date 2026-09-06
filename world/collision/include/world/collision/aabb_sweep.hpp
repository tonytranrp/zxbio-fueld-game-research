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
    // The whole motion is applied in sub-steps no longer than this per axis. The sweep tests only
    // end positions, so a sub-step shorter than the body's smallest extent (0.6 m wide) cannot
    // skip over anything: consecutive body boxes overlap along the path. At boost speed a frame
    // moves ~1.1 m -- five sub-steps, a tree trunk is 0.5 m thick.
    float max_substep = 0.25f;
};

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
    if (query.overlaps_solid(body)) {
        SweepResult r;
        r.started_inside = true;
        r.delta = wanted;
        return r;
    }
    // Sub-step so no single end-position test can jump an obstacle (see SweepParams::max_substep).
    const float longest = std::max(std::max(std::abs(wanted.x), std::abs(wanted.y)), std::abs(wanted.z));
    const int substeps = params.max_substep > 0.0f
                             ? std::max(1, static_cast<int>(std::ceil(longest / params.max_substep)))
                             : 1;
    SweepResult total;
    Aabb box = body;
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
