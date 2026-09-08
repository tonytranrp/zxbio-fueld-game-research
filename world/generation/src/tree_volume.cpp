#include "world/generation/tree_volume.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace world::generation {

namespace {

using world::chunk::MaterialID;

constexpr float kPi = 3.14159265358979323846f;

// Squared distance from `p` to the segment `a..b`. The one geometric primitive this file needs; a
// ball is the degenerate case with a == b, which is why both live in one `TreePrimitive`.
[[nodiscard]] float distance_sq_to_segment(const glm::vec3& p, const glm::vec3& a,
                                           const glm::vec3& b) noexcept {
    const glm::vec3 ab = b - a;
    const float denom = glm::dot(ab, ab);
    float t = 0.0f;
    if (denom > 1.0e-12f) {
        t = std::clamp(glm::dot(p - a, ab) / denom, 0.0f, 1.0f);
    }
    const glm::vec3 closest = a + t * ab;
    const glm::vec3 d = p - closest;
    return glm::dot(d, d);
}

[[nodiscard]] bool boxes_overlap(const glm::vec3& aLo, const glm::vec3& aHi, const glm::vec3& bLo,
                                 const glm::vec3& bHi) noexcept {
    return aLo.x <= bHi.x && aHi.x >= bLo.x && aLo.y <= bHi.y && aHi.y >= bLo.y && aLo.z <= bHi.z &&
           aHi.z >= bLo.z;
}

} // namespace

