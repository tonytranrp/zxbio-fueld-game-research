#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>

#include "aim_query.hpp"
#include "crash_handler.hpp"
#include "engine/core/clock.hpp"
#include "engine/core/log.hpp"
#include "engine/core/math.hpp"
#include "engine/ecs/components.hpp"
#include "engine/ecs/registry.hpp"
#include "engine/input/glfw_input.hpp"
#include "engine/jobs/thread_pool.hpp"
#include "glfw_window.hpp"
#include "render/diligent/debug_overlay.hpp"
#include "render/diligent/frame_verify.hpp"
#include "render/diligent/gpu_tools.hpp"
#include "render/diligent/post_process.hpp"
#include "render/diligent/render_context.hpp"
#include "render/diligent/terrain_renderer.hpp"
#include "render/interface/camera.hpp"
#include "spectator_camera.hpp"
#include "world/streaming/chunk_events.hpp"
#include "world/streaming/world_bounds.hpp"
#include "world_loader.hpp"

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
    std::uint32_t frames = 0; // 0 = run until the window is closed
    // Group S (Voxel Representation Redesign SS3): the world's horizontal Chebyshev half-size,
    // pregenerated once at startup rather than streamed around the camera. Defaults to goal 127's
    // 48-column trial size, not the original 8km ask -- see docs/goals.md goal 132.
    std::int32_t radius = world::streaming::kDefaultWorldBounds.radius_chunks;
    int seed = 1337;
    bool verify_frame = false; // Group B smoke check: read the frame back, fail on an empty one
    bool validation = false;
    bool autofly = false; // Group D smoke check: fly +X automatically once the world has loaded
    bool walk = false;    // start in walk (gravity) mode; with --autofly, also asserts no fall-through
    std::size_t upload_budget = 4; // mesh commits per frame; 0 = unlimited (the pre-fix stutter behavior)
    std::uint32_t dump_every = 0;  // goal 7: write a numbered frame dump every N frames (0 = off)
    // Goal 52's per-pass kill switches: isolate a visual regression to one pass without reverts.
    bool no_post = false;    // whole post chain off: render straight to swap chain (pre-Stage-2 path)
    bool no_bloom = false;   // bloom off, tonemap composite still on
    bool no_tonemap = false; // tonemap off (raw clamp), bloom still on
    bool no_sky = false;     // gradient-sky pass off (falls back to the flat clear color)
    // Debug camera overrides for the visual-verification workflow (goal 8's multi-angle baseline
    // and every later "view a dump from X" check): unset = the default start pose.
    std::optional<glm::vec3> start_pos;
    std::optional<float> start_yaw_deg;
    std::optional<float> start_pitch_deg;
};

