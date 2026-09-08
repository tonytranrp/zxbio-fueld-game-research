// Prompt 006 Group AM-B: the fluvial core -- research Part 7 §10.2 stage 3, Part 2 §11 stages 2-6.
//
// GOAL 300, THE PARALLELISATION QUESTION, ANSWERED FIRST BECAUSE IT DECIDES THE REST.
// The prompt asks for a determinism strategy before the first solver, and warns that retrofitting
// determinism onto a parallel reduction is far more work than designing for it. **The strategy is
// that every stage in this file is SERIAL, and that is not a concession.**
//
//   * Priority-flood is a priority queue: the order cells are popped in IS the algorithm, and a
//     parallel version needs the basin-graph decomposition (Cordonnier) to be correct at all.
//   * Flow accumulation walks a stack in a fixed order, adding each cell into its receiver. Two
//     threads adding into the same receiver is a reduction whose float result depends on
//     scheduling -- the exact hazard rule 1 names.
//   * The implicit SPIM sweep reads each node's receiver AFTER that receiver has been updated.
//     That dependency is the reason the method is unconditionally stable; breaking it to
//     parallelise would break the stability, not just the determinism.
//
// And the cost says there is nothing to buy: these are O(n) passes over 250,000 cells. Measured
// below in the log; the whole fluvial core runs in milliseconds. `concurrency-and-parallelism.md`
// rule 36 says to check for a `std::execution::par` overload before reaching for a pool -- the
// honest answer here is that neither is warranted, and a serial pass that is bit-identical by
// construction beats a parallel one that needs a test to prove it.
//
// FLOAT SUMMATION ORDER is therefore fixed by construction: every accumulation happens in stack
// order, which is a pure function of the filled elevation field.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <queue>
#include <vector>

#include "world/generation/field/fluvial.hpp"

