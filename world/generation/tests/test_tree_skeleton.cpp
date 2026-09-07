#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <cstring>
#include <map>
#include <vector>

#include "world/generation/tree_skeleton.hpp"

using namespace world::generation;

namespace {

TreeSkeleton grown(TreeSpecies species, int seed = 1337, glm::vec3 base = {0.0f, 0.0f, 0.0f}) {
    return grow_skeleton(seed, base, species_params(species));
}

constexpr TreeSpecies kAll[] = {TreeSpecies::RoundBroadleaf, TreeSpecies::Conifer, TreeSpecies::Shrub,
                                TreeSpecies::Aspen};

} // namespace

TEST_CASE("Every species actually grows something", "[trees][skeleton]") {
    for (const TreeSpecies s : kAll) {
        const TreeSkeleton tree = grown(s);
        INFO("species " << static_cast<int>(s));
        REQUIRE_FALSE(tree.empty());
        REQUIRE(tree.segments.size() > 20); // a handful of segments is not a tree
    }
}

TEST_CASE("The skeleton is deterministic to the byte", "[trees][skeleton]") {
    // Goal 186's determinism check, and the project-wide standard: same seed and position, same
    // tree, in every tool and on every run.
    for (const TreeSpecies s : kAll) {
        const TreeSkeleton a = grown(s, 4242, {13.0f, 7.5f, -22.0f});
        const TreeSkeleton b = grown(s, 4242, {13.0f, 7.5f, -22.0f});
        REQUIRE(a.segments.size() == b.segments.size());
        REQUIRE(std::memcmp(a.segments.data(), b.segments.data(),
                            a.segments.size() * sizeof(SkeletonSegment)) == 0);
    }
    // ...and a different seed gives a different tree, or "deterministic" would be trivially true
    // of a function that ignores its seed.
    const TreeSkeleton one = grown(TreeSpecies::RoundBroadleaf, 1);
    const TreeSkeleton other = grown(TreeSpecies::RoundBroadleaf, 999999);
    const bool differs = one.segments.size() != other.segments.size() ||
                         std::memcmp(one.segments.data(), other.segments.data(),
                                     one.segments.size() * sizeof(SkeletonSegment)) != 0;
    REQUIRE(differs);
}

TEST_CASE("Connectivity: one root, every parent earlier in the list", "[trees][skeleton]") {
    // The invariant the pipe model's single backward pass depends on. If it ever breaks, radii go
    // silently wrong rather than loudly wrong, so it is pinned here.
    for (const TreeSpecies s : kAll) {
        const TreeSkeleton tree = grown(s);
        INFO("species " << static_cast<int>(s));
        int roots = 0;
        for (std::size_t i = 0; i < tree.segments.size(); ++i) {
            const SkeletonSegment& seg = tree.segments[i];
            if (seg.parent < 0) {
                ++roots;
            } else {
                REQUIRE(static_cast<std::size_t>(seg.parent) < i);
            }
        }
        REQUIRE(roots == 1);
    }
}

TEST_CASE("Segments are connected end to start and are one step long", "[trees][skeleton]") {
    for (const TreeSpecies s : kAll) {
        const SpeciesParams params = species_params(s);
        const TreeSkeleton tree = grown(s);
        for (const SkeletonSegment& seg : tree.segments) {
            REQUIRE_THAT(glm::length(seg.end - seg.start),
                         Catch::Matchers::WithinRel(params.segment_length, 1.0e-4f));
            if (seg.parent >= 0) {
                const glm::vec3 parentEnd = tree.segments[static_cast<std::size_t>(seg.parent)].end;
                REQUIRE(glm::length(seg.start - parentEnd) < 1.0e-4f);
            }
        }
    }
}

TEST_CASE("The skeleton fits the crown volume it was given", "[trees][skeleton]") {
    for (const TreeSpecies s : kAll) {
        const SpeciesParams params = species_params(s);
        const TreeSkeleton tree = grown(s);
        const TreeBounds b = tree.bounds();
        INFO("species " << static_cast<int>(s));
        // Never below the base, and never wider or taller than the crown plus one growth step
        // (a tip may overshoot the last attractor by up to one segment).
        const float slack = params.segment_length + 0.01f;
        REQUIRE(b.min.y >= -slack);
        REQUIRE(b.max.y <= params.crown_centre_height + 0.5f * params.crown_height + slack);
        REQUIRE(std::max(std::abs(b.min.x), b.max.x) <= params.crown_radius + slack);
        REQUIRE(std::max(std::abs(b.min.z), b.max.z) <= params.crown_radius + slack);
    }
}

