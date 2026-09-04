#pragma once

#include <cstdint>
#include <vector>

#include "engine/core/math.hpp"
#include "world/chunk/material.hpp"

namespace world::meshing {

struct Vertex {
    glm::vec3 position; // chunk-local space, same units as world::chunk's voxel grid
    glm::vec3 normal;
    world::chunk::MaterialID material;
    // Baked per-vertex (per-cell) ambient occlusion in [0,1]: 1.0 = fully open (flat ground and
    // anything convex), darker only for concave cells -- see research/baked-ao-design.md for why
    // per-vertex is the honest Surface-Nets adaptation of the per-face-corner cube-quad scheme.
    // Defaulted 1.0 so decoration geometry (trees) appended via aggregate init stays unoccluded.
    float ao = 1.0f;
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
};

} // namespace world::meshing
