#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>

#include "chunk_streaming.hpp"
#include "crash_handler.hpp"
#include "engine/core/clock.hpp"
#include "engine/core/log.hpp"
#include "engine/core/math.hpp"
#include "engine/ecs/components.hpp"
#include "engine/ecs/registry.hpp"
#include "engine/input/glfw_input.hpp"
#include "engine/jobs/thread_pool.hpp"
#include "glfw_window.hpp"
#include "spectator_camera.hpp"
#include "render/diligent/debug_overlay.hpp"
#include "render/diligent/frame_verify.hpp"
#include "render/diligent/gpu_tools.hpp"
#include "render/diligent/render_context.hpp"
#include "render/diligent/terrain_renderer.hpp"
#include "render/interface/camera.hpp"
#include "world/streaming/chunk_streamer.hpp"

#if defined(TRACY_ENABLE)
#include <tracy/Tracy.hpp>
#else
#define FrameMark
#endif

namespace {

using engine::core::log;
using engine::core::LogLevel;

struct AppOptions {
    render::diligent::Backend backend = render::diligent::Backend::Vulkan;
    std::uint32_t frames = 0;   // 0 = run until the window is closed
    std::int32_t radius = 3;    // meshed horizontal chunk radius around the origin
    int seed = 1337;
    bool verify_frame = false;  // Group B smoke check: read the frame back, fail on an empty one
    bool validation = false;
    bool autofly = false; // Group D smoke check: fly +X automatically; fail if unload never bounds the loaded set
};

// How long --verify-frame keeps waiting for streaming to settle before declaring failure. At
// debug-build meshing speeds the initial radius-3 load takes roughly 10 s of wall clock; anything
// that far past it means the pipeline stalled, not that it is slow.
constexpr std::uint32_t kVerifyStreamingTimeoutFrames = 3000;

std::optional<AppOptions> parse_args(std::span<char*> args) {
    AppOptions options;
    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string_view arg = args[i];
        const auto next_value = [&]() -> const char* { return i + 1 < args.size() ? args[++i] : nullptr; };
        if (arg == "--mode") {
            const char* value = next_value();
            const std::string_view mode = value ? value : "";
            if (mode == "vk" || mode == "vulkan") {
                options.backend = render::diligent::Backend::Vulkan;
            } else if (mode == "d3d12") {
                options.backend = render::diligent::Backend::D3D12;
            } else {
                log(LogLevel::Error, "--mode expects vk|d3d12, got \"{}\"", mode);
                return std::nullopt;
            }
        } else if (arg == "--frames") {
            const char* value = next_value();
            options.frames = value ? static_cast<std::uint32_t>(std::strtoul(value, nullptr, 10)) : 0;
        } else if (arg == "--radius") {
            const char* value = next_value();
            options.radius = value ? static_cast<std::int32_t>(std::strtol(value, nullptr, 10)) : options.radius;
        } else if (arg == "--seed") {
            const char* value = next_value();
            options.seed = value ? static_cast<int>(std::strtol(value, nullptr, 10)) : options.seed;
        } else if (arg == "--verify-frame") {
            options.verify_frame = true;
        } else if (arg == "--validation") {
            options.validation = true;
        } else if (arg == "--autofly") {
            options.autofly = true;
        } else {
            log(LogLevel::Error, "unknown argument \"{}\" (known: --mode vk|d3d12, --frames N, --radius N, --seed N, --verify-frame, --validation, --autofly)", arg);
            return std::nullopt;
        }
    }
    return options;
}