TreeVolume::TreeVolume(const TreeSkeleton& skeleton, const TreeVolumeParams& params) {
    woodMaterial_ = params.wood_material;
    leafMaterial_ = params.leaf_material;
    if (skeleton.empty()) {
        return;
    }
    const std::size_t n = skeleton.segments.size();

    // --- branches ---------------------------------------------------------------------------------
    primitives_.reserve(n * 2u);
    for (const SkeletonSegment& s : skeleton.segments) {
        TreePrimitive prim;
        prim.a = s.start;
        prim.b = s.end;
        prim.radius = std::max(s.radius, params.min_branch_radius);
        prim.leaf = false;
        primitives_.push_back(prim);
    }

    // --- leaf clouds ------------------------------------------------------------------------------
    // A segment's OWN leaf area is what its children do not already carry (`leaf_count` is distal),
    // the same decomposition `tree_sway.cpp` does. Area becomes volume through the leaf area density
    // and volume becomes a radius, so a denser crown is a bigger cloud rather than a magic constant.
    std::vector<float> own(n, 0.0f);
    for (std::size_t i = 0; i < n; ++i) {
        own[i] = skeleton.segments[i].leaf_count;
    }
    for (std::size_t i = n; i-- > 1;) {
        const std::int32_t p = skeleton.segments[i].parent;
        if (p >= 0) {
            own[static_cast<std::size_t>(p)] -= skeleton.segments[i].leaf_count;
        }
    }
    // The crown's own volume, as the ellipsoid enclosing every leaf-bearing tip, and the density
    // that spreads this tree's leaf area through it. See the header for why this is derived.
    glm::vec3 crownLo{std::numeric_limits<float>::max()};
    glm::vec3 crownHi{std::numeric_limits<float>::lowest()};
    float totalLeaf = 0.0f;
    for (std::size_t i = 0; i < n; ++i) {
        if (own[i] <= 0.0f) {
            continue;
        }
        crownLo = glm::min(crownLo, skeleton.segments[i].end);
        crownHi = glm::max(crownHi, skeleton.segments[i].end);
        totalLeaf += own[i];
    }
    float density = params.max_leaf_area_density;
    if (totalLeaf > 0.0f && crownHi.x >= crownLo.x) {
        const glm::vec3 halfSpan = 0.5f * glm::max(crownHi - crownLo, glm::vec3{0.5f});
        const float crownVolume = (4.0f / 3.0f) * kPi * halfSpan.x * halfSpan.y * halfSpan.z;
        density = std::clamp(totalLeaf / std::max(crownVolume, 1.0e-3f), params.min_leaf_area_density,
                             params.max_leaf_area_density);
    }
    for (std::size_t i = 0; i < n; ++i) {
        const float area = std::max(0.0f, own[i]);
        if (area <= 0.0f) {
            continue;
        }
        const float volume = area / density;
        float radius = std::cbrt(3.0f * volume / (4.0f * kPi));
        if (radius < params.min_leaf_radius) {
            // Too small to be worth a primitive at any voxel size this engine uses. Rolled into the
            // parent rather than dropped, so the crown keeps its leaf area -- dropping it is how a
            // canopy silently thins out at exactly the seeds with the finest subdivision.
            const std::int32_t p = skeleton.segments[i].parent;
            if (p >= 0) {
                own[static_cast<std::size_t>(p)] += area;
            }
            continue;
        }
        radius = std::min(radius, params.max_leaf_radius);
        // TWO clouds per leaf-bearing segment, at its midpoint and its tip, each carrying half the
        // area. One cloud per tip was tried first and rendered as a bunch of separate balls on a
        // stick -- a real crown reads as a mass, not as spheres. Two at half the volume each are
        // 0.79x the radius but half the spacing, so they overlap and fuse, and the silhouette gains
        // the lumpiness a single sphere per tip cannot have. Found by looking at the capture; the
        // brick count barely moved (measured, +0.1%).
        const float half = radius * 0.7937005f; // (1/2)^(1/3)
        // A deterministic per-segment jitter, so no two clouds in a crown are exactly the same size.
        // Hashed from the index alone: the same skeleton gives the same crown in every tool.
        const std::uint32_t h = (static_cast<std::uint32_t>(i) * 2654435761u) ^ 0x9e3779b9u;
        const float jitter = 0.85f + 0.30f * static_cast<float>((h >> 8) & 0xFFFFu) / 65535.0f;
        for (int k = 0; k < 2; ++k) {
            TreePrimitive prim;
            prim.a = k == 0 ? 0.5f * (skeleton.segments[i].start + skeleton.segments[i].end)
                            : skeleton.segments[i].end;
            prim.b = prim.a;
            prim.radius = half * (k == 0 ? 2.0f - jitter : jitter);
            prim.leaf = true;
            primitives_.push_back(prim);
        }
    }

    // --- bounds and the uniform grid ----------------------------------------------------------------
    glm::vec3 lo{std::numeric_limits<float>::max()};
    glm::vec3 hi{std::numeric_limits<float>::lowest()};
    float largest = 0.0f;
    for (const TreePrimitive& p : primitives_) {
        lo = glm::min(lo, glm::min(p.a, p.b) - glm::vec3{p.radius});
        hi = glm::max(hi, glm::max(p.a, p.b) + glm::vec3{p.radius});
        largest = std::max(largest, p.radius);
    }
    bounds_.min = lo;
    bounds_.max = hi;

    // Cells sized to the MEAN primitive radius, not the largest. Sizing them to the largest was the
    // obvious choice and the wrong one: the largest primitive in a crown is a leaf cloud near a
    // metre across, which makes every cell two metres wide and puts a dozen clouds in each -- and
    // `material_at` then distance-tests all dozen for every voxel. Mean-sized cells make a
    // primitive span a few cells (cheap, this is ~200 primitives) and the candidate list short
    // (not cheap, this is the hottest loop in the voxelizer).
    gridMin_ = lo;
    float meanRadius = 0.0f;
    for (const TreePrimitive& p : primitives_) {
        meanRadius += p.radius;
    }
    meanRadius /= static_cast<float>(primitives_.size());
    cellSize_ = std::max(2.0f * meanRadius, 0.12f);
    (void)largest;
    const glm::vec3 span = hi - lo;
    for (int axis = 0; axis < 3; ++axis) {
        dims_[axis] = std::clamp(static_cast<int>(std::ceil(span[axis] / cellSize_)), 1, 96);
    }
    const auto cells = static_cast<std::size_t>(dims_.x) * static_cast<std::size_t>(dims_.y) *
                       static_cast<std::size_t>(dims_.z);

    // Counting sort into the grid: one pass to count, a prefix sum, one pass to place. No vector of
    // vectors, which at ~200 primitives would cost more in allocator traffic than in the sort.
    const auto cell_range = [&](const TreePrimitive& p, glm::ivec3& c0, glm::ivec3& c1) {
        const glm::vec3 pLo = glm::min(p.a, p.b) - glm::vec3{p.radius};
        const glm::vec3 pHi = glm::max(p.a, p.b) + glm::vec3{p.radius};
        for (int axis = 0; axis < 3; ++axis) {
            c0[axis] =
                std::clamp(static_cast<int>((pLo[axis] - gridMin_[axis]) / cellSize_), 0, dims_[axis] - 1);
            c1[axis] =
                std::clamp(static_cast<int>((pHi[axis] - gridMin_[axis]) / cellSize_), 0, dims_[axis] - 1);
        }
    };
    const auto index_of = [&](int x, int y, int z) {
        return static_cast<std::size_t>((z * dims_.y + y) * dims_.x + x);
    };

    cellStart_.assign(cells + 1u, 0u);
    std::size_t total = 0;
    for (const TreePrimitive& p : primitives_) {
        glm::ivec3 c0{0};
        glm::ivec3 c1{0};
        cell_range(p, c0, c1);
        for (int z = c0.z; z <= c1.z; ++z) {
            for (int y = c0.y; y <= c1.y; ++y) {
                for (int x = c0.x; x <= c1.x; ++x) {
                    ++cellStart_[index_of(x, y, z) + 1u];
                    ++total;
                }
            }
        }
    }
    for (std::size_t i = 1; i < cellStart_.size(); ++i) {
        cellStart_[i] += cellStart_[i - 1];
    }
    cellItems_.assign(total, 0u);
    std::vector<std::uint32_t> cursor(cellStart_.begin(), cellStart_.end() - 1);
    for (std::uint32_t i = 0; i < primitives_.size(); ++i) {
        glm::ivec3 c0{0};
        glm::ivec3 c1{0};
        cell_range(primitives_[i], c0, c1);
        for (int z = c0.z; z <= c1.z; ++z) {
            for (int y = c0.y; y <= c1.y; ++y) {
                for (int x = c0.x; x <= c1.x; ++x) {
                    cellItems_[cursor[index_of(x, y, z)]++] = i;
                }
            }
        }
    }
}

