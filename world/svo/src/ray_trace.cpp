#include "world/svo/ray_trace.hpp"

#include <algorithm>
#include <bit>
#include <cmath>

namespace world::svo {

using world::chunk::MaterialID;

namespace {

// Safety bound on traversal iterations -- a well-formed tree never needs more than a few hundred
// per ray; the GPU mirror uses a smaller bound and paints a debug color when it trips.
constexpr std::uint32_t kMaxIterations = 16384;
constexpr float kBigInverse = 1.0e30f;

struct Traversal {
    glm::vec3 o;    // root-normalized origin ([0,1)^3 is the root)
    glm::vec3 d;    // direction in root units (t stays in the caller's units)
    glm::vec3 invd; // 1/d with a huge finite stand-in for zero components
    glm::ivec3 step;
    float cells; // 2^V
    int V;
};

int argmin3(const glm::vec3& v) noexcept {
    if (v.x <= v.y && v.x <= v.z) {
        return 0;
    }
    return v.y <= v.z ? 1 : 2;
}

glm::ivec3 normal_from(int axis, const glm::ivec3& step, const glm::vec3& d) noexcept {
    glm::ivec3 n{0, 0, 0};
    if (axis < 0) {
        // Ray started inside the hit cell: face against the dominant direction component.
        glm::vec3 a = glm::abs(d);
        axis = argmin3(-a); // argmax
        n[axis] = d[axis] > 0.0f ? -1 : 1;
        return n;
    }
    n[axis] = -step[axis];
    return n;
}

// Steps the integer cell coordinate out of the cell [cellMin, cellMax] (inclusive, finest units)
// along `axis`; the other axes come from the ray position at the exit time, clamped into the
// exited cell so float error can never place them outside it. Returns false when the new cell
// lies outside the root.
bool advance(const Traversal& tr, float t, int axis, const glm::ivec3& cellMin, const glm::ivec3& cellMax,
             glm::ivec3& c) noexcept {
    const glm::vec3 p = tr.o + t * tr.d;
    for (int a = 0; a < 3; ++a) {
        if (a == axis) {
            c[a] = tr.step[a] > 0 ? cellMax[a] + 1 : cellMin[a] - 1;
        } else {
            const auto f = static_cast<std::int32_t>(std::floor(p[a] * tr.cells));
            c[a] = std::clamp(f, cellMin[a], cellMax[a]);
        }
    }
    const auto n = static_cast<std::int32_t>(tr.cells);
    return c.x >= 0 && c.y >= 0 && c.z >= 0 && c.x < n && c.y < n && c.z < n;
}

// Exit time and axis of the axis-aligned cell [cellMin, cellMax] (finest units) along the ray.
float cell_exit(const Traversal& tr, const glm::ivec3& cellMin, const glm::ivec3& cellMax,
                int& axis) noexcept {
    glm::vec3 tExit;
    for (int a = 0; a < 3; ++a) {
        const float boundary = static_cast<float>(tr.step[a] > 0 ? cellMax[a] + 1 : cellMin[a]) / tr.cells;
        tExit[a] = (boundary - tr.o[a]) * tr.invd[a];
    }
    axis = argmin3(tExit);
    return tExit[axis];
}

} // namespace

Hit trace_ray(const BrickTree& tree, const Ray& ray, const TraceParams& params) noexcept {
    Hit miss;
    if (tree.empty()) {
        return miss;
    }
    const TreeGeometry& g = tree.geometry;
    Traversal tr;
    tr.V = g.voxel_bits();
    // The traversal stack (`stack[kMaxLevels]` below) is sized from kMaxVoxelBits (tree_layout.hpp's
    // "float has a 24-bit mantissa" ceiling); voxel_bits() is root_size_log2 - voxel_size_log2, both
    // set from user-supplied CLI flags (--region-log2/--voxel-log2) with no upstream clamp, so this
    // is a real boundary, not a defensive no-op -- caught once per ray rather than trusted from the
    // caller (clang-analyzer's ArrayBound flagged the unguarded stack access this closes).
    if (tr.V > kMaxVoxelBits) {
        return miss;
    }
    tr.cells = std::ldexp(1.0f, tr.V);
    const float invRoot = 1.0f / g.root_edge();
    tr.o = (ray.origin - g.origin) * invRoot;
    tr.d = ray.dir * invRoot;
    for (int a = 0; a < 3; ++a) {
        tr.step[a] = tr.d[a] >= 0.0f ? 1 : -1;
        tr.invd[a] =
            std::abs(tr.d[a]) > 1.0e-20f ? 1.0f / tr.d[a] : (tr.d[a] >= 0.0f ? kBigInverse : -kBigInverse);
    }

    // Slab test against the root [0,1]^3.
    float tEnter = 0.0f;
    float tExit = params.max_t;
    int enterAxis = -1;
    for (int a = 0; a < 3; ++a) {
        const float t0 = (0.0f - tr.o[a]) * tr.invd[a];
        const float t1 = (1.0f - tr.o[a]) * tr.invd[a];
        const float tNear = std::min(t0, t1);
        const float tFar = std::max(t0, t1);
        if (tNear > tEnter) {
            tEnter = tNear;
            enterAxis = a;
        }
        tExit = std::min(tExit, tFar);
    }
    if (tExit < tEnter) {
        return miss;
    }

    const bool inside =
        tr.o.x >= 0.0f && tr.o.y >= 0.0f && tr.o.z >= 0.0f && tr.o.x < 1.0f && tr.o.y < 1.0f && tr.o.z < 1.0f;
    float t = inside ? 0.0f : tEnter;
    int lastAxis = inside ? -1 : enterAxis;
    const auto n = static_cast<std::int32_t>(tr.cells);
    glm::ivec3 c;
    {
        const glm::vec3 p = tr.o + t * tr.d;
        for (int a = 0; a < 3; ++a) {
            c[a] = std::clamp(static_cast<std::int32_t>(std::floor(p[a] * tr.cells)), 0, n - 1);
        }
        if (!inside) {
            c[enterAxis] = tr.step[enterAxis] > 0 ? 0 : n - 1;
        }
    }

    std::uint32_t stack[kMaxLevels];
    stack[0] = tree.root;
    int level = 0;
    const std::uint32_t* nodes = tree.nodes.data();

    // `cubeEdge` is the hit cube's edge in world units; `attrLevel` the deepest level whose node
    // on the stack carries attributes for this hit (the brick leaf itself, or a solid leaf's
    // parent). The smoothing rule then walks up while the ancestor spans less than
    // t * smooth_pixel_angle.
    const auto make_hit = [&](float tHit, MaterialID material, int hitLevel, bool lodCube,
                              std::uint32_t steps, float cubeEdge, int attrLevel) {
        Hit h;
        h.hit = true;
        h.t = tHit;
        h.material = material;
        h.normal = normal_from(lastAxis, tr.step, tr.d);
        h.position = ray.origin + tHit * ray.dir;
        h.level = hitLevel;
        h.lod_cube = lodCube;
        h.solid_leaf = attrLevel < hitLevel; // only a solid leaf's attributes come from its parent
        h.steps = steps;
        h.cube_edge = cubeEdge;
        int L = attrLevel;
        if (params.smooth_pixel_angle > 0.0f) {
            const float wanted = tHit * params.smooth_pixel_angle;
            while (L > 0 && g.level_edge(L) < wanted) {
                --L;
            }
        }
        if (L >= 0) {
            const std::uint32_t attrHeader = nodes[stack[L]];
            const std::uint32_t slot = node_attr_slot(attrHeader);
            if (slot != kNoNode) {
                const std::uint32_t attr = nodes[stack[L] + slot];
                h.smooth_normal = node_attr_normal(attr);
                h.coverage = node_attr_coverage(attr);
                h.smooth_level = L;
            }
        }
        return h;
    };

    for (std::uint32_t iteration = 1; iteration <= kMaxIterations; ++iteration) {
        if (t > params.max_t) {
            miss.steps = iteration;
            return miss;
        }
        glm::ivec3 cellMin;
        glm::ivec3 cellMax;
        int exitAxis = 0;
        float tOut = 0.0f;

        // Descend from the current level to the deepest node containing cell c. `level` only ever
        // grows via `level = childLevel` below, one internal-node step at a time from an entry
        // state of <= max_brick_level() -- itself <= tr.V - 3, guarded <= kMaxVoxelBits - 3 above --
        // so it never reaches kMaxLevels (stack's extent). The analyzer can't trace that invariant
        // through the loop; verified by the 7,000-ray brute-force oracle (test_ray_trace.cpp).
        for (;;) {
            const std::uint32_t node = stack[level]; // NOLINT(clang-analyzer-security.ArrayBound)
            const std::uint32_t header = nodes[node];
            const std::uint32_t kind = node_kind(header);
            if (kind == kNodeKindSolid) {
                return make_hit(t, node_material(header), level, false, iteration, g.level_edge(level),
                                level - 1);
            }
            if (kind == kNodeKindBrick) {
                const int shift = tr.V - level - TreeGeometry::kBrickLog2; // finest bits below a brick voxel
                const int cellShift = tr.V - level;
                const std::uint32_t* bw = tree.brick_words(nodes[node + kNodeBrickIndexSlot]);
                glm::ivec3 v{(c.x >> shift) & 7, (c.y >> shift) & 7, (c.z >> shift) & 7};
                const glm::ivec3 brickCell{(c.x >> cellShift) << cellShift, (c.y >> cellShift) << cellShift,
                                           (c.z >> cellShift) << cellShift};
                const float voxelEdge = std::ldexp(1.0f, -(level + TreeGeometry::kBrickLog2)); // root units
                const glm::vec3 brickOrigin = glm::vec3{brickCell} / tr.cells;
                glm::vec3 tMax;
                glm::vec3 tDelta;
                for (int a = 0; a < 3; ++a) {
                    const float boundary =
                        brickOrigin[a] + static_cast<float>(v[a] + (tr.step[a] > 0 ? 1 : 0)) * voxelEdge;
                    tMax[a] = (boundary - tr.o[a]) * tr.invd[a];
                    tDelta[a] = voxelEdge * std::abs(tr.invd[a]);
                }
                for (;;) {
                    const std::size_t index = brick_voxel_index(v.x, v.y, v.z);
                    if (brick_word_occupied(bw, index)) {
                        return make_hit(t, brick_word_material(bw, index), level, false, iteration,
                                        g.level_voxel_edge(level), level);
                    }
                    const int axis = argmin3(tMax);
                    t = tMax[axis];
                    tMax[axis] += tDelta[axis];
                    v[axis] += tr.step[axis];
                    lastAxis = axis;
                    if (v[axis] < 0 || v[axis] > 7) {
                        break;
                    }
                }
                cellMin = brickCell;
                cellMax = brickCell + glm::ivec3{(1 << cellShift) - 1};
                exitAxis = lastAxis;
                tOut = t;
                break;
            }

            // Internal node.
            const int childLevel = level + 1;
            if (params.lod_pixel_angle > 0.0f) {
                const float childEdgeWorld = g.level_edge(childLevel);
                if (childEdgeWorld < (t + params.t_offset) * params.lod_pixel_angle &&
                    (params.lod_coverage_threshold <= 0.0f ||
                     node_attr_coverage(nodes[node + kNodeAttrSlotInternal]) >=
                         params.lod_coverage_threshold)) {
                    return make_hit(t, node_material(header), level, true, iteration, g.level_edge(level),
                                    level);
                }
            }
            const int sh = tr.V - childLevel;
            const int octant = octant_of(c.x >> sh, c.y >> sh, c.z >> sh);
            const std::uint32_t mask = node_child_mask(header);
            if ((mask & (1u << octant)) != 0u) {
                stack[childLevel] = nodes[node + node_child_slot(header, octant)];
                level = childLevel;
                continue;
            }
            // Empty child cell: leave it.
            cellMin = glm::ivec3{(c.x >> sh) << sh, (c.y >> sh) << sh, (c.z >> sh) << sh};
            cellMax = cellMin + glm::ivec3{(1 << sh) - 1};
            tOut = cell_exit(tr, cellMin, cellMax, exitAxis);
            lastAxis = exitAxis;
            break;
        }

        // Step into the neighboring cell and pop to the deepest common ancestor.
        const glm::ivec3 cOld = c;
        t = tOut;
        if (!advance(tr, tOut, exitAxis, cellMin, cellMax, c)) {
            miss.steps = iteration;
            return miss;
        }
        const auto diff = static_cast<std::uint32_t>((cOld.x ^ c.x) | (cOld.y ^ c.y) | (cOld.z ^ c.z));
        const int common = tr.V - static_cast<int>(std::bit_width(diff));
        level = std::min(level, common);
    }
    miss.steps = kMaxIterations;
    return miss;
}

Hit trace_ray_brute_force(const BrickTree& tree, const Ray& ray) noexcept {
    Hit miss;
    if (tree.empty()) {
        return miss;
    }
    const TreeGeometry& g = tree.geometry;
    const int V = g.voxel_bits();
    const float cells = std::ldexp(1.0f, V);
    const float invRoot = 1.0f / g.root_edge();
    const glm::vec3 o = (ray.origin - g.origin) * invRoot;
    const glm::vec3 d = ray.dir * invRoot;
    glm::vec3 invd;
    glm::ivec3 step;
    for (int a = 0; a < 3; ++a) {
        step[a] = d[a] >= 0.0f ? 1 : -1;
        invd[a] = std::abs(d[a]) > 1.0e-20f ? 1.0f / d[a] : (d[a] >= 0.0f ? kBigInverse : -kBigInverse);
    }
    float tEnter = 0.0f;
    float tExit = std::numeric_limits<float>::infinity();
    int enterAxis = -1;
    for (int a = 0; a < 3; ++a) {
        const float t0 = (0.0f - o[a]) * invd[a];
        const float t1 = (1.0f - o[a]) * invd[a];
        if (std::min(t0, t1) > tEnter) {
            tEnter = std::min(t0, t1);
            enterAxis = a;
        }
        tExit = std::min(tExit, std::max(t0, t1));
    }
    if (tExit < tEnter) {
        return miss;
    }
    const bool inside = o.x >= 0.0f && o.y >= 0.0f && o.z >= 0.0f && o.x < 1.0f && o.y < 1.0f && o.z < 1.0f;
    float t = inside ? 0.0f : tEnter;
    int lastAxis = inside ? -1 : enterAxis;
    const auto n = static_cast<std::int32_t>(cells);
    glm::ivec3 v;
    {
        const glm::vec3 p = o + t * d;
        for (int a = 0; a < 3; ++a) {
            v[a] = std::clamp(static_cast<std::int32_t>(std::floor(p[a] * cells)), 0, n - 1);
        }
        if (!inside) {
            v[enterAxis] = step[enterAxis] > 0 ? 0 : n - 1;
        }
    }
    const float voxelEdge = 1.0f / cells;
    glm::vec3 tMax;
    glm::vec3 tDelta;
    for (int a = 0; a < 3; ++a) {
        const float boundary = static_cast<float>(v[a] + (step[a] > 0 ? 1 : 0)) * voxelEdge;
        tMax[a] = (boundary - o[a]) * invd[a];
        tDelta[a] = voxelEdge * std::abs(invd[a]);
    }
    for (std::uint32_t iteration = 1; iteration <= 4u * static_cast<std::uint32_t>(n); ++iteration) {
        const glm::vec3 center = g.origin + (glm::vec3{v} + 0.5f) * (g.root_edge() / cells);
        const MaterialID m = tree.material_at(center);
        if (m != MaterialID::Air) {
            Hit h;
            h.hit = true;
            h.t = t;
            h.material = m;
            h.normal = normal_from(lastAxis, step, d);
            h.position = ray.origin + t * ray.dir;
            h.level = g.max_brick_level();
            h.steps = iteration;
            h.cube_edge = g.finest_voxel_edge();
            return h;
        }
        const int axis = argmin3(tMax);
        t = tMax[axis];
        tMax[axis] += tDelta[axis];
        v[axis] += step[axis];
        lastAxis = axis;
        if (v[axis] < 0 || v[axis] >= n) {
            break;
        }
    }
    return miss;
}

} // namespace world::svo
