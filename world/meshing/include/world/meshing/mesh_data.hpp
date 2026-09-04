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
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
};

} // namespace world::meshing
