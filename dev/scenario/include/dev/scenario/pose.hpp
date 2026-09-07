#pragma once

#include "engine/core/math.hpp"

namespace dev::scenario {

// Where the camera is and which way it looks. Degrees, not radians: a .scn file is written by a
// human, and every other debug surface in this repo (--yaw, --pitch, the overlay) is in degrees.
struct Pose {
    glm::vec3 position{0.0f};
    float yaw_deg = 0.0f;
    float pitch_deg = 0.0f;

    [[nodiscard]] friend bool operator==(const Pose&, const Pose&) = default;
};

// A pose given RELATIVE TO THE TERRAIN: x and z, and how far above the ground surface the eye
// should start. The harness resolves it against HeightmapGenerator::height_at at load time.
//
// This exists because the first version of walk_shoreline was authored with an absolute y of 30 m
// at a point where the ground is 48 m -- the camera spawned eighteen metres inside a hill, every
// capture was a flat grey square, and the contrast assertion read 0.0%. An absolute pose is a
// number that has to be right about a world it cannot see. It also would not have survived Prompt
// 006, which replaces the terrain generator outright: every absolute pose in the library would
// have silently moved underground on the day that landed.
struct GroundPose {
    glm::vec2 xz{0.0f};
    float yaw_deg = 0.0f;
    float pitch_deg = 0.0f;
    float height_above_ground = 1.7f; // an eye height by default

    [[nodiscard]] friend bool operator==(const GroundPose&, const GroundPose&) = default;
};

} // namespace dev::scenario
