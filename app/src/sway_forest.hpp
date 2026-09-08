#pragma once

// The in-ring trees that are actually being swayed (Prompt 007 goal 335 = goal 190).
//
// Goal 190's Check is a FRAME BUDGET -- "<= 0.5 ms/frame for all in-ring trees, measured with the
// attributor" -- and a budget cannot be measured against a model that no frame runs. This is the
// thing the frame runs: it enumerates the tree placements around the camera, grows a skeleton for
// each once, and steps them every frame with the per-tree chain budget
// `world::generation::sway_joint_budget` hands it.
//
// WHY IT ENUMERATES PLACEMENTS RATHER THAN READING THE OCTREE'S TREE LIST. `TerrainSampler` already
// has the trees it voxelized, but it lives on the build thread inside a job and its list is a
// build's worth of state, rebuilt whenever the world is. `compute_tree_placements` is the same
// deterministic function the sampler itself calls, so asking it directly gives the same trees
// without reaching across a thread boundary or pinning a build's lifetime to a frame's.
//
// WHAT IT DOES NOT DO YET: nothing draws these. Goal 336 replaces the implicit box+octahedron
// voxelization with the skeleton, and goal 337 is what carries the motion to the marcher. Until
// then this is the dynamics running, measured, and viewable through `tree_dump --sway-frames`.
// That ordering is the prompt's (335 -> 336 -> 337), and it is the right one: a sway model whose
// cost is unknown is not something to build a voxelizer on top of.

#include <cstddef>
#include <cstdint>
#include <vector>

#include "engine/core/math.hpp"
#include "world/generation/heightmap_generator.hpp"
#include "world/generation/tree_skeleton.hpp"
#include "world/generation/tree_sway.hpp"
#include "world/wind/wind_field.hpp"

namespace app {

struct SwayForestOptions {
    // How far out trees are simulated at all. NOT a look-and-feel number: a 20 m tree's sway is
    // resolvable to about a kilometre, so the bound that matters is cost, and this is where the
    // count is capped. 120 m is roughly the distance at which a tree is 6 px of crown.
    float radius_m = 120.0f;
    // A hard ceiling on how many skeletons are grown, so a dense forest cannot turn world-ready into
    // a stall. Trees are taken nearest-first, so the ones dropped are the ones already least visible.
    std::size_t max_trees = 512;
    bool enabled = true;
};

class SwayForest {
public:
    SwayForest(const world::generation::HeightmapGenerator& heightmap, int seed,
               const SwayForestOptions& options)
        : heightmap_(&heightmap), seed_(seed), options_(options) {}

    // Grow (or re-grow) the resident set around `camera`. Cheap and a no-op while the camera stays
    // within a quarter of the radius of where the set was last built -- growing 500 skeletons is not
    // a per-frame cost, and re-growing them because the player took a step would be the rebuild
    // storm Prompt 004 spent a group removing, in miniature.
    void refresh(glm::vec3 camera);

    // One fixed tick over every resident tree. Returns nothing; the cost is what the caller times.
    void step(const world::wind::WindParams& wind, float windTime, float dt, glm::vec3 camera);

    [[nodiscard]] std::size_t tree_count() const noexcept { return trees_.size(); }
    [[nodiscard]] std::size_t segment_total() const noexcept { return segmentTotal_; }
    [[nodiscard]] std::size_t chain_total() const noexcept { return chainTotal_; }
    // Chains actually integrated on the last `step`, after the per-tree LOD budget. This is the
    // number the frame cost is proportional to, and it is a long way below `chain_total`.
    [[nodiscard]] std::size_t stepped_chains() const noexcept { return steppedChains_; }
    [[nodiscard]] double grow_seconds() const noexcept { return growSeconds_; }

    struct Tree {
        world::generation::TreeSkeleton skeleton;
        world::generation::SwayState state;
        glm::vec3 base{0.0f};
        float height = 0.0f;
    };
    [[nodiscard]] const std::vector<Tree>& trees() const noexcept { return trees_; }

private:
    const world::generation::HeightmapGenerator* heightmap_;
    int seed_;
    SwayForestOptions options_;

    std::vector<Tree> trees_;
    glm::vec3 builtAt_{0.0f};
    bool built_ = false;
    std::size_t segmentTotal_ = 0;
    std::size_t chainTotal_ = 0;
    std::size_t steppedChains_ = 0;
    double growSeconds_ = 0.0;
};

} // namespace app