int run(const AppOptions& options) {
    app::GlfwWindow window(1280, 720, "voxel_app");

    render::diligent::RenderContextCreateInfo contextCI;
    contextCI.backend = options.backend;
    contextCI.native_window_handle = window.native_handle();
    contextCI.enable_validation = options.validation;
    render::diligent::RenderContext context(contextCI);

    render::diligent::TerrainRenderer renderer(context);
    render::diligent::attach_gpu_profiler(context); // Tracy GPU zones (Vulkan only; safe no-op elsewhere)

    // The camera is an ordinary ECS entity (PHASE_1_BRIEF.md §6): Transform + CameraLens are
    // engine components, SpectatorCameraState is this app's movement policy. Starts above the
    // terrain looking toward the origin; WASD + Space/Ctrl fly it, holding RMB mouse-looks,
    // Shift boosts, Esc quits.
    engine::ecs::Registry registry;
    const engine::ecs::Entity cameraEntity = registry.create();
    {
        auto& transform = registry.emplace<engine::ecs::Transform>(cameraEntity);
        transform.position = {40.0f, 110.0f, 170.0f};
        registry.emplace<engine::ecs::CameraLens>(cameraEntity);
        auto& spectator = registry.emplace<app::SpectatorCameraState>(cameraEntity);
        // Initial yaw/pitch chosen to face the terrain origin from the start position (the
        // quaternion is rebuilt from these every update, so they are the source of truth).
        const glm::vec3 toOrigin = glm::normalize(glm::vec3(0.0f, 20.0f, 0.0f) - transform.position);
        spectator.yaw_radians = std::atan2(-toOrigin.x, -toOrigin.z);
        spectator.pitch_radians = std::asin(toOrigin.y);
    }

    engine::input::GlfwInput input(window.handle());
    // After GlfwInput: the overlay's ImGui GLFW backend chain-installs on top of input's
    // callbacks, so both receive events.
    render::diligent::DebugOverlay overlay(context, window.handle());
    engine::core::Clock clock;

    // Group D: terrain streams in around the camera instead of being pre-built. --radius maps to
    // R_load; R_unload keeps the mandatory >=2 hysteresis gap on top of it. The streaming system
    // owns its worker pool so its teardown ordering is self-contained.
    world::streaming::StreamingConfig streamingConfig;
    streamingConfig.load_radius = options.radius;
    streamingConfig.unload_radius = options.radius + 2;
    app::ChunkStreamingSystem streaming(streamingConfig, options.seed, std::jthread::hardware_concurrency(),
                                        renderer, registry);
    log(LogLevel::Info, "streaming with load radius {}, unload radius {} (+{:.0f}s delay), {} worker threads",
        streamingConfig.load_radius, streamingConfig.unload_radius, streamingConfig.unload_delay_seconds,
        streaming.worker_thread_count());

    bool verifyOk = !options.verify_frame;
    bool verifyRan = false;
    std::uint32_t frame = 0;
    auto lastReport = std::chrono::steady_clock::now();
    std::uint32_t framesSinceReport = 0;
    float smoothedFps = 0.0f;

    // VK_EXT_memory_budget is polled on a timer per its own documented usage pattern, never per
    // frame (task 30).
    render::diligent::GpuMemoryBudget budget;
    auto lastBudgetPoll = std::chrono::steady_clock::now() - std::chrono::hours(1);

    while (!window.should_close() && (options.frames == 0 || frame < options.frames)) {
        window.poll_events();
        if (input.state().quit_requested) {
            break;
        }
        clock.tick();

        auto [transform, lens, spectator] =
            registry.get<engine::ecs::Transform, engine::ecs::CameraLens, app::SpectatorCameraState>(cameraEntity);
        app::update_spectator_camera(transform, spectator, input.state(), input.take_look_delta(),
                                     static_cast<float>(clock.delta_seconds()));
        if (options.autofly) {
            // Constant sideways travel at boost speed -- exercises the full load/unload cycle
            // (hysteresis, delayed removal, GPU teardown) with no human at the controls.
            transform.position.x += 160.0f * static_cast<float>(clock.delta_seconds());
        }

        streaming.update(transform.position, clock.elapsed_seconds());

        render::interface::Camera camera;
        camera.position = transform.position;
        camera.orientation = transform.orientation;
        camera.fov_y_radians = lens.fov_y_radians;
        camera.near_plane = lens.near_plane;
        camera.far_plane = lens.far_plane;

        const auto [width, height] = window.framebuffer_size();
        if (width == 0 || height == 0) {
            continue; // minimized -- nothing to render into
        }
        if (width != context.width() || height != context.height()) {
            context.resize(width, height);
        }

        renderer.render(camera);

        if (std::chrono::steady_clock::now() - lastBudgetPoll >= std::chrono::seconds(2)) {
            const bool firstPoll = !budget.available;
            budget = render::diligent::query_gpu_memory_budget(context);
            lastBudgetPoll = std::chrono::steady_clock::now();
            if (firstPoll && budget.available) {
                log(LogLevel::Info, "VK_EXT_memory_budget: {:.0f} MiB device-local budget, {:.0f} MiB in use machine-wide",
                    static_cast<double>(budget.device_local_budget_bytes) / (1024.0 * 1024.0),
                    static_cast<double>(budget.device_local_usage_bytes) / (1024.0 * 1024.0));
            }
        }
        {
            const float dtMs = static_cast<float>(clock.delta_seconds()) * 1000.0f;
            const float instantFps = dtMs > 0.0f ? 1000.0f / dtMs : 0.0f;
            smoothedFps = smoothedFps == 0.0f ? instantFps : smoothedFps * 0.95f + instantFps * 0.05f;
            render::diligent::OverlayStats stats;
            stats.fps = smoothedFps;
            stats.frame_ms = dtMs;
            stats.ready_chunks = streaming.ready_chunk_count();
            stats.visible_chunks = renderer.last_visible_count();
            stats.total_chunk_meshes = renderer.chunk_count();
            stats.jobs_in_flight = streaming.in_flight_count();
            stats.gpu_self_bytes = renderer.gpu_memory().allocated_bytes();
            stats.gpu_self_peak_bytes = renderer.gpu_memory().peak_bytes();
            stats.budget = budget;
            overlay.render(stats);
        }

        // Verification waits for streaming to settle (terrain arrives asynchronously now), then
        // reads the frame back while it is still the current back buffer -- before Present.
        if (options.verify_frame && !verifyRan && frame >= 5 && streaming.settled() &&
            streaming.ready_chunk_count() > 0) {
            const float fraction = render::diligent::sample_non_reference_pixel_fraction(context);
            verifyOk = fraction >= 0.05f;
            verifyRan = true;
            log(verifyOk ? LogLevel::Info : LogLevel::Error,
                "frame verification: {:.1f}% of pixels differ from the sky reference pixel (threshold 5%) after "
                "{} chunks streamed in",
                static_cast<double>(fraction) * 100.0, streaming.ready_chunk_count());
        }
        if (options.verify_frame && !verifyRan && frame >= kVerifyStreamingTimeoutFrames) {
            log(LogLevel::Error, "frame verification: streaming never settled within {} frames", frame);
            break;
        }

        context.present();
        FrameMark;
        ++frame;
        ++framesSinceReport;

        // A verify-only run (no explicit frame budget) has done its job once the readback ran.
        if (options.verify_frame && verifyRan && options.frames == 0) {
            break;
        }

        const auto now = std::chrono::steady_clock::now();
        if (now - lastReport >= std::chrono::seconds(2)) {
            const double seconds = std::chrono::duration<double>(now - lastReport).count();
            log(LogLevel::Info,
                "{:.1f} fps, {}/{} chunk meshes visible after culling, {} ready / {} in flight "
                "(pending {}, gen {}, mesh {}), {:.1f} MiB GPU",
                framesSinceReport / seconds, renderer.last_visible_count(), renderer.chunk_count(),
                streaming.ready_chunk_count(), streaming.in_flight_count(), streaming.pending_mesh_count(),
                streaming.generation_in_flight_count(), streaming.mesh_in_flight_count(),
                static_cast<double>(renderer.gpu_memory().allocated_bytes()) / (1024.0 * 1024.0));
            lastReport = now;
            framesSinceReport = 0;
        }
    }

    bool autoflyOk = true;
    if (options.autofly) {
        // If delayed unload (tasks 22/25/26) works, the loaded set stays near one desired cube;
        // if it silently never unloads, 15s of travel accumulates hundreds of stale chunks. 2x the
        // full cube is a deliberately loose bound that still cleanly separates the two.
        const auto span = static_cast<std::size_t>(2 * options.radius + 1);
        const std::size_t bound = span * span * 6 * 2;
        autoflyOk = streaming.ready_chunk_count() <= bound;
        log(autoflyOk ? LogLevel::Info : LogLevel::Error,
            "autofly: {} chunks loaded at exit (bound {}), {:.1f} MiB GPU, peak {:.1f} MiB",
            streaming.ready_chunk_count(), bound,
            static_cast<double>(renderer.gpu_memory().allocated_bytes()) / (1024.0 * 1024.0),
            static_cast<double>(renderer.gpu_memory().peak_bytes()) / (1024.0 * 1024.0));
    }

    log(LogLevel::Info, "exiting after {} frames on {}", frame, render::diligent::to_string(context.backend()));
    return verifyOk && autoflyOk ? EXIT_SUCCESS : EXIT_FAILURE;
}

} // namespace

int main(int argc, char** argv) {
    app::install_crash_handler();
    // Unbuffered stdout: when output is redirected to a file (smoke runs, CI), full buffering
    // would otherwise eat the final log lines -- including exception reports -- if the process
    // dies without flushing. Cost is irrelevant at this log volume.
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    const auto options = parse_args(std::span<char*>(argv, static_cast<std::size_t>(argc)));
    if (!options) {
        return EXIT_FAILURE;
    }
    try {
        return run(*options);
    } catch (const std::exception& e) {
        log(LogLevel::Error, "fatal: {}", e.what());
        return EXIT_FAILURE;
    }
}