void TreeVolume::gather(const glm::vec3& lo, const glm::vec3& hi, std::vector<std::uint32_t>& out) const {
    out.clear();
    if (primitives_.empty()) {
        return;
    }
    glm::ivec3 c0{0};
    glm::ivec3 c1{0};
    for (int axis = 0; axis < 3; ++axis) {
        const float a = (lo[axis] - gridMin_[axis]) / cellSize_;
        const float b = (hi[axis] - gridMin_[axis]) / cellSize_;
        if (b < 0.0f || a > static_cast<float>(dims_[axis])) {
            return; // wholly outside the tree
        }
        c0[axis] = std::clamp(static_cast<int>(std::floor(a)), 0, dims_[axis] - 1);
        c1[axis] = std::clamp(static_cast<int>(std::floor(b)), 0, dims_[axis] - 1);
    }
    for (int z = c0.z; z <= c1.z; ++z) {
        for (int y = c0.y; y <= c1.y; ++y) {
            for (int x = c0.x; x <= c1.x; ++x) {
                const auto cell = static_cast<std::size_t>((z * dims_.y + y) * dims_.x + x);
                for (std::size_t i = cellStart_[cell]; i < cellStart_[cell + 1u]; ++i) {
                    out.push_back(cellItems_[i]);
                }
            }
        }
    }
    // One primitive spans several cells, so the same index can arrive several times. Sorting and
    // uniquing beats testing it twice: the geometry test is the expensive half.
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
}