// How long --verify-frame keeps waiting for the world to finish loading before declaring failure.
// Wall-clock, not a frame count: a loading-screen frame's cost is now dominated by WorldLoader::
// pump() draining real generation/mesh work, which varies with --radius, so a frame-count budget
// tuned for the old per-tick streaming system's cost has no fixed meaning here anymore.
constexpr std::chrono::seconds kVerifyLoadTimeout{600};

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
            options.radius =
                value ? static_cast<std::int32_t>(std::strtol(value, nullptr, 10)) : options.radius;
        } else if (arg == "--seed") {
            const char* value = next_value();
            options.seed = value ? static_cast<int>(std::strtol(value, nullptr, 10)) : options.seed;
        } else if (arg == "--verify-frame") {
            options.verify_frame = true;
        } else if (arg == "--validation") {
            options.validation = true;
        } else if (arg == "--autofly") {
            options.autofly = true;
        } else if (arg == "--walk") {
            options.walk = true;
        } else if (arg == "--upload-budget") {
            const char* value = next_value();
            options.upload_budget =
                value ? static_cast<std::size_t>(std::strtoul(value, nullptr, 10)) : options.upload_budget;
        } else if (arg == "--dump-every") {
            const char* value = next_value();
            options.dump_every =
                value ? static_cast<std::uint32_t>(std::strtoul(value, nullptr, 10)) : options.dump_every;
        } else if (arg == "--no-post") {
            options.no_post = true;
        } else if (arg == "--no-sky") {
            options.no_sky = true;
        } else if (arg == "--no-bloom") {
            options.no_bloom = true;
        } else if (arg == "--no-tonemap") {
            options.no_tonemap = true;
        } else if (arg == "--pos") {
            const char* value = next_value();
            glm::vec3 p{};
#if defined(_MSC_VER)
            const int parsed = value ? sscanf_s(value, "%f,%f,%f", &p.x, &p.y, &p.z) : 0;
#else
            const int parsed = value ? std::sscanf(value, "%f,%f,%f", &p.x, &p.y, &p.z) : 0;
#endif
            if (parsed == 3) {
                options.start_pos = p;
            } else {
                log(LogLevel::Error, "--pos expects x,y,z");
                return std::nullopt;
            }
        } else if (arg == "--yaw") {
            const char* value = next_value();
            options.start_yaw_deg = value ? static_cast<float>(std::strtod(value, nullptr)) : 0.0f;
        } else if (arg == "--pitch") {
            const char* value = next_value();
            options.start_pitch_deg = value ? static_cast<float>(std::strtod(value, nullptr)) : 0.0f;
#ifndef NDEBUG
        } else if (arg == "--crash-test") {
            // Group J task 20's check: deliberately exercise each crash-handler hook. Debug-only
            // by construction -- the flag does not exist in release builds.
            const char* value = next_value();
            const std::string_view mode = value ? value : "";
            log(LogLevel::Info, "crash-test: triggering \"{}\"", mode);
            if (mode == "av") {
                int* p = nullptr;
                *p = 42; // NOLINT(clang-analyzer-core.NullDereference) -- the point of the test
            } else if (mode == "abort") {
                std::abort();
            } else if (mode == "terminate") {
                std::terminate();
            }
            log(LogLevel::Error, "--crash-test expects av|abort|terminate, got \"{}\"", mode);
            return std::nullopt;
#endif
        } else {
            log(LogLevel::Error,
                "unknown argument \"{}\" (known: --mode vk|d3d12, --frames N, --radius N, --seed N, "
                "--verify-frame, --validation, --autofly, --walk, --upload-budget N, --dump-every N)",
                arg);
            return std::nullopt;
        }
    }
    return options;
}

// Goal 130: the overlay's ready-chunk count is event-sourced (ChunkMeshReady), the same discipline
// Group L established for the old streaming system -- a static world just drops the unload half of
// that pairing, since nothing ever unloads.
struct ChunkEventCounters {
    std::size_t ready = 0;
    std::size_t lifetime_loaded = 0; // ChunkLoaded events seen (voxels applied); monotonic

    void on_loaded(const world::streaming::ChunkLoaded&) { ++lifetime_loaded; }
    void on_mesh_ready(const world::streaming::ChunkMeshReady&) { ++ready; }

    void connect(engine::events::Dispatcher& dispatcher) {
        dispatcher.sink<world::streaming::ChunkLoaded>().connect<&ChunkEventCounters::on_loaded>(*this);
        dispatcher.sink<world::streaming::ChunkMeshReady>().connect<&ChunkEventCounters::on_mesh_ready>(
            *this);
    }
};

// ---- per-frame phases (goal 50: run() split into independently-readable pieces) ----------------

