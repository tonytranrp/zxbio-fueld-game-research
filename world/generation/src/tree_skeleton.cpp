#include "world/generation/tree_skeleton.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace world::generation {

namespace {

// The same splitmix-style mixing the placement grid uses: deterministic, cheap, and good enough
// for scattering points in a volume. Not a general-purpose RNG -- it is a hash, so the Nth value
// for a tree does not depend on how many values anything else drew.
[[nodiscard]] std::uint32_t mix(std::uint32_t x) noexcept {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

[[nodiscard]] float unit_float(std::uint32_t h) noexcept {
    // [0, 1) from the top 24 bits: the low bits of a multiply-xorshift hash are the weakest.
    return static_cast<float>(h >> 8) * (1.0f / 16777216.0f);
}

// A point inside the crown ellipsoid, tapered toward the top. Rejection-free: sample the height
// first, then a disc of the radius that height allows, so the taper is exact rather than
// approximated by throwing points away.
[[nodiscard]] glm::vec3 crown_attractor(const SpeciesParams& params, std::uint32_t index,
                                        std::uint32_t salt) noexcept {
    const std::uint32_t h0 = mix(index * 0x9E3779B9U + salt);
    const std::uint32_t h1 = mix(h0 ^ 0xB5297A4DU);
    const std::uint32_t h2 = mix(h1 ^ 0x68E31DA4U);

    // Height within the crown, biased slightly toward the middle by averaging two samples -- a
    // uniform column puts as many attractors in the topmost slice as the widest one, and the crown
    // grows a flat top.
    const float tRaw = 0.5f * (unit_float(h0) + unit_float(h1));
    const float y = params.crown_centre_height + (tRaw - 0.5f) * params.crown_height;

    // Horizontal radius available at this height: the ellipsoid's own profile, then the taper.
    const float norm =
        params.crown_height > 0.0f ? (y - params.crown_centre_height) / (0.5f * params.crown_height) : 0.0f;
    const float ellipse = std::sqrt(std::max(0.0f, 1.0f - norm * norm));
    const float taper = 1.0f - params.crown_taper * std::clamp(tRaw, 0.0f, 1.0f);
    const float maxRadius = params.crown_radius * ellipse * std::max(0.0f, taper);

    // sqrt for a uniform disc, not a centre-heavy one.
    const float radius = maxRadius * std::sqrt(unit_float(h2));
    const float angle = unit_float(mix(h2 ^ 0x1B56C4E9U)) * 6.283185307179586f;
    return glm::vec3{radius * std::cos(angle), y, radius * std::sin(angle)};
}

} // namespace

TreeBounds TreeSkeleton::bounds() const noexcept {
    TreeBounds b;
    if (segments.empty()) {
        return b;
    }
    b.min = segments.front().start;
    b.max = segments.front().start;
    for (const SkeletonSegment& s : segments) {
        b.min = glm::min(b.min, glm::min(s.start, s.end));
        b.max = glm::max(b.max, glm::max(s.start, s.end));
    }
    return b;
}

SpeciesParams species_params(TreeSpecies species) noexcept {
    SpeciesParams p;
    switch (species) {
    case TreeSpecies::RoundBroadleaf:
        // A dome on a bare stem: wide crown, no taper.
        p.crown_radius = 3.0f;
        p.crown_height = 5.0f;
        p.crown_centre_height = 6.0f;
        p.crown_taper = 0.0f;
        p.trunk_height = 3.5f;
        p.flutter_response = 1.0f;
        break;
    case TreeSpecies::Conifer:
        // A cone: narrower, taller, and tapering to a point, which is what the v1 silhouette's
        // three stacked shrinking octahedra were approximating.
        p.crown_radius = 2.2f;
        p.crown_height = 8.0f;
        p.crown_centre_height = 6.5f;
        p.crown_taper = 0.95f;
        p.trunk_height = 1.5f;
        p.segment_length = 0.35f;
        p.attraction_radius = 2.2f;
        p.kill_radius = 0.6f;
        p.flutter_response = 0.4f; // stiff needles barely flutter
        break;
    case TreeSpecies::Shrub:
        // No trunk, squashed, and small enough that it needs far fewer attractors -- spending 220
        // on a 1.5 m bush is most of the cost of a whole forest for none of the shape.
        p.crown_radius = 1.4f;
        p.crown_height = 1.8f;
        p.crown_centre_height = 1.0f;
        p.crown_taper = 0.2f;
        p.trunk_height = 0.0f;
        p.segment_length = 0.25f;
        p.attraction_radius = 1.2f;
        p.kill_radius = 0.35f;
        p.attractor_count = 70;
        p.max_iterations = 90;
        p.flutter_response = 1.2f;
        break;
    case TreeSpecies::Aspen:
        // Tall, narrow, high-flutter: the showcase for the wind work. A flattened petiole is why a
        // real aspen shimmers in wind that barely moves an oak (tree research §4.4).
        p.crown_radius = 2.0f;
        p.crown_height = 6.0f;
        p.crown_centre_height = 8.0f;
        p.crown_taper = 0.25f;
        p.trunk_height = 5.5f;
        p.segment_length = 0.38f;
        p.flutter_response = 2.6f;
        break;
    }
    return p;
}

TreeSpecies species_of(const TreePlacement& tree) noexcept {
    switch (tree.shape) {
    case TreeShape::Conifer:
        return TreeSpecies::Conifer;
    case TreeShape::Shrub:
        return TreeSpecies::Shrub;
    case TreeShape::Round:
        break;
    }
    // A deterministic minority of round broadleaves are aspens, so the high-flutter species exists
    // in the world without being placed by a separate rule.
    // Both multiplies happen in UNSIGNED arithmetic, where wraparound is defined. Doing them in
    // int is the classic spatial-hash mistake: these constants overflow int32 for any coordinate
    // past ~29, so `48 * 73856093` is undefined behaviour -- which is exactly what UBSan caught
    // here, on a tree at x = 12. The hash wants wraparound; it just has to ask for it legally.
    const auto gx = static_cast<std::uint32_t>(static_cast<std::int32_t>(tree.world_x * 4.0f));
    const auto gz = static_cast<std::uint32_t>(static_cast<std::int32_t>(tree.world_z * 4.0f));
    const std::uint32_t key = (gx * 73856093U) ^ (gz * 19349663U);
    return (mix(key) % 5u) == 0u ? TreeSpecies::Aspen : TreeSpecies::RoundBroadleaf;
}

TreeSkeleton grow_skeleton(int seed, glm::vec3 base, const SpeciesParams& params) {
    TreeSkeleton skeleton;
    if (params.segment_length <= 0.0f || params.attractor_count == 0) {
        return skeleton;
    }
    const auto salt = static_cast<std::uint32_t>(seed) * 0x27D4EB2FU;

    // --- attractors ------------------------------------------------------------------------------
    std::vector<glm::vec3> attractors;
    attractors.reserve(params.attractor_count);
    for (std::uint32_t i = 0; i < params.attractor_count; ++i) {
        attractors.push_back(base + crown_attractor(params, i, salt));
    }
    std::vector<bool> consumed(attractors.size(), false);

    // --- the stem --------------------------------------------------------------------------------
    // Grow straight up until a tip is within reach of the crown, THEN colonize. Without this the
    // first iteration branches at ground level and the tree has no trunk.
    std::vector<glm::vec3> nodes; // node positions; index matches `nodeParentSegment`
    std::vector<std::int32_t> nodeParentSegment;
    nodes.push_back(base);
    nodeParentSegment.push_back(-1);

    const float crownBottom = params.crown_centre_height - 0.5f * params.crown_height;
    const float stemTop = std::max(params.trunk_height, crownBottom - params.attraction_radius * 0.5f);
    glm::vec3 tip = base;
    while (tip.y - base.y + params.segment_length <= stemTop) {
        const glm::vec3 next = tip + glm::vec3{0.0f, params.segment_length, 0.0f};
        SkeletonSegment s;
        s.parent = static_cast<std::int32_t>(skeleton.segments.size()) - 1;
        s.start = tip;
        s.end = next;
        skeleton.segments.push_back(s);
        nodes.push_back(next);
        nodeParentSegment.push_back(static_cast<std::int32_t>(skeleton.segments.size()) - 1);
        tip = next;
    }

    // --- space colonization ------------------------------------------------------------------------
    const float attractionSq = params.attraction_radius * params.attraction_radius;
    const float killSq = params.kill_radius * params.kill_radius;

    std::vector<glm::vec3> pull(nodes.size(), glm::vec3{0.0f});
    std::vector<std::uint32_t> pullCount(nodes.size(), 0u);

    for (std::uint32_t iteration = 0; iteration < params.max_iterations; ++iteration) {
        pull.assign(nodes.size(), glm::vec3{0.0f});
        pullCount.assign(nodes.size(), 0u);

        // Every live attractor votes for its single nearest node, and only if that node is close
        // enough to be influenced. One vote each is what keeps a dense clump from dragging every
        // nearby tip into the same place.
        bool anyVote = false;
        for (std::size_t a = 0; a < attractors.size(); ++a) {
            if (consumed[a]) {
                continue;
            }
            float bestSq = attractionSq;
            std::size_t best = nodes.size();
            for (std::size_t n = 0; n < nodes.size(); ++n) {
                const glm::vec3 d = attractors[a] - nodes[n];
                const float dSq = glm::dot(d, d);
                if (dSq < bestSq) {
                    bestSq = dSq;
                    best = n;
                }
            }
            if (best < nodes.size()) {
                const glm::vec3 dir = attractors[a] - nodes[best];
                const float len = glm::length(dir);
                if (len > 1.0e-5f) {
                    pull[best] += dir / len;
                    ++pullCount[best];
                    anyVote = true;
                }
            }
        }
        if (!anyVote) {
            break; // nothing in reach: the crown is either filled or unreachable
        }

        // Grow one segment from every node that got votes. Iterating over the ORIGINAL node count
        // matters: nodes added this round must not grow again until the next one, or a single tip
        // runs away across the crown in one iteration.
        const std::size_t growable = nodes.size();
        for (std::size_t n = 0; n < growable; ++n) {
            if (pullCount[n] == 0u) {
                continue;
            }
            glm::vec3 dir = pull[n];
            const float len = glm::length(dir);
            if (len <= 1.0e-5f) {
                continue;
            }
            dir /= len;
            // A touch of upward bias: real branches fight gravity, and without it a crown grown
            // from a symmetric attractor cloud droops into a disc.
            dir = glm::normalize(dir + glm::vec3{0.0f, 0.25f, 0.0f});
            const glm::vec3 next = nodes[n] + dir * params.segment_length;

            SkeletonSegment s;
            s.parent = nodeParentSegment[n];
            s.start = nodes[n];
            s.end = next;
            skeleton.segments.push_back(s);
            nodes.push_back(next);
            nodeParentSegment.push_back(static_cast<std::int32_t>(skeleton.segments.size()) - 1);
        }

        // Consume the attractors the new tips have reached.
        for (std::size_t a = 0; a < attractors.size(); ++a) {
            if (consumed[a]) {
                continue;
            }
            for (std::size_t n = growable; n < nodes.size(); ++n) {
                const glm::vec3 d = attractors[a] - nodes[n];
                if (glm::dot(d, d) < killSq) {
                    consumed[a] = true;
                    break;
                }
            }
        }
    }
    return skeleton;
}

TreeSkeleton grow_skeleton_for(int seed, const TreePlacement& tree) {
    const TreeSpecies species = species_of(tree);
    SpeciesParams params = species_params(species);
    // Fit the preset to the placement the world already decided on, so a v2 tree stands where the
    // v1 tree stood and fills the space it filled.
    if (tree.canopy_radius > 0.0f) {
        const float scale = tree.canopy_radius / params.crown_radius;
        params.crown_radius = tree.canopy_radius;
        params.crown_height *= scale;
        params.crown_centre_height =
            tree.trunk_height + 0.5f * params.crown_height * (species == TreeSpecies::Shrub ? 0.0f : 1.0f);
        params.trunk_height = tree.trunk_height;
    }
    const glm::vec3 base{tree.world_x, tree.base_height, tree.world_z};
    return grow_skeleton(seed, base, params);
}

std::vector<std::size_t> leaf_segments(const TreeSkeleton& skeleton) {
    std::vector<bool> hasChild(skeleton.segments.size(), false);
    for (const SkeletonSegment& s : skeleton.segments) {
        if (s.parent >= 0) {
            hasChild[static_cast<std::size_t>(s.parent)] = true;
        }
    }
    std::vector<std::size_t> tips;
    for (std::size_t i = 0; i < skeleton.segments.size(); ++i) {
        if (!hasChild[i]) {
            tips.push_back(i);
        }
    }
    return tips;
}

float crown_footprint(const TreeSkeleton& skeleton) noexcept {
    if (skeleton.segments.empty()) {
        return 0.0f;
    }
    // The disc the crown covers, from the skeleton's own horizontal extent about the trunk. Using
    // what GREW rather than what was asked for means a stunted tree carries less leaf, which is
    // both physically right and what keeps LAI a meaningful number.
    const glm::vec3 base = skeleton.segments.front().start;
    float far = 0.0f;
    for (const SkeletonSegment& s : skeleton.segments) {
        const float dx = s.end.x - base.x;
        const float dz = s.end.z - base.z;
        far = std::max(far, std::sqrt(dx * dx + dz * dz));
    }
    return 3.14159265f * far * far;
}

void apply_pipe_model(TreeSkeleton& skeleton, const PipeModelParams& params) {
    if (skeleton.segments.empty()) {
        return;
    }
    // Which segments are tips. Leaves hang on those and nowhere else -- a leaf halfway down a
    // branch would break the accumulation below, and real leaves are distal anyway.
    std::vector<bool> hasChild(skeleton.segments.size(), false);
    for (const SkeletonSegment& s : skeleton.segments) {
        if (s.parent >= 0) {
            hasChild[static_cast<std::size_t>(s.parent)] = true;
        }
    }
    std::size_t tipCount = 0;
    for (std::size_t i = 0; i < skeleton.segments.size(); ++i) {
        if (!hasChild[i]) {
            ++tipCount;
        }
    }
    if (tipCount == 0) {
        return;
    }

    // Leaf area is a property of the CROWN: LAI x footprint, shared over whatever tips grew. Tip
    // count varies several-fold between seeds, so hanging a fixed area on each one would make the
    // same species come out at wildly different densities -- see PipeModelParams.
    const float totalLeaf = params.leaf_area_index * crown_footprint(skeleton);
    const float perTip = totalLeaf / static_cast<float>(tipCount);
    for (std::size_t i = 0; i < skeleton.segments.size(); ++i) {
        skeleton.segments[i].leaf_count = hasChild[i] ? 0.0f : perTip;
    }

    // ONE backward pass accumulates every segment's distal leaf area into its parent. This works
    // only because a parent is always EARLIER in the list than its children (grow_skeleton
    // guarantees it, and a test pins it), so by the time index i is read, every child of i has
    // already been visited. No recursion, no second structure, no sort.
    for (std::size_t i = skeleton.segments.size(); i-- > 0;) {
        const SkeletonSegment& s = skeleton.segments[i];
        if (s.parent >= 0) {
            skeleton.segments[static_cast<std::size_t>(s.parent)].leaf_count += s.leaf_count;
        }
    }

    // Pipe model: cross-sectional AREA is proportional to the leaf it supplies, so the radius is a
    // square root. That is also why branch junctions satisfy da Vinci's rule for free -- the parent
    // carries the sum of its children's leaf, so its area is the sum of their areas.
    for (SkeletonSegment& s : skeleton.segments) {
        const float area = params.area_per_leaf_unit * s.leaf_count;
        s.radius = std::max(params.min_radius, std::sqrt(area / 3.14159265f));
    }
}

float total_leaf_area(const TreeSkeleton& skeleton) {
    // The root carries everything distal to it, which is the whole tree -- but sum the tips instead
    // of trusting that, so this stays correct for a skeleton with several roots.
    float total = 0.0f;
    std::vector<bool> hasChild(skeleton.segments.size(), false);
    for (const SkeletonSegment& s : skeleton.segments) {
        if (s.parent >= 0) {
            hasChild[static_cast<std::size_t>(s.parent)] = true;
        }
    }
    for (std::size_t i = 0; i < skeleton.segments.size(); ++i) {
        if (!hasChild[i]) {
            total += skeleton.segments[i].leaf_count;
        }
    }
    return total;
}

} // namespace world::generation
