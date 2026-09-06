#pragma once

// Module-internal + test-visible template body of world::svo::build_tree (tree_builder.hpp).
// Production code reaches it only through the explicit TerrainSampler instantiation in
// src/tree_builder.cpp; tests include this header directly to build trees over their own analytic
// samplers.

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstdint>
#include <future>
#include <stdexcept>
#include <utility>
#include <vector>

#include "world/svo/tree_builder.hpp"

namespace world::svo::detail {

// What a built node reports upward (Group Z): its area-weighted exposed-face normal sum (world
// units squared, so coarse and fine children mix by real surface area) and the fraction of its
// volume that is occupied. The parent packs its own summary into its attribute word
// (tree_layout.hpp) and keeps accumulating.
struct NodeSummary {
    glm::vec3 normal_sum{0.0f};
    float coverage = 0.0f;
};

struct SubtreeOutput {
    std::vector<std::uint32_t> nodes;
    std::vector<std::uint32_t> bricks;
    std::uint32_t root = kNoNode;
    NodeSummary summary; // of `root`, for the parent that splices this subtree in
    BuildStats stats;
};

// Shifts every child pointer / brick index inside `out` (from its root down) by the given bases so
// it can be appended verbatim to a larger array.
inline void relocate_subtree(SubtreeOutput& out, std::uint32_t nodeOffset, std::uint32_t nodeBase,
                             std::uint32_t brickBase) {
    std::vector<std::uint32_t> stack;
    stack.push_back(nodeOffset);
    while (!stack.empty()) {
        const std::uint32_t at = stack.back();
        stack.pop_back();
        const std::uint32_t header = out.nodes[at];
        const std::uint32_t kind = node_kind(header);
        if (kind == kNodeKindBrick) {
            out.nodes[at + kNodeBrickIndexSlot] += brickBase;
        } else if (kind == kNodeKindInternal) {
            const std::uint32_t mask = node_child_mask(header);
            for (int octant = 0; octant < 8; ++octant) {
                if ((mask & (1u << octant)) != 0u) {
                    std::uint32_t& slot = out.nodes[at + node_child_slot(header, octant)];
                    stack.push_back(slot);
                    slot += nodeBase;
                }
            }
        }
    }
}

// "Seen from above" representative for an internal node: majority among the present TOP octants'
// representatives, falling back to the bottom octants. Never Air for a node with any child.
inline world::chunk::MaterialID choose_representative(const std::array<world::chunk::MaterialID, 8>& reps,
                                                      std::uint32_t mask) {
    using world::chunk::MaterialID;
    const auto vote = [&](std::uint32_t octantMask) {
        std::array<int, 256> counts{};
        int bestCount = 0;
        MaterialID best = MaterialID::Air;
        for (int octant = 0; octant < 8; ++octant) {
            if ((mask & octantMask & (1u << octant)) == 0u ||
                reps[static_cast<std::size_t>(octant)] == MaterialID::Air) {
                continue;
            }
            const auto m = static_cast<std::size_t>(reps[static_cast<std::size_t>(octant)]);
            if (++counts[m] > bestCount) {
                bestCount = counts[m];
                best = static_cast<MaterialID>(m);
            }
        }
        return best;
    };
    constexpr std::uint32_t kTopOctants = 0b11001100u; // bit1 (y) set: octants 2,3,6,7
    // Water is by construction the top of every column below sea level, so a node whose top
    // octants hold any water contains the water SURFACE -- what a viewer above sees. Without this
    // a shore node's top octants tie between water and the seabed rising through them, and the
    // tie-break alternates per node: sand-colored checkerboards on the water at every LOD ring.
    for (int octant = 0; octant < 8; ++octant) {
        if ((mask & kTopOctants & (1u << octant)) != 0u &&
            reps[static_cast<std::size_t>(octant)] == MaterialID::Water) {
            return MaterialID::Water;
        }
    }
    const MaterialID top = vote(kTopOctants);
    return top != MaterialID::Air ? top : vote(~kTopOctants);
}

template <VoxelSampler S>
class Builder {
public:
    Builder(const S& sampler, const TreeGeometry& geometry, const BuildParams& params)
        : sampler_(sampler), geometry_(geometry), params_(params) {}