MaterialID TreeVolume::material_at(const glm::vec3& p) const {
    if (primitives_.empty() || p.x < bounds_.min.x || p.x > bounds_.max.x || p.y < bounds_.min.y ||
        p.y > bounds_.max.y || p.z < bounds_.min.z || p.z > bounds_.max.z) {
        return MaterialID::Air;
    }
    // A POINT lands in exactly ONE cell, so there is nothing to deduplicate and `gather`'s sort is
    // pure waste here. It was not: this is the hottest function in the whole voxelizer -- one call
    // per candidate voxel, at 7.8 mm, over a canopy -- and running std::sort inside it cost 38% of
    // the octree build. Measured before and after; see the goal 336 entry.
    glm::ivec3 cell{0};
    for (int axis = 0; axis < 3; ++axis) {
        cell[axis] = std::clamp(static_cast<int>((p[axis] - gridMin_[axis]) / cellSize_), 0,
                                dims_[axis] - 1);
    }
    const auto index = static_cast<std::size_t>((cell.z * dims_.y + cell.y) * dims_.x + cell.x);

    MaterialID out = MaterialID::Air;
    for (std::size_t slot = cellStart_[index]; slot < cellStart_[index + 1u]; ++slot) {
        const std::uint32_t i = cellItems_[slot];
        const TreePrimitive& prim = primitives_[i];
        if (distance_sq_to_segment(p, prim.a, prim.b) > prim.radius * prim.radius) {
            continue;
        }
        if (!prim.leaf) {
            return woodMaterial_; // wood wins outright, as it does in the implicit shape
        }
        out = leafMaterial_;
    }
    return out;
}

bool TreeVolume::intersects_box(const glm::vec3& lo, const glm::vec3& hi) const {
    if (primitives_.empty() || !boxes_overlap(lo, hi, bounds_.min, bounds_.max)) {
        return false;
    }
    thread_local std::vector<std::uint32_t> candidates;
    gather(lo, hi, candidates);
    for (const std::uint32_t i : candidates) {
        const TreePrimitive& prim = primitives_[i];
        // Exact: the closest point of the box to the segment's own closest point. Two clamps rather
        // than a full segment-box distance, which is conservative in the ALLOWED direction -- this
        // predicate may say yes too often, never no too often.
        const glm::vec3 mid = 0.5f * (prim.a + prim.b);
        const glm::vec3 onBox = glm::clamp(mid, lo, hi);
        const glm::vec3 ab = prim.b - prim.a;
        const float denom = glm::dot(ab, ab);
        const float t =
            denom > 1.0e-12f ? std::clamp(glm::dot(onBox - prim.a, ab) / denom, 0.0f, 1.0f) : 0.0f;
        const glm::vec3 onSeg = prim.a + t * ab;
        const glm::vec3 nearest = glm::clamp(onSeg, lo, hi);
        const glm::vec3 d = nearest - onSeg;
        if (glm::dot(d, d) <= prim.radius * prim.radius) {
            return true;
        }
    }
    return false;
}

bool TreeVolume::contains_box(const glm::vec3& lo, const glm::vec3& hi) const {
    if (primitives_.empty()) {
        return false;
    }
    thread_local std::vector<std::uint32_t> candidates;
    gather(lo, hi, candidates);
    for (const std::uint32_t i : candidates) {
        const TreePrimitive& prim = primitives_[i];
        // A capsule and a ball are both convex, so a box is inside iff all eight corners are. Exact,
        // and false negatives (a box filled by two overlapping primitives and no single one) are
        // allowed by the contract -- they cost a subdivision, not a hole.
        const float r2 = prim.radius * prim.radius;
        bool all = true;
        for (int corner = 0; corner < 8 && all; ++corner) {
            const glm::vec3 c{(corner & 1) != 0 ? hi.x : lo.x, (corner & 2) != 0 ? hi.y : lo.y,
                              (corner & 4) != 0 ? hi.z : lo.z};
            all = distance_sq_to_segment(c, prim.a, prim.b) <= r2;
        }
        if (all) {
            return true;
        }
    }
    return false;
}

std::size_t TreeVolume::memory_bytes() const noexcept {
    return primitives_.capacity() * sizeof(TreePrimitive) + cellStart_.capacity() * sizeof(std::uint32_t) +
           cellItems_.capacity() * sizeof(std::uint32_t);
}

TreeVolume tree_volume_for(int seed, const TreePlacement& tree, const TreeVolumeParams& params) {
    TreeSkeleton skeleton = grow_skeleton_for(seed, tree);
    if (skeleton.empty()) {
        return {};
    }
    apply_pipe_model(skeleton);
    return TreeVolume{skeleton, params};
}

} // namespace world::generation