TEST_CASE("Trunked species grow a bare stem before they branch", "[trees][skeleton]") {
    // The reason the stem is grown separately: colonizing from the ground up branches at ground
    // level and the tree has no trunk at all.
    for (const TreeSpecies s : {TreeSpecies::RoundBroadleaf, TreeSpecies::Conifer, TreeSpecies::Aspen}) {
        const SpeciesParams params = species_params(s);
        const TreeSkeleton tree = grown(s);
        INFO("species " << static_cast<int>(s));
        // Below half the trunk height there is exactly one segment per level -- no forks.
        int belowTrunk = 0;
        for (const SkeletonSegment& seg : tree.segments) {
            if (seg.end.y < params.trunk_height * 0.5f) {
                ++belowTrunk;
                REQUIRE_THAT(seg.start.x, Catch::Matchers::WithinAbs(0.0, 1.0e-5));
                REQUIRE_THAT(seg.start.z, Catch::Matchers::WithinAbs(0.0, 1.0e-5));
            }
        }
        REQUIRE(belowTrunk > 0);
    }
}

TEST_CASE("Branch tips are spread out, not piled up", "[trees][skeleton]") {
    // The property space colonization is FOR: the kill radius guarantees no two tips end up on top
    // of each other, which is what stops a crown collapsing into a single spike.
    const SpeciesParams params = species_params(TreeSpecies::RoundBroadleaf);
    const TreeSkeleton tree = grown(TreeSpecies::RoundBroadleaf);
    const std::vector<std::size_t> tips = leaf_segments(tree);
    REQUIRE(tips.size() > 10);
    int tooClose = 0;
    for (std::size_t i = 0; i < tips.size(); ++i) {
        for (std::size_t j = i + 1; j < tips.size(); ++j) {
            if (glm::length(tree.segments[tips[i]].end - tree.segments[tips[j]].end) <
                params.kill_radius * 0.25f) {
                ++tooClose;
            }
        }
    }
    // A few coincidences are fine (two tips can converge on the same last attractor); a pile is not.
    REQUIRE(tooClose < static_cast<int>(tips.size()) / 4);
}

TEST_CASE("Pipe model: a hand-built skeleton gets exactly the expected radii", "[trees][pipe]") {
    // Goal 187's check. A root with two children, each a tip: the root must carry both leaves, and
    // its cross-sectional AREA must be the sum of theirs -- that is the whole model.
    TreeSkeleton tree;
    tree.segments.push_back(SkeletonSegment{-1, {0, 0, 0}, {0, 1, 0}, 0.0f, 0.0f});
    tree.segments.push_back(SkeletonSegment{0, {0, 1, 0}, {1, 2, 0}, 0.0f, 0.0f});
    tree.segments.push_back(SkeletonSegment{0, {0, 1, 0}, {-1, 2, 0}, 0.0f, 0.0f});

    // min_radius off, so this tests the MODEL rather than the twig floor. The floor gets its own
    // assertion at the end -- keeping both in one case is how the first version of this test ended
    // up reporting a trunk exactly as thick as a twig and looking like a model bug.
    PipeModelParams params;
    params.min_radius = 0.0f;
    apply_pipe_model(tree, params);

    // Both tips share the crown's leaf area equally, and the root carries the sum -- which is the
    // only structural claim the pipe model makes.
    const float expectedPerTip = params.leaf_area_index * crown_footprint(tree) / 2.0f;
    REQUIRE_THAT(tree.segments[1].leaf_count, Catch::Matchers::WithinRel(expectedPerTip, 1e-4f));
    REQUIRE_THAT(tree.segments[2].leaf_count, Catch::Matchers::WithinRel(expectedPerTip, 1e-4f));
    REQUIRE_THAT(tree.segments[0].leaf_count, Catch::Matchers::WithinRel(2.0f * expectedPerTip, 1e-4f));

    const float tipArea = 3.14159265f * tree.segments[1].radius * tree.segments[1].radius;
    const float rootArea = 3.14159265f * tree.segments[0].radius * tree.segments[0].radius;
    REQUIRE_THAT(rootArea, Catch::Matchers::WithinRel(2.0f * tipArea, 1.0e-4f));
    // The exact radius the model asks for, not just the ratio.
    REQUIRE_THAT(tree.segments[1].radius,
                 Catch::Matchers::WithinRel(
                     std::sqrt(params.area_per_leaf_unit * expectedPerTip / 3.14159265f), 1e-4f));

    // And the floor, separately: with it on, nothing in this tiny tree is thinner than a twig.
    PipeModelParams floored;
    apply_pipe_model(tree, floored);
    for (const SkeletonSegment& seg : tree.segments) {
        REQUIRE(seg.radius >= floored.min_radius);
    }
}