    // Builds the node for the level-`level` cell `cell` into `out`. `known` carries a
    // classification the parent already computed for this box (or Mixed-with-unknown = compute).
    // When `jobs` is non-null and `level == params.parallel_split_level`, the pre-built subtree for
    // this cell is spliced in instead of built. `summary` (optional) receives the node's normal
    // sum + coverage for the parent's attribute word.
    std::uint32_t build_node(int level, glm::ivec3 cell, SubtreeOutput& out, const BoxClassification* known,
                             std::vector<SubtreeOutput>* jobs, NodeSummary* summary) {
        const Box box = box_of(level, cell);
        BoxClassification cls;
        if (known != nullptr) {
            cls = *known;
        } else {
            cls = classify_timed(box, out);
        }
        if (cls.cls == BoxClass::Air) {
            return kNoNode;
        }
        if (cls.cls == BoxClass::Solid) {
            return emit_solid(out, cls.material, summary);
        }
        if (jobs != nullptr && level == params_.parallel_split_level) {
            return splice_job(cell, out, *jobs, summary);
        }
        if (is_leaf_level(level, box)) {
            return emit_brick(level, box, out, summary);
        }

        // Internal node: classify every child first so the exact number of slots to reserve is
        // known before any child appends to `out` (children are written AFTER their parent's slots,
        // and the SVDAG layout needs the pointers contiguous right behind the header). A child
        // classified Mixed can still turn out entirely absent after sampling (the classification is
        // conservative); its slot then stays unused -- the padding_words statistic counts those.
        std::array<BoxClassification, 8> childCls{};
        std::uint32_t nonAir = 0;
        for (int octant = 0; octant < 8; ++octant) {
            const glm::ivec3 childCell = child_cell(cell, octant);
            childCls[static_cast<std::size_t>(octant)] = classify_timed(box_of(level + 1, childCell), out);
            if (childCls[static_cast<std::size_t>(octant)].cls != BoxClass::Air) {
                ++nonAir;
            }
        }
        if (nonAir == 0) {
            return kNoNode;
        }
        const auto me = static_cast<std::uint32_t>(out.nodes.size());
        out.nodes.resize(out.nodes.size() + kNodeFirstChildSlot + nonAir, 0u);

        std::uint32_t mask = 0;
        std::uint32_t slot = 0;
        std::array<world::chunk::MaterialID, 8> reps{};
        std::array<NodeSummary, 8> childSummary{};
        for (int octant = 0; octant < 8; ++octant) {
            const BoxClassification& c = childCls[static_cast<std::size_t>(octant)];
            if (c.cls == BoxClass::Air) {
                continue;
            }
            const std::uint32_t child = build_node(level + 1, child_cell(cell, octant), out, &c, jobs,
                                                   &childSummary[static_cast<std::size_t>(octant)]);
            if (child == kNoNode) {
                continue;
            }
            mask |= 1u << octant;
            out.nodes[me + kNodeFirstChildSlot + slot] = child;
            ++slot;
            reps[static_cast<std::size_t>(octant)] = node_material(out.nodes[child]);
        }
        if (mask == 0u) {
            // Every child vanished after sampling; nothing was appended past our reservation
            // (a vanishing subtree appends nothing, recursively), so the reservation can be undone.
            out.nodes.resize(me);
            return kNoNode;
        }
        out.stats.padding_words += nonAir - static_cast<std::uint32_t>(std::popcount(mask));
        out.nodes[me] = make_node_header(kNodeKindInternal, mask, choose_representative(reps, mask));

        // Attributes: children's summaries plus the coarse exposure a brick cannot see -- a solid
        // child's face against an ABSENT (air) sibling is one whole exposed child face.
        NodeSummary mine;
        const float childEdge = geometry_.level_edge(level + 1);
        const float childFaceArea = childEdge * childEdge;
        for (int octant = 0; octant < 8; ++octant) {
            if ((mask & (1u << octant)) == 0u) {
                continue;
            }
            const NodeSummary& cs = childSummary[static_cast<std::size_t>(octant)];
            mine.normal_sum += cs.normal_sum;
            mine.coverage += cs.coverage * 0.125f;
            const std::uint32_t childHeader =
                out.nodes[out.nodes[me + node_child_slot(out.nodes[me], octant)]];
            if (node_kind(childHeader) != kNodeKindSolid) {
                continue;
            }
            for (int axis = 0; axis < 3; ++axis) {
                const int sibling = octant ^ (1 << axis);
                if ((mask & (1u << sibling)) == 0u) {
                    // The sibling in the +axis direction is absent when this child sits in the -half.
                    mine.normal_sum[axis] += ((octant >> axis) & 1) != 0 ? -childFaceArea : childFaceArea;
                }
            }
        }
        out.nodes[me + kNodeAttrSlotInternal] = pack_summary(mine);
        if (summary != nullptr) {
            *summary = mine;
        }
        return me;
    }