// Input handling + camera movement + walk-mode invariant accounting. Returns the camera snapshot
// the render pass consumes.
render::interface::Camera update_camera_phase(engine::ecs::Registry& registry,
                                              engine::ecs::Entity cameraEntity,
                                              engine::input::GlfwInput& input, app::WorldLoader& world,
                                              const engine::core::Clock& clock, const AppOptions& options,
                                              std::uint32_t& walkViolations) {
    auto [transform, lens, spectator] =
        registry.get<engine::ecs::Transform, engine::ecs::CameraLens, app::SpectatorCameraState>(
            cameraEntity);
    if (input.take_walk_toggle()) {
        // Deliberate transition handling (Group V task 25): position is untouched, vertical
        // velocity zeroed -- entering walk mid-air simply starts a clean fall; leaving it
        // freezes wherever you are. No snap in either direction.
        spectator.mode =
            spectator.mode == app::CameraMoveMode::Fly ? app::CameraMoveMode::Walk : app::CameraMoveMode::Fly;
        spectator.vertical_velocity = 0.0f;
        log(LogLevel::Info, "camera mode: {}", spectator.mode == app::CameraMoveMode::Walk ? "walk" : "fly");
    }
    const float groundHeight = world.ground_height(transform.position.x, transform.position.z);
    app::update_spectator_camera(transform, spectator, input.state(), input.take_look_delta(),
                                 static_cast<float>(clock.delta_seconds()), groundHeight);
    if (options.autofly) {
        // Constant sideways travel (at boost speed in fly mode; ground-bound in walk mode) --
        // goal 133's mechanical re-check: a static, fully-loaded world should show zero
        // generation-driven frame spikes crossing it, unlike the pre-redesign log's collapse to
        // 1-2 fps under the old per-tick streaming system.
        transform.position.x += (options.walk ? 20.0f : 160.0f) * static_cast<float>(clock.delta_seconds());
    }
    if (options.walk && spectator.mode == app::CameraMoveMode::Walk) {
        // Group V task 27's mechanical check, evaluated EVERY frame of a walk run: the camera
        // must never end an update below the ground surface (fall-through at chunk seams or
        // steep slopes is exactly the bug class this watches for).
        if (transform.position.y < groundHeight + app::kEyeHeight - 0.01f) {
            ++walkViolations;
        }
    }

    render::interface::Camera camera;
    camera.position = transform.position;
    camera.orientation = transform.orientation;
    camera.fov_y_radians = lens.fov_y_radians;
    camera.near_plane = lens.near_plane;
    camera.far_plane = lens.far_plane;
    return camera;
}

// Loop-carried telemetry for the overlay and the 2-second stats report.
struct FrameTelemetry {
    std::chrono::steady_clock::time_point lastReport = std::chrono::steady_clock::now();
    std::uint32_t framesSinceReport = 0;
    float smoothedFrameMs = 0.0f;
    float worstFrameMs = 0.0f;        // per 2s window -- the number a stutter actually is
    float worstFrameMsOverall = 0.0f; // goal 133: worst frame across the WHOLE run, not just one window
    render::diligent::GpuMemoryBudget budget;
    std::chrono::steady_clock::time_point lastBudgetPoll =
        std::chrono::steady_clock::now() - std::chrono::hours(1);
};

// Budget poll + overlay draw (pre-Present).
void overlay_phase(FrameTelemetry& t, const engine::core::Clock& clock,
                   render::diligent::RenderContext& context, render::diligent::TerrainRenderer& renderer,
                   app::WorldLoader& world, render::diligent::DebugOverlay& overlay,
                   const ChunkEventCounters& chunkCounters, const render::interface::Camera& camera) {
    if (std::chrono::steady_clock::now() - t.lastBudgetPoll >= std::chrono::seconds(2)) {
        const bool firstPoll = !t.budget.available;
        t.budget = render::diligent::query_gpu_memory_budget(context);
        t.lastBudgetPoll = std::chrono::steady_clock::now();
        if (firstPoll && t.budget.available) {
            log(LogLevel::Info,
                "VK_EXT_memory_budget: {:.0f} MiB device-local budget, {:.0f} MiB in use machine-wide",
                static_cast<double>(t.budget.device_local_budget_bytes) / (1024.0 * 1024.0),
                static_cast<double>(t.budget.device_local_usage_bytes) / (1024.0 * 1024.0));
        }
    }
    const float dtMs = static_cast<float>(clock.delta_seconds()) * 1000.0f;
    t.smoothedFrameMs = t.smoothedFrameMs == 0.0f ? dtMs : t.smoothedFrameMs * 0.95f + dtMs * 0.05f;
    render::diligent::OverlayStats stats;
    stats.fps = t.smoothedFrameMs > 0.0f ? 1000.0f / t.smoothedFrameMs : 0.0f;
    stats.frame_ms = t.smoothedFrameMs;
    stats.ready_chunks = chunkCounters.ready; // event-sourced, not polled
    stats.visible_chunks = renderer.last_visible_count();
    stats.total_chunk_meshes = renderer.chunk_count();
    const app::TreeEmitCounts objectCounts = world.object_counts();
    stats.objects = objectCounts.total();
    stats.objects_round = objectCounts.round;
    stats.objects_conifer = objectCounts.conifer;
    stats.objects_shrub = objectCounts.shrub;
    // Goal 84: what material the crosshair (view center) is aiming at -- analytic ray march.
    const glm::vec3 aimDir = camera.orientation * glm::vec3(0.0f, 0.0f, -1.0f);
    const app::AimHit aim = app::query_aim(world.heightmap(), camera.position, aimDir);
    if (aim.hit) {
        std::snprintf(stats.aim_line, sizeof(stats.aim_line), "%s @ %.0f,%.0f,%.0f",
                      app::material_name(aim.material), static_cast<double>(aim.position.x),
                      static_cast<double>(aim.position.y), static_cast<double>(aim.position.z));
    }
    stats.gpu_self_bytes = renderer.gpu_memory().allocated_bytes();
    stats.gpu_self_peak_bytes = renderer.gpu_memory().peak_bytes();
    stats.budget = t.budget;
    overlay.render(stats);
}

