#pragma once

#include <cstdint>
#include <limits>

#include "engine/core/math.hpp"
#include "world/chunk/material.hpp"
#include "world/svo/brick_tree.hpp"

namespace world::svo {

struct Ray {
    glm::vec3 origin{0.0f};
    glm::vec3 dir{0.0f, 0.0f, -1.0f}; // need not be normalized; t is in units of |dir|
};

struct TraceParams {
    // LOD early-out (Laine & Karras): stop descending when a child's edge, divided by the distance
    // travelled, drops below this angle (radians ~ one pixel's angular size * quality). 0 disables
    // it -- the exact traversal the tests use.
    float lod_pixel_angle = 0.0f;
    // Distance already travelled from the CAMERA before this ray started (secondary rays), so LOD
    // is judged by distance from the eye, not from the bounce point.
    float t_offset = 0.0f;
    float max_t = std::numeric_limits<float>::infinity();
};

struct Hit {
    bool hit = false;
    float t = 0.0f;
    world::chunk::MaterialID material = world::chunk::MaterialID::Air;
    glm::ivec3 normal{0, 0, 0}; // axis-aligned face normal (one non-zero component)
    glm::vec3 position{0.0f};   // origin + t * dir
    int level = -1;             // node level the hit came from (brick level for a voxel hit)
    bool lod_cube = false;      // true when an LOD early-out shaded an internal node as a cube
    std::uint32_t steps = 0;    // traversal iterations (diagnostics)
};

// The CPU REFERENCE marcher (research/micro-voxel-pivot-log.md §2.7): stack-based octree descent
// with integer-cell stepping (the exit axis forces the next cell's coordinate exactly; only the
// non-exit axes are position-derived, and those are clamped into the exited cell) plus an
// Amanatides-Woo DDA inside brick leaves. It walks the exact flat words the GPU gets; the HLSL in
// render/diligent/shaders/svo_march.psh.hlsl mirrors this function statement for statement, and
// tools/svo_render renders whole frames with it so a GPU frame can be diffed against it.
[[nodiscard]] Hit trace_ray(const BrickTree& tree, const Ray& ray, const TraceParams& params = {}) noexcept;

// Ground-truth oracle for tests: brute-force DDA over every finest voxel of a UNIFORM-LOD tree
// (via BrickTree::material_at), no hierarchy -- slow, obviously correct.
[[nodiscard]] Hit trace_ray_brute_force(const BrickTree& tree, const Ray& ray) noexcept;

} // namespace world::svo