    [[nodiscard]] Box box_of(int level, glm::ivec3 cell) const noexcept {
        const float edge = geometry_.level_edge(level);
        const glm::vec3 min = geometry_.origin + glm::vec3{cell} * edge;
        return Box{min, min + glm::vec3{edge}};
    }

    [[nodiscard]] static glm::ivec3 child_cell(glm::ivec3 cell, int octant) noexcept {
        return glm::ivec3{cell.x * 2 + (octant & 1), cell.y * 2 + ((octant >> 1) & 1),
                          cell.z * 2 + ((octant >> 2) & 1)};
    }

private:
    [[nodiscard]] bool is_leaf_level(int level, const Box& box) const noexcept {
        if (level >= geometry_.max_brick_level()) {
            return true;
        }
        if (params_.uniform_lod) {
            return false;
        }
        const glm::vec3 nearest = glm::clamp(params_.lod_center, box.min, box.max);
        const float distance = glm::length(nearest - params_.lod_center);
        const float finest = geometry_.finest_voxel_edge();
        const float target = std::max(finest, distance * finest / std::max(params_.lod_radius, 1.0e-3f));
        return geometry_.level_voxel_edge(level) <= target;
    }

    [[nodiscard]] static std::uint32_t pack_summary(const NodeSummary& s) noexcept {
        const float length = glm::length(s.normal_sum);
        const glm::vec3 n = length > 1.0e-12f ? s.normal_sum / length : glm::vec3{0.0f};
        return make_node_attributes(n, s.coverage);
    }

    static std::uint32_t emit_solid(SubtreeOutput& out, world::chunk::MaterialID material,
                                    NodeSummary* summary) {
        const auto at = static_cast<std::uint32_t>(out.nodes.size());
        out.nodes.push_back(make_node_header(kNodeKindSolid, 0u, material));
        ++out.stats.solid_leaves;
        if (summary != nullptr) {
            *summary = NodeSummary{glm::vec3{0.0f}, 1.0f}; // exposure is the parent's to judge
        }
        return at;
    }

    BoxClassification classify_timed(const Box& box, SubtreeOutput& out) const {
        const auto start = std::chrono::steady_clock::now();
        const BoxClassification cls = sampler_.classify(box);
        out.stats.classify_seconds +=
            std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        ++out.stats.boxes_classified;
        return cls;
    }

    std::uint32_t emit_brick(int level, const Box& box, SubtreeOutput& out, NodeSummary* summary) {
        Brick brick;
        const auto start = std::chrono::steady_clock::now();
        const float voxelEdge = geometry_.level_voxel_edge(level);
        sampler_.fill_brick(box.min, voxelEdge, brick);
        out.stats.fill_seconds +=
            std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        ++out.stats.bricks_sampled;
        const auto lv = static_cast<std::size_t>(level);
        ++out.stats.sampled_per_level[lv];
        if (brick.empty()) {
            ++out.stats.empty_per_level[lv];
            return kNoNode;
        }
        if (brick.is_homogeneous()) {
            ++out.stats.solid_per_level[lv];
            return emit_solid(out, brick.at(std::size_t{0}), summary);
        }
        NodeSummary mine;
        mine.normal_sum = glm::vec3{brick.exposed_face_sum()} * (voxelEdge * voxelEdge);
        mine.coverage = static_cast<float>(brick.occupied_count()) / static_cast<float>(kBrickVoxels);
        const auto at = static_cast<std::uint32_t>(out.nodes.size());
        out.nodes.push_back(make_node_header(kNodeKindBrick, 0u, brick.representative()));
        out.nodes.push_back(static_cast<std::uint32_t>(out.bricks.size() / kBrickWords));
        out.nodes.push_back(pack_summary(mine));
        out.bricks.insert(out.bricks.end(), brick.words().begin(), brick.words().end());
        ++out.stats.bricks_kept;
        if (summary != nullptr) {
            *summary = mine;
        }
        return at;
    }