namespace world::generation::field {
namespace {

// The eight D8 neighbours, and the distance to each in cell units. Ordered so that the four
// cardinals come first -- ties then resolve toward a cardinal, which is the conventional choice
// and, more importantly, a FIXED one.
constexpr std::array<std::int32_t, 8> kDx{1, -1, 0, 0, 1, 1, -1, -1};
constexpr std::array<std::int32_t, 8> kDz{0, 0, 1, -1, 1, -1, 1, -1};
const std::array<float, 8> kDist{1.0f, 1.0f, 1.0f, 1.0f, 1.41421356f, 1.41421356f, 1.41421356f, 1.41421356f};

} // namespace

void priority_flood(TerrainField& out, float epsilon) {
    // Barnes, Lehman & Mulla (2014), the +epsilon variant. Flood inward from the tile edge with a
    // priority queue, raising each pit cell to its outlet level plus a nudge, so that when the pass
    // ends EVERY cell has a strictly downhill path to the boundary -- which is what makes a flow
    // direction defined everywhere, including on flats.
    //
    // The epsilon is what resolves flats. Without it a filled depression is exactly level and D8
    // has no steepest neighbour to pick, so the network breaks up into disconnected pieces inside
    // every lake bed. With it, the fill carries an infinitesimal gradient in the order the flood
    // reached the cells, which is exactly the drainage direction a real lake outlet has.
    const std::int32_t n = out.cells();
    std::span<float> h = out.plane(Plane::Elevation);
    std::vector<char> closed(static_cast<std::size_t>(n) * static_cast<std::size_t>(n), 0);

    struct Node {
        float height;
        std::int32_t index;
        // Deliberately compares on height ALONE with index as the tie-break: a comparator that
        // ties is a comparator whose result depends on the queue's internal ordering, and this
        // whole file exists to be bit-reproducible.
        [[nodiscard]] bool operator<(const Node& other) const noexcept {
            return height != other.height ? height > other.height : index > other.index;
        }
    };
    std::priority_queue<Node> open;

    const auto push = [&](std::int32_t cx, std::int32_t cz) {
        const std::size_t i = out.index(cx, cz);
        if (closed[i] != 0) {
            return;
        }
        closed[i] = 1;
        open.push(Node{h[i], static_cast<std::int32_t>(i)});
    };

    for (std::int32_t c = 0; c < n; ++c) {
        push(c, 0);
        push(c, n - 1);
        push(0, c);
        push(n - 1, c);
    }
    while (!open.empty()) {
        const Node node = open.top();
        open.pop();
        const std::int32_t cx = node.index % n;
        const std::int32_t cz = node.index / n;
        for (int d = 0; d < 8; ++d) {
            const std::int32_t nx = cx + kDx[static_cast<std::size_t>(d)];
            const std::int32_t nz = cz + kDz[static_cast<std::size_t>(d)];
            if (!out.in_bounds(nx, nz)) {
                continue;
            }
            const std::size_t ni = out.index(nx, nz);
            if (closed[ni] != 0) {
                continue;
            }
            closed[ni] = 1;
            h[ni] = std::max(h[ni], node.height + epsilon);
            open.push(Node{h[ni], static_cast<std::int32_t>(ni)});
        }
    }
}

FlowNetwork build_flow_network(const TerrainField& field) {
    // D8, and the reason is recorded rather than defaulted. Tarboton's D-infinity partitions flow
    // between the two steepest downslope neighbours and gives visibly smoother accumulation on
    // hillslopes -- but the 2025 re-evaluation the research cites (§35) measures D-infinity
    // carrying its OWN ~25% cardinal/ordinal bias, so it is not the bias-free option it is usually
    // presented as. D8 is chosen for three reasons that hold here:
    //
    //   1. Every downstream consumer wants a SINGLE receiver: the implicit SPIM sweep solves
    //      h_i against one h_receiver, and a partitioned receiver turns that O(n) solve into a
    //      linear system.
    //   2. The artefact D8 is criticised for -- diagonal striping in accumulation on smooth
    //      hillslopes -- is suppressed here by the hillslope-diffusion stage and by the channel
    //      threshold, which throws away exactly the low-accumulation cells where it shows.
    //   3. It is exactly reproducible with an integer tie-break, and this field is a determinism
    //      contract.
    const std::int32_t n = field.cells();
    const std::span<const float> h = field.plane(Plane::Elevation);
    const std::size_t count = field.cell_count();

    FlowNetwork net;
    net.receiver.assign(count, 0);
    net.order.resize(count);

    for (std::int32_t cz = 0; cz < n; ++cz) {
        for (std::int32_t cx = 0; cx < n; ++cx) {
            const std::size_t i = field.index(cx, cz);
            std::size_t best = i; // its own receiver = an outlet
            float bestSlope = 0.0f;
            for (int d = 0; d < 8; ++d) {
                const std::int32_t nx = cx + kDx[static_cast<std::size_t>(d)];
                const std::int32_t nz = cz + kDz[static_cast<std::size_t>(d)];
                if (!field.in_bounds(nx, nz)) {
                    continue;
                }
                const std::size_t ni = field.index(nx, nz);
                const float slope = (h[i] - h[ni]) / kDist[static_cast<std::size_t>(d)];
                if (slope > bestSlope) {
                    bestSlope = slope;
                    best = ni;
                }
            }
            net.receiver[i] = static_cast<std::uint32_t>(best);
        }
    }

    // The traversal order: cells sorted by DECREASING filled elevation. After priority-flood every
    // receiver is strictly lower than its donor, so this order guarantees a cell is visited before
    // its receiver -- which is what makes accumulation one pass, and (reversed) makes the implicit
    // SPIM sweep one pass. The index tie-break keeps it a total order on equal heights.
    for (std::size_t i = 0; i < count; ++i) {
        net.order[i] = static_cast<std::uint32_t>(i);
    }
    std::sort(net.order.begin(), net.order.end(),
              [&](std::uint32_t a, std::uint32_t b) { return h[a] != h[b] ? h[a] > h[b] : a < b; });
    return net;
}

void accumulate_flow(TerrainField& out, const FlowNetwork& net) {
    std::span<float> acc = out.plane(Plane::FlowAccum);
    // Each cell contributes itself, then hands its total downstream. Walking `order` (decreasing
    // elevation) means a cell's own total is complete before it is added to its receiver.
    std::fill(acc.begin(), acc.end(), 1.0f);
    for (const std::uint32_t i : net.order) {
        const std::uint32_t r = net.receiver[i];
        if (r != i) {
            acc[r] += acc[i];
        }
    }
}

void incise_stream_power(TerrainField& out, const FlowNetwork& net, const FluvialParams& p) {
    // Braun & Willett (2013), the implicit O(n) solver, in its n = 1 form where the update is
    // linear and closed-form:
    //
    //     h_i' = (h_i + dt*U + C * h_r') / (1 + C),    C = K * dt * A^m / dx
    //
    // Solved in order of INCREASING elevation -- the reverse of the accumulation order -- so that
    // h_r' on the right-hand side is already the updated value. That is what makes it implicit and
    // therefore unconditionally stable at any dt, which is the whole point of the method: an
    // explicit version needs dt small enough for the steepest channel in the domain and would need
    // thousands of steps where this needs a handful.
    const std::span<float> h = out.plane(Plane::Elevation);
    const std::span<const float> acc = out.plane(Plane::FlowAccum);
    const float dx = out.geometry().cell_size;
    const float cellArea = out.geometry().cell_area();

    for (int step = 0; step < p.steps; ++step) {
        // Uplift first, applied everywhere above sea level -- the term the incision is competing
        // against, and without it the landscape simply flattens to base level and the slope-area
        // relationship never establishes.
        for (float& v : h) {
            if (v > p.sea_level) {
                v += p.uplift * p.dt;
            }
        }
        for (auto it = net.order.rbegin(); it != net.order.rend(); ++it) {
            const std::uint32_t i = *it;
            const std::uint32_t r = net.receiver[i];
            if (r == i || h[i] <= p.sea_level) {
                continue; // an outlet, or already at base level: nothing to incise
            }
            const float area = acc[i] * cellArea;
            const float c = p.k * p.dt * std::pow(area, p.m) / dx;
            const float updated = (h[i] + c * h[r]) / (1.0f + c);
            // Never cut below the receiver: the implicit form cannot overshoot, but the guard
            // makes that a property of the code rather than of the arithmetic.
            h[i] = std::max(updated, h[r]);
        }
    }
}

void diffuse_hillslopes(TerrainField& out, const FluvialParams& p) {
    // Part 2 §4 and §11(5): linear hillslope diffusion, dh/dt = D grad^2 h.
    //
    // The research is blunt that skipping this is "the single most recognizable 'procedural
    // terrain' tell" -- knife-sharp ridges with no characteristic valley wavelength, which is
    // exactly what the shipped terrain shows. It is what puts a FINITE CURVATURE on a ridge and,
    // with the incision term, sets the valley spacing L_c = (D/K)^(1/(2m+2)).
    //
    // Explicit, because the stability bound is not binding: dt < dx^2/(4D) is 16^2/(4*0.01) =
    // 6,400 yr against a dt of 1,000, and an implicit solve would need a linear system for no gain.
    const std::int32_t n = out.cells();
    const std::span<float> h = out.plane(Plane::Elevation);
    const float dx = out.geometry().cell_size;
    const float coefficient = p.diffusivity * p.dt / (dx * dx);
    std::vector<float> next(h.begin(), h.end());
    for (int step = 0; step < p.diffusion_steps; ++step) {
        for (std::int32_t cz = 0; cz < n; ++cz) {
            for (std::int32_t cx = 0; cx < n; ++cx) {
                const std::size_t i = out.index(cx, cz);
                if (h[i] <= p.sea_level) {
                    next[i] = h[i];
                    continue;
                }
                // Five-point Laplacian with a replicated edge, so the boundary neither gains nor
                // loses material -- a zero-gradient edge rather than a zero-height one, which
                // would carve a moat around the field.
                const auto at = [&](std::int32_t x, std::int32_t z) {
                    const std::int32_t qx = x < 0 ? 0 : (x >= n ? n - 1 : x);
                    const std::int32_t qz = z < 0 ? 0 : (z >= n ? n - 1 : z);
                    return h[out.index(qx, qz)];
                };
                const float laplacian =
                    at(cx + 1, cz) + at(cx - 1, cz) + at(cx, cz + 1) + at(cx, cz - 1) - 4.0f * h[i];
                next[i] = h[i] + coefficient * laplacian;
            }
        }
        std::copy(next.begin(), next.end(), h.begin());
    }
}

float characteristic_valley_spacing(const FluvialParams& p) noexcept {
    // Part 2 §11(5): L_c = (D/K)^(1/(2m+2)). The number the measured valley spacing is checked
    // against in goal 306, so it is derived here rather than written down as a constant.
    return std::pow(p.diffusivity / p.k, 1.0f / (2.0f * p.m + 2.0f));
}

} // namespace world::generation::field