TEST_CASE("Branch junctions satisfy da Vinci's rule on grown trees", "[trees][pipe]") {
    // Goal 187's second check, over real skeletons: a parent's cross-section is the sum of its
    // children's, within 15%. It holds by construction here (area is linear in leaf count, and a
    // parent carries the sum), so what this really guards is the min_radius floor not swamping the
    // rule on twigs.
    for (int seed = 1; seed <= 6; ++seed) {
        TreeSkeleton tree = grow_skeleton(seed, {0, 0, 0}, species_params(TreeSpecies::RoundBroadleaf));
        apply_pipe_model(tree);

        constexpr float floorRadius = PipeModelParams{}.min_radius + 1.0e-4f;
        std::map<std::size_t, float> childArea;
        for (const SkeletonSegment& s : tree.segments) {
            if (s.parent >= 0) {
                childArea[static_cast<std::size_t>(s.parent)] += s.radius * s.radius;
            }
        }
        // Junctions where the floor clamped ANY participant are excluded, parent or child: a twig
        // held at min_radius is deliberately thicker than the model says, and summing a clamped
        // child into an unclamped parent is comparing two different things.
        std::map<std::size_t, bool> clamped;
        for (const SkeletonSegment& s : tree.segments) {
            if (s.parent >= 0 && s.radius <= floorRadius) {
                clamped[static_cast<std::size_t>(s.parent)] = true;
            }
        }
        int checked = 0;
        for (const auto& [index, area] : childArea) {
            const float parentArea = tree.segments[index].radius * tree.segments[index].radius;
            if (tree.segments[index].radius > floorRadius && !clamped[index]) {
                REQUIRE(std::abs(parentArea - area) <= 0.15f * parentArea);
                ++checked;
            }
        }
        REQUIRE(checked > 5); // the test is not vacuous
    }
}

TEST_CASE("Leaf area is in the LAI band, and denser at the tips", "[trees][leaves]") {
    // Goal 188's check. LAI (leaf area index) is leaf area over ground footprint; the research's
    // oak numbers put a real broadleaf in the 0.5-3.0 band, and a tree an order of magnitude off
    // that would make every downstream density wrong.
    // Across SEEDS, not one lucky one: tip count varies with how the colonization happens to fill
    // the crown, so a single-seed assertion says nothing about the species.
    const SpeciesParams params = species_params(TreeSpecies::RoundBroadleaf);
    const float footprint = 3.14159265f * params.crown_radius * params.crown_radius;
    const PipeModelParams pipe;
    REQUIRE(pipe.leaf_area_index > 0.5f); // the species sits inside the band real canopies occupy
    REQUIRE(pipe.leaf_area_index < 3.0f);
    for (int seed = 1; seed <= 12; ++seed) {
        TreeSkeleton t = grow_skeleton(seed, {0, 0, 0}, species_params(TreeSpecies::RoundBroadleaf));
        apply_pipe_model(t);
        // LAI is now exact BY CONSTRUCTION against the crown the tree actually grew, whatever the
        // tip count did -- which is the property the old per-tip formulation could not give
        // (34 tips at one seed, 245 at another, so LAI ranged 0.3 to 8.7).
        const float lai = total_leaf_area(t) / crown_footprint(t);
        INFO("seed " << seed << " LAI = " << lai << " over " << leaf_segments(t).size() << " tips");
        REQUIRE_THAT(lai, Catch::Matchers::WithinRel(pipe.leaf_area_index, 1.0e-3f));
        // And the crown it grew is close to the crown it was asked for.
        REQUIRE(crown_footprint(t) > 0.3f * footprint);
        REQUIRE(crown_footprint(t) < 1.6f * footprint);
    }

    TreeSkeleton tree = grow_skeleton(7, {0, 0, 0}, species_params(TreeSpecies::RoundBroadleaf));
    apply_pipe_model(tree);

    // Distribution shape: the upper half of the crown carries more leaf than the lower half, which
    // is what "denser at the tips" means for a dome.
    float upper = 0.0f;
    float lower = 0.0f;
    for (const std::size_t tip : leaf_segments(tree)) {
        (tree.segments[tip].end.y > params.crown_centre_height ? upper : lower) +=
            tree.segments[tip].leaf_count;
    }
    REQUIRE(upper > lower * 0.8f); // not lopsided downward
    REQUIRE(upper + lower > 0.0f);
}

