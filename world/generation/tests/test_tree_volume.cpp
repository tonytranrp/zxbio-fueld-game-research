// Prompt 007 goal 336 = docs/goals.md goal 191. The three predicates a `TerrainSampler` asks a tree
// are not symmetric, and the asymmetry is the whole safety property:
//
//   intersects_box  may say YES too often   -- a false yes costs a subdivision
//                   must never say NO wrongly -- a false no is a HOLE in the tree
//   contains_box    may say NO too often    -- a false no costs a subdivision
//                   must never say YES wrongly -- a false yes fills a box with leaves
//
// A volume that got either backwards would not fail loudly; it would produce a tree with bites
// missing or blocks of floating canopy, at some seeds, at some resolutions. So both directions are
// tested against `material_at` as the oracle, on thousands of random boxes, rather than on a
// hand-picked few.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>
#include <random>

#include "world/generation/heightmap_generator.hpp"
#include "world/generation/tree_placement.hpp"
#include "world/generation/tree_volume.hpp"

using namespace world::generation;
using world::chunk::MaterialID;

namespace {

[[nodiscard]] TreeVolume grown(int seed = 1337) {
    TreeSkeleton skeleton = grow_skeleton(seed, glm::vec3(0.0f), species_params(TreeSpecies::RoundBroadleaf));
    apply_pipe_model(skeleton);
    return TreeVolume{skeleton, TreeVolumeParams{}};
}

} // namespace

TEST_CASE("a grown volume has branches and leaf clouds and a bound that holds them", "[tree][volume]") {
    const TreeVolume volume = grown();
    REQUIRE_FALSE(volume.empty());

    std::size_t wood = 0;
    std::size_t leaves = 0;
    for (const TreePrimitive& p : volume.primitives()) {
        (p.leaf ? leaves : wood) += 1u;
        // Every primitive inside the stated bounds, or the sampler's AABB rejection would cut a
        // branch off and nothing would say so.
        const glm::vec3 lo = glm::min(p.a, p.b) - glm::vec3{p.radius};
        const glm::vec3 hi = glm::max(p.a, p.b) + glm::vec3{p.radius};
        REQUIRE(lo.x >= volume.bounds().min.x - 1.0e-4f);
        REQUIRE(hi.x <= volume.bounds().max.x + 1.0e-4f);
        REQUIRE(lo.y >= volume.bounds().min.y - 1.0e-4f);
        REQUIRE(hi.y <= volume.bounds().max.y + 1.0e-4f);
    }
    INFO(wood << " branch capsules, " << leaves << " leaf clouds, " << volume.memory_bytes() << " bytes");
    CHECK(wood > 50);
    CHECK(leaves > 10);
    // Two clouds per leaf-bearing segment, so the count is even by construction.
    CHECK(leaves % 2 == 0);
}

TEST_CASE("intersects_box never misses a box the volume actually fills", "[tree][volume]") {
    // The dangerous direction. `material_at` is the oracle: if any sample inside the box is solid,
    // `intersects_box` MUST be true.
    const TreeVolume volume = grown();
    const TreeBounds b = volume.bounds();
    std::mt19937 rng(20260908u);
    std::uniform_real_distribution<float> ux(b.min.x - 0.5f, b.max.x + 0.5f);
    std::uniform_real_distribution<float> uy(b.min.y - 0.5f, b.max.y + 0.5f);
    std::uniform_real_distribution<float> uz(b.min.z - 0.5f, b.max.z + 0.5f);
    std::uniform_real_distribution<float> usize(0.02f, 0.9f);

    int checked = 0;
    int solidFound = 0;
    for (int i = 0; i < 4000; ++i) {
        const glm::vec3 lo{ux(rng), uy(rng), uz(rng)};
        const glm::vec3 hi = lo + glm::vec3{usize(rng), usize(rng), usize(rng)};
        // Sample a 4x4x4 lattice inside the box. Not proof, but 64 witnesses per box over 4,000
        // boxes is a quarter of a million chances to catch a false negative.
        bool anySolid = false;
        for (int k = 0; k < 64 && !anySolid; ++k) {
            const glm::vec3 t{static_cast<float>((k & 3) + 1) / 5.0f,
                              static_cast<float>(((k >> 2) & 3) + 1) / 5.0f,
                              static_cast<float>(((k >> 4) & 3) + 1) / 5.0f};
            anySolid = volume.material_at(lo + t * (hi - lo)) != MaterialID::Air;
        }
        ++checked;
        if (anySolid) {
            ++solidFound;
            REQUIRE(volume.intersects_box(lo, hi));
        }
    }
    INFO(solidFound << " of " << checked << " random boxes contained solid; every one was reported");
    CHECK(solidFound > 100); // and the test actually exercised the case
}

