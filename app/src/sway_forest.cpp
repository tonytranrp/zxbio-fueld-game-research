#include "sway_forest.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

#include "world/chunk/chunk.hpp"
#include "world/generation/tree_placement.hpp"

namespace app {

namespace {

using world::generation::TreePlacement;

constexpr float kChunkSpan = static_cast<float>(world::chunk::kChunkSize);

} // namespace

void SwayForest::refresh(glm::vec3 camera) {
    if (!options_.enabled) {
        trees_.clear();
        return;
    }
    // Hysteresis, for the same reason goal 249 gave the octree rebuild one: the set is expensive to
    // build and almost identical after a step.
    if (built_ &&
        glm::length(glm::vec2(camera.x - builtAt_.x, camera.z - builtAt_.z)) < 0.25f * options_.radius_m) {
        return;
    }

    const auto start = std::chrono::steady_clock::now();

    const auto cx = static_cast<std::int32_t>(std::floor(camera.x / kChunkSpan));
    const auto cz = static_cast<std::int32_t>(std::floor(camera.z / kChunkSpan));
    const auto reach = static_cast<std::int32_t>(std::ceil(options_.radius_m / kChunkSpan)) + 1;

    // Gather nearest-first so `max_trees` drops the least visible rather than an arbitrary corner of
    // the ring.
    struct Candidate {
        TreePlacement tree;
        float distanceSq = 0.0f;
    };
    std::vector<Candidate> candidates;
    const float radiusSq = options_.radius_m * options_.radius_m;
    for (std::int32_t dz = -reach; dz <= reach; ++dz) {
        for (std::int32_t dx = -reach; dx <= reach; ++dx) {
            for (const TreePlacement& t :
                 world::generation::compute_tree_placements(cx + dx, cz + dz, seed_, *heightmap_)) {
                const float ddx = t.world_x - camera.x;
                const float ddz = t.world_z - camera.z;
                const float d2 = ddx * ddx + ddz * ddz;
                if (d2 <= radiusSq) {
                    candidates.push_back({t, d2});
                }
            }
        }
    }
    if (candidates.size() > options_.max_trees) {
        std::nth_element(candidates.begin(),
                         candidates.begin() + static_cast<std::ptrdiff_t>(options_.max_trees),
                         candidates.end(),
                         [](const Candidate& a, const Candidate& b) { return a.distanceSq < b.distanceSq; });
        candidates.resize(options_.max_trees);
    }

    trees_.clear();
    trees_.reserve(candidates.size());
    segmentTotal_ = 0;
    chainTotal_ = 0;
    for (const Candidate& c : candidates) {
        Tree tree;
        tree.skeleton = world::generation::grow_skeleton_for(seed_, c.tree);
        if (tree.skeleton.empty()) {
            continue;
        }
        world::generation::apply_pipe_model(tree.skeleton);
        const world::generation::SwayParams params = world::generation::sway_params_for(
            world::generation::species_params(world::generation::species_of(c.tree)));
        tree.state = world::generation::make_sway_state(tree.skeleton, params);
        tree.base = tree.skeleton.segments.front().start;
        const world::generation::TreeBounds b = tree.skeleton.bounds();
        tree.height = b.max.y - b.min.y;
        segmentTotal_ += tree.skeleton.segments.size();
        chainTotal_ += tree.state.size();
        trees_.push_back(std::move(tree));
    }

    builtAt_ = camera;
    built_ = true;
    growSeconds_ = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

void SwayForest::step(const world::wind::WindParams& wind, float windTime, float dt, glm::vec3 camera) {
    steppedChains_ = 0;
    if (!options_.enabled) {
        return;
    }
    for (Tree& tree : trees_) {
        const float dx = tree.base.x - camera.x;
        const float dz = tree.base.z - camera.z;
        const float distance = std::sqrt(dx * dx + dz * dz);
        const std::size_t budget =
            world::generation::sway_joint_budget(tree.state.size(), distance, tree.height);
        if (budget == 0) {
            continue;
        }
        world::generation::step_sway_partial(tree.state, wind, windTime, dt, budget);
        steppedChains_ += budget;
    }
}

} // namespace app
