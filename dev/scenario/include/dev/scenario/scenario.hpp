#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "dev/scenario/assertion.hpp"
#include "dev/scenario/capture.hpp"
#include "dev/scenario/motion_script.hpp"
#include "dev/scenario/pose.hpp"

namespace dev::scenario {

enum class BackendSelection : std::uint8_t { Vulkan, D3D12, Both };

// A named, versioned, deterministic description of "put the camera here, drive these inputs for
// this long, capture at these moments, assert these budgets".
//
// `options` are kept as raw argument strings rather than a parsed AppOptions: this module must
// build with -DVOXEL_BUILD_RENDERER=OFF (it is tested in the gating no-GPU CI job) and AppOptions
// names SvoRenderer::Settings. The harness feeds them through the app's own table, so a typo in a
// .scn produces the table's own diagnostic rather than a second one written here.
struct Scenario {
    std::string name;
    std::string description;
    std::vector<std::string> options;
    std::optional<Pose> pose;             // absolute
    std::optional<GroundPose> ground_pose; // relative to the terrain surface (preferred)
    std::vector<Segment> segments;
    std::vector<CapturePoint> captures;
    std::vector<Assertion> assertions;
    BackendSelection backend = BackendSelection::Both;
    std::string golden_dir; // relative to the repo's dev/goldens by convention; empty = no golden
    std::string source;     // the .scn path, or "<built-in>"

    [[nodiscard]] bool operator==(const Scenario& other) const noexcept {
        // `source` is deliberately excluded: a round-trip through emit()/parse() writes no path,
        // and two copies of the same scenario read from two places are the same scenario.
        return name == other.name && description == other.description && options == other.options &&
               pose == other.pose && ground_pose == other.ground_pose &&
               segments == other.segments && captures == other.captures &&
               assertions == other.assertions && backend == other.backend && golden_dir == other.golden_dir;
    }
};

[[nodiscard]] bool parse_backend(std::string_view text, BackendSelection& out) noexcept;
[[nodiscard]] std::string_view backend_name(BackendSelection backend) noexcept;

} // namespace dev::scenario