    std::uint32_t splice_job(glm::ivec3 cell, SubtreeOutput& out, std::vector<SubtreeOutput>& jobs,
                             NodeSummary* summary) const {
        const int n = 1 << params_.parallel_split_level;
        const auto un = static_cast<std::size_t>(n);
        const std::size_t index =
            (static_cast<std::size_t>(cell.z) * un + static_cast<std::size_t>(cell.y)) * un +
            static_cast<std::size_t>(cell.x);
        SubtreeOutput& job = jobs[index];
        out.stats.boxes_classified += job.stats.boxes_classified;
        out.stats.bricks_sampled += job.stats.bricks_sampled;
        out.stats.bricks_kept += job.stats.bricks_kept;
        out.stats.solid_leaves += job.stats.solid_leaves;
        out.stats.padding_words += job.stats.padding_words;
        out.stats.classify_seconds += job.stats.classify_seconds;
        out.stats.fill_seconds += job.stats.fill_seconds;
        for (std::size_t lv = 0; lv < kMaxLevels; ++lv) {
            out.stats.sampled_per_level[lv] += job.stats.sampled_per_level[lv];
            out.stats.empty_per_level[lv] += job.stats.empty_per_level[lv];
            out.stats.solid_per_level[lv] += job.stats.solid_per_level[lv];
        }
        if (job.root == kNoNode) {
            return kNoNode;
        }
        const auto nodeBase = static_cast<std::uint32_t>(out.nodes.size());
        const auto brickBase = static_cast<std::uint32_t>(out.bricks.size() / kBrickWords);
        relocate_subtree(job, job.root, nodeBase, brickBase);
        out.nodes.insert(out.nodes.end(), job.nodes.begin(), job.nodes.end());
        out.bricks.insert(out.bricks.end(), job.bricks.begin(), job.bricks.end());
        const std::uint32_t root = job.root + nodeBase;
        if (summary != nullptr) {
            *summary = job.summary;
        }
        job = SubtreeOutput{}; // release the job's memory as soon as it is merged
        return root;
    }

    const S& sampler_;
    TreeGeometry geometry_;
    BuildParams params_;
};

} // namespace world::svo::detail

namespace world::svo {

template <VoxelSampler S>
BrickTree build_tree(const S& sampler, const TreeGeometry& geometry, const BuildParams& params,
                     engine::jobs::ThreadPool* pool, BuildStats* stats) {
    if (geometry.voxel_bits() > kMaxVoxelBits || geometry.voxel_bits() < TreeGeometry::kBrickLog2) {
        throw std::invalid_argument("build_tree: voxel_bits must be within [3, 24]");
    }
    const auto start = std::chrono::steady_clock::now();
    detail::Builder<S> builder(sampler, geometry, params);
    detail::SubtreeOutput out;

    const int split = params.parallel_split_level;
    if (pool != nullptr && split > 0 && split <= geometry.max_brick_level()) {
        const int n = 1 << split;
        std::vector<detail::SubtreeOutput> jobs(static_cast<std::size_t>(n) * static_cast<std::size_t>(n) *
                                                static_cast<std::size_t>(n));
        std::vector<std::future<void>> futures;
        futures.reserve(jobs.size());
        for (int z = 0; z < n; ++z) {
            for (int y = 0; y < n; ++y) {
                for (int x = 0; x < n; ++x) {
                    const auto un = static_cast<std::size_t>(n);
                    const std::size_t index =
                        (static_cast<std::size_t>(z) * un + static_cast<std::size_t>(y)) * un +
                        static_cast<std::size_t>(x);
                    futures.push_back(pool->submit([&builder, &jobs, index, split, x, y, z] {
                        detail::SubtreeOutput& job = jobs[index];
                        job.root = builder.build_node(split, glm::ivec3{x, y, z}, job, nullptr, nullptr,
                                                      &job.summary);
                    }));
                }
            }
        }
        for (std::future<void>& f : futures) {
            f.get(); // rethrows a job's exception here, on the caller's thread
        }
        out.root = builder.build_node(0, glm::ivec3{0, 0, 0}, out, nullptr, &jobs, &out.summary);
    } else {
        out.root = builder.build_node(0, glm::ivec3{0, 0, 0}, out, nullptr, nullptr, &out.summary);
    }

    BrickTree tree;
    tree.geometry = geometry;
    if (out.root == kNoNode) {
        tree.nodes.clear();
        tree.bricks.clear();
        tree.root = 0;
    } else {
        tree.nodes = std::move(out.nodes);
        tree.bricks = std::move(out.bricks);
        tree.root = out.root;
    }
    if (stats != nullptr) {
        *stats = out.stats;
        stats->seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    }
    return tree;
}

} // namespace world::svo