TEST_CASE("Radii taper from root to tip", "[trees][pipe]") {
    TreeSkeleton tree = grow_skeleton(11, {0, 0, 0}, species_params(TreeSpecies::Conifer));
    apply_pipe_model(tree);
    // The trunk is the thickest thing in the tree, and no child is thicker than its parent.
    float thickest = 0.0f;
    for (const SkeletonSegment& s : tree.segments) {
        thickest = std::max(thickest, s.radius);
    }
    REQUIRE_THAT(tree.segments.front().radius, Catch::Matchers::WithinRel(thickest, 1.0e-5f));
    for (const SkeletonSegment& s : tree.segments) {
        if (s.parent >= 0) {
            REQUIRE(s.radius <= tree.segments[static_cast<std::size_t>(s.parent)].radius + 1.0e-6f);
        }
    }
}

TEST_CASE("A v1 placement grows a v2 skeleton in the space it occupied", "[trees][skeleton]") {
    // What makes v2 adoptable: the world's tree DISTRIBUTION is unchanged, only the geometry.
    TreePlacement placement;
    placement.world_x = 12.0f;
    placement.world_z = -30.0f;
    placement.base_height = 8.0f;
    placement.trunk_height = 4.0f;
    placement.canopy_radius = 2.5f;
    placement.shape = TreeShape::Round;

    const TreeSkeleton tree = grow_skeleton_for(1337, placement);
    REQUIRE_FALSE(tree.empty());
    const TreeBounds b = tree.bounds();
    REQUIRE(b.min.y >= placement.base_height - 0.5f);
    REQUIRE(std::abs(b.min.x - placement.world_x) < placement.canopy_radius + 1.0f);
    REQUIRE(std::abs(b.max.z - placement.world_z) < placement.canopy_radius + 1.0f);
    // And it is deterministic per placement, like everything else in the generator.
    const TreeSkeleton again = grow_skeleton_for(1337, placement);
    REQUIRE(std::memcmp(tree.segments.data(), again.segments.data(),
                        tree.segments.size() * sizeof(SkeletonSegment)) == 0);
}

TEST_CASE("Species selection is defined at world coordinates, not just near the origin",
          "[trees][skeleton]") {
    // UBSan caught signed integer overflow in the species hash: the spatial-hash constants exceed
    // int32 for any coordinate past about 29, so a tree at x = 12 was already undefined behaviour.
    // MSVC has no UBSan, so nothing local could see it -- only CI. This case walks coordinates far
    // enough out that a signed formulation must overflow, so a regression is caught there again.
    for (float x = -40000.0f; x <= 40000.0f; x += 1237.0f) {
        for (float z = -40000.0f; z <= 40000.0f; z += 3719.0f) {
            TreePlacement p;
            p.world_x = x;
            p.world_z = z;
            p.shape = TreeShape::Round;
            const TreeSpecies s = species_of(p);
            REQUIRE((s == TreeSpecies::RoundBroadleaf || s == TreeSpecies::Aspen));
        }
    }
    // The non-round shapes are not hashed at all -- they map straight through.
    TreePlacement conifer;
    conifer.shape = TreeShape::Conifer;
    REQUIRE(species_of(conifer) == TreeSpecies::Conifer);
    TreePlacement shrub;
    shrub.shape = TreeShape::Shrub;
    REQUIRE(species_of(shrub) == TreeSpecies::Shrub);
}