// The 2-second stats line + event/poll consistency check (post-Present). Group S note: no more
// in-flight/pending breakdown -- once the world has finished loading (the only state this phase
// ever runs in) there is no streaming work left to report on, by construction.
void report_phase(FrameTelemetry& t, const engine::core::Clock& clock,
                  render::diligent::TerrainRenderer& renderer, app::WorldLoader& world,
                  const ChunkEventCounters& chunkCounters) {
    const auto frameMs = static_cast<float>(clock.delta_seconds()) * 1000.0f;
    t.worstFrameMs = std::max(t.worstFrameMs, frameMs);
    t.worstFrameMsOverall = std::max(t.worstFrameMsOverall, frameMs);
    const auto now = std::chrono::steady_clock::now();
    if (now - t.lastReport < std::chrono::seconds(2)) {
        return;
    }
    const double seconds = std::chrono::duration<double>(now - t.lastReport).count();
    log(LogLevel::Info, "{:.1f} fps (worst {:.1f} ms), {}/{} chunk meshes visible after culling, {} ready",
        t.framesSinceReport / seconds, t.worstFrameMs, renderer.last_visible_count(), renderer.chunk_count(),
        world.ready_chunk_count());
    t.worstFrameMs = 0.0f;
    if (chunkCounters.ready != world.ready_chunk_count()) {
        log(LogLevel::Error, "event/poll chunk-count mismatch: events say {}, loader says {}",
            chunkCounters.ready, world.ready_chunk_count());
    }
    t.lastReport = now;
    t.framesSinceReport = 0;
}

// Verification readback + frame dumps + F2 screenshots (all pre-Present readbacks). Only ever
// called once the world has finished loading (verify-frame's whole premise is "does the finished
// scene look right").
struct CaptureState {
    bool verifyOk = false;
    bool verifyRan = false;
    std::uint32_t screenshotCounter = 0; // F2 capture numbering (goal 9)
};

bool capture_phase(CaptureState& cap, const AppOptions& options, std::uint32_t frame,
                   render::diligent::RenderContext& context, std::size_t readyChunks,
                   engine::input::GlfwInput& input) {
    if (options.verify_frame && !cap.verifyRan && readyChunks > 0) {
        const float fraction = render::diligent::sample_non_reference_pixel_fraction(context);
        // Local-contrast metric (see frame_verify.cpp for the two prior metrics it replaced and
        // why): terrain texture measures 12.3% at this pose, sky-only 0.9%. 6% keeps the
        // winding-bug lesson's discrimination -- sliver-band frames fail hard.
        cap.verifyOk = fraction >= 0.06f;
        cap.verifyRan = true;
        log(cap.verifyOk ? LogLevel::Info : LogLevel::Error,
            "frame verification: {:.1f}% of pixels carry terrain-scale local contrast (threshold 6%) after "
            "{} chunks loaded",
            static_cast<double>(fraction) * 100.0, readyChunks);
    }
    if (options.dump_every > 0 && frame % options.dump_every == 0) {
        char dumpPath[64];
        std::snprintf(dumpPath, sizeof(dumpPath), "frame_%05u.png", frame);
        (void)render::diligent::dump_frame(context, dumpPath);
    }
    if (input.take_screenshot()) {
        char shotPath[64];
        std::snprintf(shotPath, sizeof(shotPath), "screenshot_%03u.png", cap.screenshotCounter++);
        const bool shotOk = render::diligent::dump_frame(context, shotPath);
        log(shotOk ? LogLevel::Info : LogLevel::Error, "screenshot: {} ({})", shotPath,
            shotOk ? "written" : "FAILED");
    }
    return true;
}

