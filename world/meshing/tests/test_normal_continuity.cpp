#include <array>
#include <cstdio>

#include <catch2/catch_test_macros.hpp>

#include "world/chunk/chunk_coord.hpp"
#include "world/chunk/chunk_store.hpp"
#include "world/generation/heightmap_generator.hpp"
#include "world/generation/terrain_fill.hpp"
#include "world/meshing/mesh_extractor.hpp"

// TERRAIN_FIXES_BRIEF Group R task 10's original premise -- catching a normal-AVERAGING bug that
// zigzagged adjacent Surface-Nets normals by tens of degrees -- no longer applies: Group Q's blocky
// meshing (research/voxel-representation-redesign.md SS2) never averages a normal across multiple
// faces at all, so real terrain SHOULD show large-angle jumps at every visible edge (a flat top face
// meeting a vertical side face is a full 90 degrees, and correctly so) -- the old "mean adjacent
// angle < 10 degrees" assertion would now fail on CORRECT blocky output, not catch a bug. The
// meaningful invariant for this algorithm instead: every emitted normal is an EXACT unit cardinal
// direction, never an interpolated or off-axis vector -- if that ever stopped being true, it would
// mean a real bug in emit_quad's normal assignment (e.g. a stray cross-product-then-normalize path
// creeping back in), not smooth-shading corrugation.
TEST_CASE("every face normal on real terrain is an exact cardinal direction", "[meshing][normals]") {
    const world::generation::HeightmapGenerator generator(1337);
    world::chunk::ChunkStore store;
    const world::chunk::ChunkCoord center{0, 0, 0}; // y=0 crosses the surface near the origin
    for (std::int32_t dz = -1; dz <= 1; ++dz) {
        for (std::int32_t dy = -1; dy <= 1; ++dy) {
            for (std::int32_t dx = -1; dx <= 1; ++dx) {
                world::generation::fill_terrain(
                    store.get_or_create({center.x + dx, center.y + dy, center.z + dz}), generator);
            }
        }
    }
    const auto mesh = world::meshing::extract_mesh(store, center);
    REQUIRE(mesh.indices.size() >= 3);

    constexpr std::array<glm::vec3, 6> kCardinal = {glm::vec3{1, 0, 0}, glm::vec3{-1, 0, 0},
                                                    glm::vec3{0, 1, 0}, glm::vec3{0, -1, 0},
                                                    glm::vec3{0, 0, 1}, glm::vec3{0, 0, -1}};
    std::size_t offAxis = 0;
    for (const auto& v : mesh.vertices) {
        bool matched = false;
        for (const glm::vec3& c : kCardinal) {
            if (v.normal == c) {
                matched = true;
                break;
            }
        }
        if (!matched) {
            if (offAxis < 5) {
                std::printf("off-axis normal (%.3f,%.3f,%.3f) at (%.1f,%.1f,%.1f)\n",
                            static_cast<double>(v.normal.x), static_cast<double>(v.normal.y),
                            static_cast<double>(v.normal.z), static_cast<double>(v.position.x),
                            static_cast<double>(v.position.y), static_cast<double>(v.position.z));
            }
            ++offAxis;
        }
    }
    CHECK(offAxis == 0);
}