TEST_CASE("contains_box is never a false positive", "[tree][volume]") {
    // The other dangerous direction: a box claimed full must have no air in it.
    const TreeVolume volume = grown();
    const TreeBounds b = volume.bounds();
    std::mt19937 rng(777u);
    std::uniform_real_distribution<float> ux(b.min.x, b.max.x);
    std::uniform_real_distribution<float> uy(b.min.y, b.max.y);
    std::uniform_real_distribution<float> uz(b.min.z, b.max.z);
    std::uniform_real_distribution<float> usize(0.01f, 0.35f);

    int full = 0;
    for (int i = 0; i < 4000; ++i) {
        const glm::vec3 lo{ux(rng), uy(rng), uz(rng)};
        const glm::vec3 hi = lo + glm::vec3{usize(rng), usize(rng), usize(rng)};
        if (!volume.contains_box(lo, hi)) {
            continue;
        }
        ++full;
        for (int k = 0; k < 64; ++k) {
            const glm::vec3 t{static_cast<float>(k & 3) / 3.0f, static_cast<float>((k >> 2) & 3) / 3.0f,
                              static_cast<float>((k >> 4) & 3) / 3.0f};
            REQUIRE(volume.material_at(lo + t * (hi - lo)) != MaterialID::Air);
        }
    }
    INFO(full << " of 4000 random boxes were reported full; none contained air");
    CHECK(full > 20);
}

TEST_CASE("wood wins over leaves, as it does in the implicit shape", "[tree][volume]") {
    // The trunk is inside the crown, so the two overlap and the rule has to be stated. It is the
    // same rule `tree_material_at` uses, which is what keeps a near tree and a far tree consistent.
    const TreeVolume volume = grown();
    const TreePrimitive* trunk = nullptr;
    for (const TreePrimitive& p : volume.primitives()) {
        if (!p.leaf && (trunk == nullptr || p.radius > trunk->radius)) {
            trunk = &p;
        }
    }
    REQUIRE(trunk != nullptr);
    CHECK(volume.material_at(0.5f * (trunk->a + trunk->b)) == MaterialID::Wood);
}

TEST_CASE("the volume is deterministic in the placement it came from", "[tree][volume]") {
    world::generation::HeightmapGenerator heightmap(1337);
    const std::vector<TreePlacement> placements = compute_tree_placements(1, 1, 1337, heightmap);
    REQUIRE_FALSE(placements.empty());

    const TreeVolume a = tree_volume_for(1337, placements.front());
    const TreeVolume b = tree_volume_for(1337, placements.front());
    REQUIRE(a.primitives().size() == b.primitives().size());
    for (std::size_t i = 0; i < a.primitives().size(); ++i) {
        REQUIRE(a.primitives()[i].radius == b.primitives()[i].radius);
        REQUIRE(a.primitives()[i].a.x == b.primitives()[i].a.x);
        REQUIRE(a.primitives()[i].leaf == b.primitives()[i].leaf);
    }
}

TEST_CASE("the crown fills the crown, which is why the density is derived", "[tree][volume]") {
    // The bug a capture found. With a FIXED leaf area density of 2.0 the clouds occupied about a
    // fifth of the crown volume and rendered as separate spheres on a stick. Deriving the density
    // from `total leaf area / crown volume` makes the clouds fill what they are in.
    //
    // Asserted as an occupancy fraction rather than as a density, because that is the thing that was
    // wrong: the summed cloud volume must be a real fraction of the crown's own volume. Overlap
    // means it can exceed 1, which is exactly what makes a canopy read as a mass.
    const TreeVolume volume = grown();
    glm::vec3 lo{std::numeric_limits<float>::max()};
    glm::vec3 hi{std::numeric_limits<float>::lowest()};
    double cloudVolume = 0.0;
    for (const TreePrimitive& p : volume.primitives()) {
        if (!p.leaf) {
            continue;
        }
        lo = glm::min(lo, p.a);
        hi = glm::max(hi, p.a);
        const double r = p.radius;
        cloudVolume += 4.0 / 3.0 * 3.14159265358979 * r * r * r;
    }
    const glm::vec3 half = 0.5f * (hi - lo);
    const double crown = 4.0 / 3.0 * 3.14159265358979 * half.x * half.y * half.z;
    const double occupancy = cloudVolume / crown;
    INFO("leaf clouds occupy " << occupancy << "x the crown ellipsoid's volume");
    CHECK(occupancy > 0.5);
    CHECK(occupancy < 4.0);
}