int run(const AppOptions& options) {
    app::GlfwWindow window(1280, 720, "voxel_app");

    render::diligent::RenderContextCreateInfo contextCI;
    contextCI.backend = options.backend;
    contextCI.native_window_handle = window.native_handle();
    contextCI.enable_validation = options.validation;
    render::diligent::RenderContext context(contextCI);

    // Post-process chain (Group D): bloom + tonemap over an offscreen HDR scene target.
    // Constructed BEFORE the renderer on purpose -- it registers the scene target whose format
    // the terrain PSO is created against. Skippable wholesale for A/B against the direct path.
    std::unique_ptr<render::diligent::PostProcessor> postProcess;
    if (!options.no_post) {
        postProcess = std::make_unique<render::diligent::PostProcessor>(context);
        postProcess->set_bloom_enabled(!options.no_bloom);
        postProcess->set_tonemap_enabled(!options.no_tonemap);
    }
    render::diligent::TerrainRenderer renderer(context);
    renderer.set_sky_enabled(!options.no_sky);
    render::diligent::attach_gpu_profiler(context); // Tracy GPU zones (Vulkan only; safe no-op elsewhere)

    // The camera is an ordinary ECS entity (Phase 1 brief §6): Transform + CameraLens are
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
        // Debug pose overrides (goal 8's multi-angle visual baseline) win over the derived pose.
        if (options.start_pos) {
            transform.position = *options.start_pos;
        }
        if (options.start_yaw_deg) {
            spectator.yaw_radians = glm::radians(*options.start_yaw_deg);
        }
        if (options.start_pitch_deg) {
            spectator.pitch_radians = glm::radians(*options.start_pitch_deg);
        }
        if (options.walk) {
            spectator.mode = app::CameraMoveMode::Walk; // starts mid-air and falls to the ground
        }
    }
    const glm::vec3 spawnPosition = registry.get<engine::ecs::Transform>(cameraEntity).position;

    engine::input::GlfwInput input(window.handle());
    // After GlfwInput: the overlay's ImGui GLFW backend chain-installs on top of input's
    // callbacks, so both receive events.
    render::diligent::DebugOverlay overlay(context, window.handle());
    engine::core::Clock clock;

    // Group S (Voxel Representation Redesign SS3): the world is static and bounded, pregenerated
    // once at startup instead of streamed around the camera. --radius maps directly to the
    // world's own horizontal half-size now, not a camera-relative load window.
    const world::streaming::WorldBounds bounds{options.radius, world::streaming::kDefaultWorldBounds.y_min,
                                               world::streaming::kDefaultWorldBounds.y_max};
    engine::events::Dispatcher dispatcher;
    ChunkEventCounters chunkCounters;
    chunkCounters.connect(dispatcher);
    app::WorldLoader world(bounds, options.seed, std::jthread::hardware_concurrency(), renderer, registry,
                           dispatcher, spawnPosition, options.upload_budget);
    world.begin();

    CaptureState cap;
    cap.verifyOk = !options.verify_frame;
    FrameTelemetry telemetry;
    std::uint32_t frame = 0;
    std::uint32_t walkViolations = 0; // frames ending below the ground surface in walk mode
    bool loggedReady = false;
    const auto loadStart = std::chrono::steady_clock::now();

    while (!window.should_close() && (options.frames == 0 || frame < options.frames)) {
        window.poll_events();
        if (input.state().quit_requested) {
            break;
        }
        clock.tick();

        const auto [width, height] = window.framebuffer_size();
        if (width == 0 || height == 0) {
            continue; // minimized -- nothing to render into
        }
        if (width != context.width() || height != context.height()) {
            context.resize(width, height);
        }

        if (!world.finished()) {
            // Goal 128/130: one increment of the one-time generation/mesh/upload pass, then a
            // real, moving loading-screen frame -- no interactive camera control, no capture/
            // verify logic, none of that is meaningful before the world exists to look at.
            world.pump();
            renderer.render(render::interface::Camera{}); // sky-only backdrop; zero chunks uploaded yet
            if (postProcess) {
                postProcess->execute(frame);
            }
            overlay.render_loading(world.ready_chunk_count(), world.total_chunk_count());
            // Goal 130's own check: a viewable capture of the loading screen mid-generation, using
            // the same --dump-every/VOXEL_DUMP_FRAME machinery the interactive phase's
            // capture_phase uses (not a second capture mechanism).
            if (options.dump_every > 0 && frame % options.dump_every == 0) {
                char dumpPath[64];
                std::snprintf(dumpPath, sizeof(dumpPath), "loading_%05u.png", frame);
                (void)render::diligent::dump_frame(context, dumpPath);
            }
            if (options.verify_frame && std::chrono::steady_clock::now() - loadStart > kVerifyLoadTimeout) {
                log(LogLevel::Error, "frame verification: world never finished loading within {}s",
                    kVerifyLoadTimeout.count());
                break;
            }
        } else {
            if (!loggedReady) {
                const double loadSeconds =
                    std::chrono::duration<double>(std::chrono::steady_clock::now() - loadStart).count();
                log(LogLevel::Info, "world ready: {} chunks in {:.1f}s", world.ready_chunk_count(),
                    loadSeconds);
                loggedReady = true;
            }
            const render::interface::Camera camera =
                update_camera_phase(registry, cameraEntity, input, world, clock, options, walkViolations);

            renderer.render(camera);
            if (postProcess) {
                postProcess->execute(frame);
            }
            overlay_phase(telemetry, clock, context, renderer, world, overlay, chunkCounters, camera);

            capture_phase(cap, options, frame, context, world.ready_chunk_count(), input);

            report_phase(telemetry, clock, renderer, world, chunkCounters);
        }

        context.present();
        FrameMark;
        ++frame;
        ++telemetry.framesSinceReport;

        // A verify-only run (no explicit frame budget) has done its job once the readback ran.
        if (options.verify_frame && cap.verifyRan && options.frames == 0) {
            break;
        }
    }

    bool walkOk = true;
    if (options.walk) {
        walkOk = walkViolations == 0;
        log(walkOk ? LogLevel::Info : LogLevel::Error,
            "walk mode: {} frames ended below the ground surface (0 required)", walkViolations);
    }

    bool autoflyOk = true;
    if (options.autofly) {
        // Goal 133's real check: a static, fully-loaded world's chunk count must not change AT
        // ALL while flying through it (the old bound-with-slack math assumed unload hysteresis
        // trailing a moving camera; a static world has no such trailing band to bound -- the
        // count is either exactly the loaded total, or something regressed).
        autoflyOk = world.ready_chunk_count() == world.total_chunk_count();
        log(autoflyOk ? LogLevel::Info : LogLevel::Error,
            "autofly: {} / {} chunks loaded at exit, worst frame {:.1f} ms over the whole run, {:.1f} MiB "
            "GPU (peak {:.1f})",
            world.ready_chunk_count(), world.total_chunk_count(),
            static_cast<double>(telemetry.worstFrameMsOverall),
            static_cast<double>(renderer.gpu_memory().allocated_bytes()) / (1024.0 * 1024.0),
            static_cast<double>(renderer.gpu_memory().peak_bytes()) / (1024.0 * 1024.0));
    }

    log(LogLevel::Info, "exiting after {} frames on {}", frame,
        render::diligent::to_string(context.backend()));
    return cap.verifyOk && autoflyOk && walkOk ? EXIT_SUCCESS : EXIT_FAILURE;
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
