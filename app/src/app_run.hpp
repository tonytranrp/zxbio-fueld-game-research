#pragma once

// The frame loop, extracted from main() (Prompt 002 goal 211).
//
// `Session` used to be a struct declared inside main.cpp's anonymous namespace, and `run_svo` /
// `run_mesh` were static functions beside it. Both are here now so `voxel_harness` links the same
// object file rather than a copy -- see frame_input.hpp for why that is non-negotiable.

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "app_options.hpp"
#include "engine/core/clock.hpp"
#include "engine/ecs/registry.hpp"
#include "frame_input.hpp"
#include "glfw_window.hpp"

namespace engine::input {
class GlfwInput;
}
namespace render::diligent {
class RenderContext;
class PostProcessor;
class DebugOverlay;
} // namespace render::diligent

namespace dev::telemetry {
struct FrameRecord;
}

namespace app {

// What the harness wants to know about a run that the app itself does not print. Every hook is
// optional; `voxel_app` passes a default-constructed RunHooks and the loop's behaviour is
// unchanged (a null std::function is simply not called).
struct RunHooks {
    // Once per rendered frame, after present, with that frame's full breakdown.
    std::function<void(const dev::telemetry::FrameRecord&)> on_frame;
    // Asked once per frame, before the capture phase: should this frame be written, and as what?
    // Returning an empty string means "no capture this frame".
    std::function<std::string(std::uint32_t frame, float scriptSeconds, bool worldReady,
                              bool treeSwappedThisFrame, bool slowFrame, bool grounded)>
        capture_name;
    // Called with the path actually written (or not written) for each capture.
    std::function<void(const std::string& name, const std::string& path, bool written)> on_capture;
    // The --verify-frame contrast fraction, so a scenario can assert on it without the harness
    // re-implementing the metric.
    std::function<void(float fraction)> on_verify;
    // End-of-run gameplay invariants. These were the ONE thing the harness asserted without
    // measuring: `assert walk_violations == 0` passed on a run that logged 1861 of them, because
    // RunResult::walk_violations was never written. An assertion that cannot fail is worse than no
    // assertion, because it reads as evidence.
    std::function<void(std::uint32_t walkViolations, std::uint32_t insideSolidEvents)> on_invariants;
    // Loading-phase frames, so the report can say how long the world was not there.
    std::function<void(double wallMs)> on_warmup_frame;
};

// Everything both renderer paths share: window, device, post chain, overlay, input, the camera
// entity, the frame clock. `visible` false creates the window hidden -- the same swap chain and
// the same readback path --verify-frame already uses, with nothing on screen. That is what
// `--headless` means here, and it is stated rather than implied: this is not a surfaceless
// presentation path, and a driver that behaves differently for an off-screen surface would not be
// caught by it.
struct Session {
    GlfwWindow window;
    std::unique_ptr<render::diligent::RenderContext> context;
    std::unique_ptr<render::diligent::PostProcessor> postProcess;
    engine::ecs::Registry registry;
    engine::ecs::Entity cameraEntity{};
    std::unique_ptr<render::diligent::DebugOverlay> overlay;
    engine::core::Clock clock;
    glm::vec3 spawnPosition{0.0f};

    Session(const AppOptions& options, bool visible);
    ~Session();

    // ImGui's GLFW backend chain-installs on top of whatever callbacks are already registered, so
    // this runs AFTER a LiveInput has installed GlfwInput's. Separated from the constructor
    // precisely so a scripted run can skip LiveInput entirely and still get an overlay.
    void attach_overlay();

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    // Per-frame preamble shared by both loops: events, clock, resize. Returns false to skip the
    // frame (minimized).
    bool begin_frame();
};

// The live keyboard/mouse. Owns the GlfwInput whose callbacks chain under ImGui's.
class LiveInput final : public FrameInput {
public:
    explicit LiveInput(GlfwWindow& window);
    ~LiveInput() override;

    [[nodiscard]] bool quit_requested() const override;
    [[nodiscard]] bool take_walk_toggle() override;
    [[nodiscard]] bool take_screenshot() override;
    [[nodiscard]] glm::vec2 take_look_delta() override;
    [[nodiscard]] TickInput tick(const glm::vec3& position, float yawRadians, float pitchRadians,
                                 bool jumpEdge) override;

    void begin_frame() override;
    [[nodiscard]] engine::input::GlfwInput& raw() noexcept { return *input_; }

private:
    std::unique_ptr<engine::input::GlfwInput> input_;
    bool jumpEdgeThisFrame_ = false;
};

// The two frame loops. `input` drives the simulation; `hooks` observes it.
int run_mesh(Session& session, const AppOptions& options, FrameInput& input, const RunHooks& hooks);
int run_svo(Session& session, const AppOptions& options, FrameInput& input, const RunHooks& hooks);

// Build a Session and dispatch on options.renderer. This is what main() calls.
int run(const AppOptions& options, bool visible = true);

} // namespace app
