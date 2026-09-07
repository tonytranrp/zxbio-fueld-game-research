#include "world/svo/cell_grid.hpp"

#include <algorithm>
#include <cmath>

namespace world::svo {
namespace {

// The ray, normalised into GRID CELL units: one unit is one cell edge, and the origin is the grid's
// own min corner. Working in these units is what makes the DDA below the textbook one rather than a
// version with `cell_edge` sprinkled through it.
struct GridRay {
    glm::vec3 o{0.0f};
    glm::vec3 d{0.0f};
    glm::vec3 invd{0.0f};
};

GridRay to_grid(const CellGrid& grid, const Ray& ray) noexcept {
    const float edge = grid.cell_edge();
    GridRay g;
    g.o = (ray.origin - grid.world_min()) / edge;
    g.d = ray.dir / edge;
    for (int a = 0; a < 3; ++a) {
        g.invd[a] = std::abs(g.d[a]) > 1.0e-20f ? 1.0f / g.d[a] : (g.d[a] >= 0.0f ? 1.0e30f : -1.0e30f);
    }
    return g;
}

} // namespace

std::size_t CellGrid::present_count() const noexcept {
    return static_cast<std::size_t>(
        std::count_if(cells_.begin(), cells_.end(), [](const GridCell& c) { return c.present(); }));
}

std::vector<glm::ivec3> CellGrid::coords() const {
    std::vector<glm::ivec3> out;
    out.reserve(cells_.size());
    for (int z = 0; z < dims_.z; ++z) {
        for (int y = 0; y < dims_.y; ++y) {
            for (int x = 0; x < dims_.x; ++x) {
                out.push_back(originCell_ + glm::ivec3{x, y, z});
            }
        }
    }
    return out;
}

const GridCell* CellGrid::at(glm::ivec3 coord) const noexcept {
    if (!contains(coord)) {
        return nullptr;
    }
    const glm::ivec3 l = coord - originCell_;
    return &cells_[static_cast<std::size_t>(l.x) +
                   static_cast<std::size_t>(dims_.x) *
                       (static_cast<std::size_t>(l.y) +
                        static_cast<std::size_t>(dims_.y) * static_cast<std::size_t>(l.z))];
}

void CellGrid::set(glm::ivec3 coord, std::shared_ptr<const BrickTree> tree) noexcept {
    if (!contains(coord)) {
        return;
    }
    const glm::ivec3 l = coord - originCell_;
    cells_[static_cast<std::size_t>(l.x) +
           static_cast<std::size_t>(dims_.x) *
               (static_cast<std::size_t>(l.y) +
                static_cast<std::size_t>(dims_.y) * static_cast<std::size_t>(l.z))]
        .tree = std::move(tree);
}

CellGrid::Totals CellGrid::totals() const noexcept {
    Totals t;
    for (const GridCell& c : cells_) {
        if (!c.present()) {
            continue;
        }
        ++t.present_cells;
        t.bricks += c.tree->brick_count();
        t.node_words += c.tree->node_words();
        t.bytes += static_cast<std::uint64_t>(c.tree->memory_bytes());
    }
    return t;
}

Hit trace_ray_grid(const CellGrid& grid, const Ray& ray, const TraceParams& params,
                   GridTraceStats* stats) noexcept {
    Hit miss;
    if (stats != nullptr) {
        *stats = GridTraceStats{};
    }
    if (grid.empty()) {
        return miss;
    }

    const GridRay g = to_grid(grid, ray);
    const glm::ivec3 dims = grid.dims();

    // Slab test against the whole grid, in cell units, so a ray that misses everything costs one
    // test rather than a walk.
    float tEnter = params.t_start > 0.0f ? params.t_start : 0.0f;
    float tExit = params.max_t;
    for (int a = 0; a < 3; ++a) {
        if (std::abs(g.d[a]) < 1.0e-20f) {
            // Parallel to this axis: constrained only by already being inside the slab.
            if (g.o[a] < 0.0f || g.o[a] >= static_cast<float>(dims[a])) {
                return miss;
            }
            continue;
        }
        const float t0 = (0.0f - g.o[a]) * g.invd[a];
        const float t1 = (static_cast<float>(dims[a]) - g.o[a]) * g.invd[a];
        tEnter = std::max(tEnter, std::min(t0, t1));
        tExit = std::min(tExit, std::max(t0, t1));
    }
    if (tExit < tEnter) {
        return miss;
    }

    // Amanatides & Woo, in cell units. The starting cell comes from the position at tEnter, clamped
    // -- a ray entering exactly on a face can land one cell out through float rounding, and clamping
    // is both cheaper and more robust than trying to make the arithmetic exact.
    glm::ivec3 cell;
    glm::ivec3 step;
    glm::vec3 tMax;
    glm::vec3 tDelta;
    const glm::vec3 entry = g.o + tEnter * g.d;
    for (int a = 0; a < 3; ++a) {
        cell[a] = std::clamp(static_cast<int>(std::floor(entry[a])), 0, dims[a] - 1);
        if (std::abs(g.d[a]) < 1.0e-20f) {
            step[a] = 0;
            tMax[a] = 1.0e30f;
            tDelta[a] = 1.0e30f;
            continue;
        }
        step[a] = g.d[a] > 0.0f ? 1 : -1;
        const float boundary = static_cast<float>(cell[a] + (step[a] > 0 ? 1 : 0));
        tMax[a] = (boundary - g.o[a]) * g.invd[a];
        tDelta[a] = std::abs(g.invd[a]);
    }

    // The walk. Cells are visited in ray order and `trace_ray` returns the nearest hit inside the
    // cell it is given, so the FIRST hit found is the globally nearest one.
    const glm::ivec3 originCell = grid.origin_cell();
    while (true) {
        if (stats != nullptr) {
            ++stats->cells_stepped;
        }
        if (const GridCell* c = grid.at(originCell + cell); c != nullptr && c->present()) {
            if (stats != nullptr) {
                ++stats->cells_entered;
            }
            // The cell's own tree positions itself by `geometry.origin`, so the WORLD ray goes
            // straight in -- no transform, and the LOD early-out still measures distance from the
            // ray's own origin exactly as goal 164 requires.
            const Hit hit = trace_ray(*c->tree, ray, params);
            if (hit.hit) {
                return hit;
            }
        }

        // Step to the next cell along the ray.
        const int axis = tMax.x < tMax.y ? (tMax.x < tMax.z ? 0 : 2) : (tMax.y < tMax.z ? 1 : 2);
        if (tMax[axis] > tExit) {
            return miss;
        }
        cell[axis] += step[axis];
        if (cell[axis] < 0 || cell[axis] >= dims[axis]) {
            return miss;
        }
        tMax[axis] += tDelta[axis];
    }
}

} // namespace world::svo
