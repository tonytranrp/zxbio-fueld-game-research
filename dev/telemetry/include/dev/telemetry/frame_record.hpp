#pragma once

#include <cstddef>
#include <cstdint>

namespace dev::telemetry {

// Where a frame's CPU time went. Lifted verbatim out of run_svo's function body (goal 215), where
// it was a local struct only the slow-frame attributor could see -- which is why the harness could
// not have reported it without copying the frame loop.
struct FramePhases {
    // TWO phases past the six run_svo had, both added by goal 215's own check rather than by
    // design. With the original six, spawn_stand's tree-swap frame measured 16.5 ms of phases
    // against a 204.6 ms loop body, and the last frame measured 0.4 against 270.9. The gap was
    // NOT where it was first assumed (begin_frame) -- an end-to-end probe of the loop body found
    // it in `capture`: the verify readback and every PNG dump do a staging copy plus a full
    // WaitForIdle plus a libpng encode, and no phase covered them. `frame_start` was added in the
    // same pass and is genuinely small (~0 ms); it stays because "measured and near zero" is a
    // different statement from "not measured". See research/dev-harness-log.md.
    double frame_start = 0.0; // Session::begin_frame(): poll_events, clock tick, resize check
    double upload = 0.0;      // begin_upload + pump_upload (buffer creation, a slice's UpdateBuffer)
    double camera = 0.0;      // input, collision (incl. a synchronous cache refresh), rebuild request
    double sway = 0.0;        // Prompt 007 goal 335: the in-ring trees' spring sway. A NINTH phase
                              // because goal 190's Check is stated as a per-frame budget, and a
                              // budget folded into `camera` is a budget nobody can read off.
    double render = 0.0;      // SvoRenderer::render (CPU side: constants, draws recorded)
    double post = 0.0;        // the post chain
    double overlay = 0.0;     // ImGui
    double present = 0.0;     // swap
    double capture = 0.0;     // --verify-frame's readback, --dump-every, F2, a scenario capture
                              // point: a staging copy + WaitForIdle + libpng encode. Zero on a
                              // frame with no capture, and 200+ ms on one that has one.

    [[nodiscard]] double sum() const noexcept {
        return frame_start + upload + camera + sway + render + post + overlay + present + capture;
    }
};

// What was happening, so a slow frame has a cause rather than only a number.
struct FrameCauses {
    bool swapped = false;   // the whole tree landed and was swapped in this frame
    bool uploading = false; // a slice was copied this frame
    bool building = false;  // a background build was running
    bool refreshed = false; // the collider's height cache was rebuilt synchronously

    [[nodiscard]] bool any() const noexcept { return swapped || uploading || building || refreshed; }
};

// What the world looked like while the frame was drawn.
struct FrameCounters {
    std::size_t bricks = 0;
    std::size_t internal_nodes = 0;
    std::size_t solid_leaves = 0;
    std::size_t resident_bytes = 0;
    std::size_t gpu_bytes = 0;
    std::size_t uploads = 0;
    double gpu_ms = 0.0; // the timestamped march+resolve range; 0 before the first valid query
    // Goal 220's per-pass ranges. `gpu_frame_ms` brackets the whole frame's GPU work; those
    // below must sum inside it, and how closely is the Check.
    double gpu_frame_ms = 0.0;
    double gpu_beam_ms = 0.0; // goal 266's coarse start-t pre-pass
    double gpu_march_ms = 0.0;
    double gpu_resolve_ms = 0.0;
    double gpu_post_ms = 0.0;
    double gpu_grass_ms = 0.0; // Prompt 007 goal 339: the instanced raster blade overlay
    double gpu_overlay_ms = 0.0;

    [[nodiscard]] double gpu_pass_sum() const noexcept {
        return gpu_beam_ms + gpu_march_ms + gpu_resolve_ms + gpu_post_ms + gpu_grass_ms +
               gpu_overlay_ms;
    }
};

struct FrameRecord {
    std::uint32_t index = 0;
    double wall_ms = 0.0;
    double scenario_seconds = 0.0;
    FramePhases phases;
    FrameCauses causes;
    FrameCounters counters;
};

} // namespace dev::telemetry
