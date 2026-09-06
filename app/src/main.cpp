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
#include "render/diligent/svo_renderer.hpp"
#include "render/diligent/terrain_renderer.hpp"
#include "render/interface/camera.hpp"
#include "spectator_camera.hpp"
#include "svo_world.hpp"
#include "world/collision/aabb_sweep.hpp"
#include "world/collision/terrain_collider.hpp"
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

// Which world representation + renderer to run (micro-voxel pivot, docs/goals.md Groups W-X).
//   Svo:  sparse-brick octree at sub-centimeter voxels near the camera, GPU ray-marched.
//   Mesh: the greedy-meshed 1 m chunk world (Groups P-V), kept intact as the fallback.
enum class RendererKind { Svo, Mesh };

struct AppOptions {
    render::diligent::Backend backend = render::diligent::Backend::Vulkan;
    RendererKind renderer = RendererKind::Svo;
    std::uint32_t frames = 0; // 0 = run until the window is closed
    // Group S (Voxel Representation Redesign SS3): the mesh world's horizontal Chebyshev half-size,
    // pregenerated once at startup rather than streamed around the camera. Defaults to goal 127's
    // 48-column trial size, not the original 8km ask -- see docs/goals.md goal 132.
    std::int32_t radius = world::streaming::kDefaultWorldBounds.radius_chunks;
    int seed = 1337;
    bool verify_frame = false; // Group B smoke check: read the frame back, fail on an empty one
    bool validation = false;
    bool autofly = false; // Group D smoke check: fly +X automatically once the world has loaded
    bool walk = false;    // start in walk (gravity) mode; with --autofly, also asserts no fall-through
    bool noclip = false;  // Group AA: skip body-vs-world collision (the pre-collision spectator)
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
    // Micro-voxel (svo) options.
    app::SvoWorldOptions svo;
    render::diligent::SvoRenderer::Settings svo_settings;
};

// How long --verify-frame keeps waiting for the world to finish loading before declaring failure.
// Wall-clock, not a frame count: a loading-screen frame's cost is dominated by the world build,
// which varies with --radius / the svo region, so a frame-count budget has no fixed meaning.
constexpr std::chrono::seconds kVerifyLoadTimeout{600};

std::optional<AppOptions> parse_args(std::span<char*> args) {
    AppOptions options;
    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string_view arg = args[i];
        const auto next_value = [&]() -> const char* { return i + 1 < args.size() ? args[++i] : nullptr; };
        const auto next_float = [&](float fallback) {
            const char* value = next_value();
            return value ? static_cast<float>(std::strtod(value, nullptr)) : fallback;
        };
        const auto next_int = [&](int fallback) {
            const char* value = next_value();
            return value ? static_cast<int>(std::strtol(value, nullptr, 10)) : fallback;
        };
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
        } else if (arg == "--renderer") {
            const char* value = next_value();
            const std::string_view kind = value ? value : "";
            if (kind == "svo") {
                options.renderer = RendererKind::Svo;
            } else if (kind == "mesh") {
                options.renderer = RendererKind::Mesh;
            } else {
                log(LogLevel::Error, "--renderer expects svo|mesh, got \"{}\"", kind);
                return std::nullopt;
            }
        } else if (arg == "--frames") {
            const char* value = next_value();
            options.frames = value ? static_cast<std::uint32_t>(std::strtoul(value, nullptr, 10)) : 0;
        } else if (arg == "--radius") {
            options.radius = next_int(options.radius);
        } else if (arg == "--seed") {
            options.seed = next_int(options.seed);
        } else if (arg == "--verify-frame") {
            options.verify_frame = true;
        } else if (arg == "--validation") {
            options.validation = true;
        } else if (arg == "--autofly") {
            options.autofly = true;
        } else if (arg == "--walk") {
            options.walk = true;
        } else if (arg == "--noclip") {
            options.noclip = true;
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
        } else if (arg == "--no-shadows") {
            options.svo_settings.shadows = false;
        } else if (arg == "--no-ao") {
            options.svo_settings.ao = false;
        } else if (arg == "--no-lod-march") {
            options.svo_settings.lod_march = false;
        } else if (arg == "--lod-quality") {
            options.svo_settings.lod_quality = next_float(options.svo_settings.lod_quality);
        } else if (arg == "--no-grain") {
            options.svo_settings.grain = false;
        } else if (arg == "--no-taa") {
            options.svo_settings.taa = false;
        } else if (arg == "--smooth-pixels") {
            options.svo_settings.smooth_pixels = next_float(options.svo_settings.smooth_pixels);
        } else if (arg == "--grain") {
            options.svo_settings.grain_amplitude = next_float(options.svo_settings.grain_amplitude);
        } else if (arg == "--ao-radius") {
            options.svo_settings.ao_radius_px = next_float(options.svo_settings.ao_radius_px);
        } else if (arg == "--shadow-lod") {
            options.svo_settings.shadow_lod = next_float(options.svo_settings.shadow_lod);
        } else if (arg == "--svo-threads") {
            options.svo.worker_threads = static_cast<std::size_t>(std::max(0, next_int(0)));
        } else if (arg == "--svo-upload-mb") {
            options.svo_settings.upload_bytes_per_frame =
                static_cast<std::size_t>(std::max(1, next_int(32))) * std::size_t{1024} * 1024;
        } else if (arg == "--debug-view") {
            // Goal 165: one shading term per view, so a wrong frame names its cause.
            const char* value = next_value();
            const std::string_view view = value ? value : "";
            using render::diligent::SvoDebugView;
            static constexpr std::pair<std::string_view, SvoDebugView> kViews[] = {
                {"lit", SvoDebugView::Lit},
                {"ao", SvoDebugView::AO},
                {"normal", SvoDebugView::Normal},
                {"facenormal", SvoDebugView::FaceNormal},
                {"level", SvoDebugView::Level},
                {"steps", SvoDebugView::Steps},
                {"coverage", SvoDebugView::Coverage},
                {"cubepx", SvoDebugView::CubePixels},
                {"smooth", SvoDebugView::SmoothNormal},
                {"lodcube", SvoDebugView::LodCube},
                {"material", SvoDebugView::Material},
                {"distance", SvoDebugView::Distance},
            };
            bool found = false;
            for (const auto& [name, id] : kViews) {
                if (view == name) {
                    options.svo_settings.debug_view = id;
                    found = true;
                }
            }
            if (!found) {
                log(LogLevel::Error,
                    "--debug-view expects "
                    "lit|ao|normal|facenormal|level|steps|coverage|cubepx|smooth|lodcube|material|distance, "
                    "got \"{}\"",
                    view);
                return std::nullopt;
            }
            options.no_post = true; // raw values, not tone-mapped ones
        } else if (arg == "--voxel-log2") {
            options.svo.voxel_size_log2 = next_int(options.svo.voxel_size_log2);
        } else if (arg == "--region-log2") {
            options.svo.root_size_log2 = next_int(options.svo.root_size_log2);
        } else if (arg == "--lod-radius") {
            options.svo.lod_radius = next_float(options.svo.lod_radius);
        } else if (arg == "--no-trees") {
            options.svo.trees = false;
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
            options.start_yaw_deg = next_float(0.0f);
        } else if (arg == "--pitch") {
            options.start_pitch_deg = next_float(0.0f);
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
                "unknown argument \"{}\" (known: --mode vk|d3d12, --renderer svo|mesh, --frames N, --radius "
                "N, "
                "--seed N, --verify-frame, --validation, --autofly, --walk, --noclip, --upload-budget N, "
                "--dump-every "
                "N, "
                "--no-post/--no-bloom/--no-tonemap/--no-sky, --no-shadows, --no-ao, --no-lod-march, "
                "--lod-quality Q, --no-grain, --no-taa, --smooth-pixels N, --grain A, --ao-radius PX, "
                "--shadow-lod M, --svo-threads N, --svo-upload-mb N, --debug-view NAME, --voxel-log2 N, "
                "--region-log2 N, "
                "--lod-radius M, --no-trees, --pos x,y,z, --yaw D, --pitch D)",
                arg);
            return std::nullopt;
        }
    }
    options.svo.seed = options.seed;
    options.svo_settings.sky = !options.no_sky;
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
// the render pass consumes. `heightmap` is whichever world's analytic ground function is live --
// both world representations sample the same one, so walk mode is representation-agnostic.
// `collider` (Group AA) sweeps the camera's body against the world in BOTH modes -- fly mode
// stops at mountains and trunks too, walk mode additionally climbs ledges; null = --noclip.
render::interface::Camera
update_camera_phase(engine::ecs::Registry& registry, engine::ecs::Entity cameraEntity,
                    engine::input::GlfwInput& input, const world::generation::HeightmapGenerator& heightmap,
                    world::collision::TerrainCollider* collider, const engine::core::Clock& clock,
                    const AppOptions& options, std::uint32_t& walkViolations) {
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
    const float groundHeight = heightmap.height_at(transform.position.x, transform.position.z);
    const bool walking = spectator.mode == app::CameraMoveMode::Walk;
    if (collider != nullptr) {
        collider->refresh(transform.position);
        const app::SpectatorStep step =
            app::compute_spectator_step(transform, spectator, input.state(), input.take_look_delta(),
                                        static_cast<float>(clock.delta_seconds()), groundHeight);
        const glm::vec3 feet = transform.position - glm::vec3{0.0f, app::kEyeHeight, 0.0f};
        const world::collision::Aabb body =
            world::collision::Aabb::upright(feet, app::kBodyHalfWidth, app::kBodyHeight);
        world::collision::SweepParams sweep;
        sweep.step_height = walking ? app::kStepHeight : 0.0f;
        const world::collision::SweepResult moved =
            world::collision::move_and_slide(*collider, body, step.delta, sweep);
        transform.position += moved.delta;
        if (walking && (moved.grounded || (moved.blocked_y && step.delta.y > 0.0f))) {
            spectator.vertical_velocity = 0.0f; // landed, or bumped the head
        }
        if (walking) {
            app::clamp_to_ground(transform, spectator, groundHeight); // backstop; see spectator_camera.hpp
        }
    } else {
        app::update_spectator_camera(transform, spectator, input.state(), input.take_look_delta(),
                                     static_cast<float>(clock.delta_seconds()), groundHeight);
    }
    if (options.autofly) {
        // Constant sideways travel (at boost speed in fly mode; ground-bound in walk mode) --
        // goal 133's mechanical re-check: a static, fully-loaded world should show zero
        // generation-driven frame spikes crossing it, unlike the pre-redesign log's collapse to
        // 1-2 fps under the old per-tick streaming system.
        transform.position.x += (options.walk ? 20.0f : 160.0f) * static_cast<float>(clock.delta_seconds());
    }
    if (options.walk && walking) {
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

    void poll_budget(render::diligent::RenderContext& context) {
        if (std::chrono::steady_clock::now() - lastBudgetPoll >= std::chrono::seconds(2)) {
            const bool firstPoll = !budget.available;
            budget = render::diligent::query_gpu_memory_budget(context);
            lastBudgetPoll = std::chrono::steady_clock::now();
            if (firstPoll && budget.available) {
                log(LogLevel::Info,
                    "VK_EXT_memory_budget: {:.0f} MiB device-local budget, {:.0f} MiB in use machine-wide",
                    static_cast<double>(budget.device_local_budget_bytes) / (1024.0 * 1024.0),
                    static_cast<double>(budget.device_local_usage_bytes) / (1024.0 * 1024.0));
            }
        }
    }

    void smooth(const engine::core::Clock& clock) {
        const float dtMs = static_cast<float>(clock.delta_seconds()) * 1000.0f;
        smoothedFrameMs = smoothedFrameMs == 0.0f ? dtMs : smoothedFrameMs * 0.95f + dtMs * 0.05f;
    }

    void track_worst(const engine::core::Clock& clock) {
        const auto frameMs = static_cast<float>(clock.delta_seconds()) * 1000.0f;
        worstFrameMs = std::max(worstFrameMs, frameMs);
        worstFrameMsOverall = std::max(worstFrameMsOverall, frameMs);
    }

    [[nodiscard]] bool report_due() const noexcept {
        return std::chrono::steady_clock::now() - lastReport >= std::chrono::seconds(2);
    }
    [[nodiscard]] double report_fps() const {
        const double seconds =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - lastReport).count();
        return seconds > 0.0 ? framesSinceReport / seconds : 0.0;
    }
    void reset_report() {
        worstFrameMs = 0.0f;
        lastReport = std::chrono::steady_clock::now();
        framesSinceReport = 0;
    }
};

// Budget poll + overlay draw (pre-Present), mesh path.
void overlay_phase(FrameTelemetry& t, const engine::core::Clock& clock,
                   render::diligent::RenderContext& context, render::diligent::TerrainRenderer& renderer,
                   app::WorldLoader& world, render::diligent::DebugOverlay& overlay,
                   const ChunkEventCounters& chunkCounters, const render::interface::Camera& camera) {
    t.poll_budget(context);
    t.smooth(clock);
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

// The 2-second stats line + event/poll consistency check (post-Present), mesh path.
void report_phase(FrameTelemetry& t, const engine::core::Clock& clock,
                  render::diligent::TerrainRenderer& renderer, app::WorldLoader& world,
                  const ChunkEventCounters& chunkCounters) {
    t.track_worst(clock);
    if (!t.report_due()) {
        return;
    }
    log(LogLevel::Info, "{:.1f} fps (worst {:.1f} ms), {}/{} chunk meshes visible after culling, {} ready",
        t.report_fps(), t.worstFrameMs, renderer.last_visible_count(), renderer.chunk_count(),
        world.ready_chunk_count());
    if (chunkCounters.ready != world.ready_chunk_count()) {
        log(LogLevel::Error, "event/poll chunk-count mismatch: events say {}, loader says {}",
            chunkCounters.ready, world.ready_chunk_count());
    }
    t.reset_report();
}

// Verification readback + frame dumps + F2 screenshots (all pre-Present readbacks). Only ever
// called once the scene exists (verify-frame's whole premise is "does the finished scene look
// right"); `sceneReady` is the chunk count on the mesh path, 1/0 on the svo path.
struct CaptureState {
    bool verifyOk = false;
    bool verifyRan = false;
    std::uint32_t screenshotCounter = 0; // F2 capture numbering (goal 9)
};

bool capture_phase(CaptureState& cap, const AppOptions& options, std::uint32_t frame,
                   render::diligent::RenderContext& context, std::size_t sceneReady,
                   engine::input::GlfwInput& input) {
    if (options.verify_frame && !cap.verifyRan && sceneReady > 0) {
        const float fraction = render::diligent::sample_non_reference_pixel_fraction(context);
        // Local-contrast metric (see frame_verify.cpp for the two prior metrics it replaced and
        // why): terrain texture measures 12.3% at this pose, sky-only 0.9%. 6% keeps the
        // winding-bug lesson's discrimination -- sliver-band frames fail hard.
        // A debug view (one flat shading term) has no terrain texture to measure: it captures only.
        const bool debugView = options.svo_settings.debug_view != render::diligent::SvoDebugView::None;
        cap.verifyOk = debugView || fraction >= 0.06f;
        cap.verifyRan = true;
        log(cap.verifyOk ? LogLevel::Info : LogLevel::Error,
            "frame verification: {:.1f}% of pixels carry terrain-scale local contrast (threshold 6%) with "
            "scene readiness {}",
            static_cast<double>(fraction) * 100.0, sceneReady);
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

// Everything both renderer paths share: window, device, post chain, overlay, input, the camera
// entity, the frame clock.
struct Session {
    app::GlfwWindow window{1280, 720, "voxel_app"};
    std::unique_ptr<render::diligent::RenderContext> context;
    std::unique_ptr<render::diligent::PostProcessor> postProcess;
    engine::ecs::Registry registry;
    engine::ecs::Entity cameraEntity{};
    std::unique_ptr<engine::input::GlfwInput> input;
    std::unique_ptr<render::diligent::DebugOverlay> overlay;
    engine::core::Clock clock;
    glm::vec3 spawnPosition{0.0f};

    explicit Session(const AppOptions& options) {
        render::diligent::RenderContextCreateInfo contextCI;
        contextCI.backend = options.backend;
        contextCI.native_window_handle = window.native_handle();
        contextCI.enable_validation = options.validation;
        context = std::make_unique<render::diligent::RenderContext>(contextCI);

        // Post-process chain (Group D): bloom + tonemap over an offscreen HDR scene target.
        // Constructed BEFORE either renderer on purpose -- it registers the scene target whose
        // format their PSOs are created against. Skippable wholesale for A/B against the direct path.
        if (!options.no_post) {
            postProcess = std::make_unique<render::diligent::PostProcessor>(*context);
            postProcess->set_bloom_enabled(!options.no_bloom);
            postProcess->set_tonemap_enabled(!options.no_tonemap);
        }
        render::diligent::attach_gpu_profiler(
            *context); // Tracy GPU zones (Vulkan only; safe no-op elsewhere)

        // The camera is an ordinary ECS entity (Phase 1 brief §6): Transform + CameraLens are
        // engine components, SpectatorCameraState is this app's movement policy. Starts above the
        // terrain looking toward the origin; WASD + Space/Ctrl fly it, holding RMB mouse-looks,
        // Shift boosts, Esc quits.
        cameraEntity = registry.create();
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
        spawnPosition = transform.position;

        input = std::make_unique<engine::input::GlfwInput>(window.handle());
        // After GlfwInput: the overlay's ImGui GLFW backend chain-installs on top of input's
        // callbacks, so both receive events.
        overlay = std::make_unique<render::diligent::DebugOverlay>(*context, window.handle());
    }

    // Per-frame preamble shared by both loops: events, quit, clock, resize. Returns false to skip
    // the frame (minimized).
    bool begin_frame() {
        window.poll_events();
        clock.tick();
        const auto [width, height] = window.framebuffer_size();
        if (width == 0 || height == 0) {
            return false; // minimized -- nothing to render into
        }
        if (width != context->width() || height != context->height()) {
            context->resize(width, height);
        }
        return true;
    }
};

int run_mesh(Session& s, const AppOptions& options) {
    render::diligent::TerrainRenderer renderer(*s.context);
    renderer.set_sky_enabled(!options.no_sky);

    // Group S (Voxel Representation Redesign SS3): the world is static and bounded, pregenerated
    // once at startup instead of streamed around the camera. --radius maps directly to the
    // world's own horizontal half-size now, not a camera-relative load window.
    const world::streaming::WorldBounds bounds{options.radius, world::streaming::kDefaultWorldBounds.y_min,
                                               world::streaming::kDefaultWorldBounds.y_max};
    engine::events::Dispatcher dispatcher;
    ChunkEventCounters chunkCounters;
    chunkCounters.connect(dispatcher);
    app::WorldLoader world(bounds, options.seed, std::jthread::hardware_concurrency(), renderer, s.registry,
                           dispatcher, s.spawnPosition, options.upload_budget);
    world.begin();
    // Group AA: the mesh world is 1 m blocks, so the body collides against 1 m voxel columns.
    world::collision::TerrainColliderParams colliderParams;
    colliderParams.seed = options.seed;
    colliderParams.voxel_edge = 1.0f;
    world::collision::TerrainCollider collider(world.heightmap(), colliderParams);

    CaptureState cap;
    cap.verifyOk = !options.verify_frame;
    FrameTelemetry telemetry;
    std::uint32_t frame = 0;
    std::uint32_t walkViolations = 0; // frames ending below the ground surface in walk mode
    bool loggedReady = false;
    const auto loadStart = std::chrono::steady_clock::now();

    while (!s.window.should_close() && (options.frames == 0 || frame < options.frames)) {
        if (!s.begin_frame()) {
            continue;
        }
        if (s.input->state().quit_requested) {
            break;
        }

        if (!world.finished()) {
            // Goal 128/130: one increment of the one-time generation/mesh/upload pass, then a
            // real, moving loading-screen frame -- no interactive camera control, no capture/
            // verify logic, none of that is meaningful before the world exists to look at.
            world.pump();
            renderer.render(render::interface::Camera{}); // sky-only backdrop; zero chunks uploaded yet
            if (s.postProcess) {
                s.postProcess->execute(frame);
            }
            s.overlay->render_loading(world.ready_chunk_count(), world.total_chunk_count());
            // Goal 130's own check: a viewable capture of the loading screen mid-generation, using
            // the same --dump-every/VOXEL_DUMP_FRAME machinery the interactive phase's
            // capture_phase uses (not a second capture mechanism).
            if (options.dump_every > 0 && frame % options.dump_every == 0) {
                char dumpPath[64];
                std::snprintf(dumpPath, sizeof(dumpPath), "loading_%05u.png", frame);
                (void)render::diligent::dump_frame(*s.context, dumpPath);
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
                world.log_timings();
                loggedReady = true;
            }
            const render::interface::Camera camera =
                update_camera_phase(s.registry, s.cameraEntity, *s.input, world.heightmap(),
                                    options.noclip ? nullptr : &collider, s.clock, options, walkViolations);

            renderer.render(camera);
            if (s.postProcess) {
                s.postProcess->execute(frame);
            }
            overlay_phase(telemetry, s.clock, *s.context, renderer, world, *s.overlay, chunkCounters, camera);
            capture_phase(cap, options, frame, *s.context, world.ready_chunk_count(), *s.input);
            report_phase(telemetry, s.clock, renderer, world, chunkCounters);
        }

        s.context->present();
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
        render::diligent::to_string(s.context->backend()));
    return cap.verifyOk && autoflyOk && walkOk ? EXIT_SUCCESS : EXIT_FAILURE;
}

// The micro-voxel path (docs/goals.md Group X): no chunks, no meshes -- a SvoWorld builds
// sparse-brick octrees around the camera on a background thread, SvoRenderer marches them.
int run_svo(Session& s, const AppOptions& options) {
    render::diligent::SvoRenderer renderer(*s.context);
    renderer.set_settings(options.svo_settings);
    app::SvoWorld world(options.svo);
    // Group AA: the body collides against the same voxelization rule the tree is sampled with, at
    // the tree's finest voxel, over a cached height grid -- independent of the renderer's LOD.
    world::collision::TerrainColliderParams colliderParams;
    colliderParams.seed = options.seed;
    colliderParams.voxel_edge = world.geometry_for(s.spawnPosition).finest_voxel_edge();
    colliderParams.trees = options.svo.trees;
    world::collision::TerrainCollider collider(world.heightmap(), colliderParams);
    collider.refresh(s.spawnPosition); // the first cache is built synchronously: pay it here, not mid-frame

    CaptureState cap;
    cap.verifyOk = !options.verify_frame;
    FrameTelemetry telemetry;
    std::uint32_t frame = 0;
    std::uint32_t walkViolations = 0;
    std::size_t uploads = 0;
    double lastUploadMs = 0.0;
    const auto loadStart = std::chrono::steady_clock::now();
    const world::svo::TreeGeometry geometry = world.geometry_for(s.spawnPosition);
    log(LogLevel::Info,
        "svo: {} m region, {:.4g} mm voxels near the camera ({} levels, finest LOD ring {:.1f} m), trees {}",
        geometry.root_edge(), static_cast<double>(geometry.finest_voxel_edge()) * 1000.0,
        geometry.max_brick_level() + 1, options.svo.lod_radius, options.svo.trees ? "on" : "off");
    world.request_build(s.spawnPosition);

    // Goal 170's measurement: where a slow frame's time went, attributed per phase, so "the lag"
    // is a number with a cause rather than a feeling. A frame over kSlowFrameMs is logged with
    // its breakdown; the exit summary counts them by what was happening.
    constexpr double kSlowFrameMs = 20.0;
    struct FramePhases {
        double upload = 0.0; // begin_upload + pump_upload (buffer creation, a slice's UpdateBuffer)
        double camera = 0.0; // input, collision (incl. a synchronous cache refresh), rebuild request
        double render = 0.0; // SvoRenderer::render (CPU side: constants, draws recorded)
        double post = 0.0;
        double overlay = 0.0;
        double present = 0.0;
        bool swapped = false;   // the whole tree landed and was swapped in this frame
        bool uploading = false; // a slice was copied this frame
        bool building = false;  // a background build was running
        bool refreshed = false; // the collider's height cache was rebuilt synchronously
    };
    FramePhases prevPhases;
    struct SlowFrames {
        std::uint32_t total = 0;
        std::uint32_t whileUploading = 0;
        std::uint32_t onSwap = 0;
        std::uint32_t whileBuilding = 0;
        std::uint32_t other = 0;
    } slow;
    const auto phase_ms = [](std::chrono::steady_clock::time_point& t) {
        const auto now = std::chrono::steady_clock::now();
        const double ms = std::chrono::duration<double, std::milli>(now - t).count();
        t = now;
        return ms;
    };

    // Hands a finished build to the renderer's staged upload, and pumps that upload one slice per
    // frame; logs the tree the frame it lands.
    const auto adopt_finished = [&]() {
        if (std::optional<world::svo::BrickTree> tree = world.take_finished()) {
            renderer.begin_upload(std::move(*tree));
        }
        if (!renderer.pump_upload()) {
            return false;
        }
        lastUploadMs = renderer.last_upload_ms();
        ++uploads;
        const app::SvoWorld::LastBuild last = world.last_build();
        log(LogLevel::Info,
            "svo tree #{}: {} bricks, {} internal, {} solid leaves, {:.1f} MB, build {:.2f}s (sampler "
            "{:.2f}s, {} classified, {} bricks sampled), staged upload {:.1f} ms over {} frames, {} trees",
            uploads, last.bricks, last.tree.internal_nodes, last.tree.solid_leaves,
            static_cast<double>(last.memory_bytes) / 1.0e6, last.stats.seconds, last.sampler_seconds,
            last.stats.boxes_classified, last.stats.bricks_sampled, lastUploadMs,
            renderer.last_upload_frames(), last.trees);
        return true;
    };

    while (!s.window.should_close() && (options.frames == 0 || frame < options.frames)) {
        if (!s.begin_frame()) {
            continue;
        }
        if (s.input->state().quit_requested) {
            break;
        }
        // The clock's delta is the PREVIOUS frame's duration: attribute it to that frame's phases.
        if (frame > 0 && renderer.has_tree()) {
            const double frameMs = s.clock.delta_seconds() * 1000.0;
            if (frameMs > kSlowFrameMs) {
                ++slow.total;
                if (prevPhases.swapped) {
                    ++slow.onSwap;
                } else if (prevPhases.uploading) {
                    ++slow.whileUploading;
                } else if (prevPhases.building) {
                    ++slow.whileBuilding;
                } else {
                    ++slow.other;
                }
                log(LogLevel::Warn,
                    "slow frame {}: {:.1f} ms = upload {:.1f} + camera {:.1f} + render {:.1f} + post {:.1f} "
                    "+ "
                    "overlay {:.1f} + present {:.1f}{}{}{}{}",
                    frame - 1, frameMs, prevPhases.upload, prevPhases.camera, prevPhases.render,
                    prevPhases.post, prevPhases.overlay, prevPhases.present,
                    prevPhases.swapped ? " [tree swapped]" : "", prevPhases.uploading ? " [uploading]" : "",
                    prevPhases.building ? " [building]" : "",
                    prevPhases.refreshed ? " [cache refreshed]" : "");
            }
        }
        FramePhases phases;
        auto phaseClock = std::chrono::steady_clock::now();
        phases.uploading = renderer.upload_pending() || world.take_finished_pending();
        phases.building = world.building();
        phases.swapped = adopt_finished();
        phases.upload = phase_ms(phaseClock);

        if (!renderer.has_tree()) {
            // Loading screen until the first tree lands: sky only, no camera control.
            renderer.render(render::interface::Camera{});
            if (s.postProcess) {
                s.postProcess->execute(frame);
            }
            const double elapsed =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - loadStart).count();
            s.overlay->render_loading_message("Building sparse voxel tree...", elapsed);
            if (options.verify_frame && std::chrono::steady_clock::now() - loadStart > kVerifyLoadTimeout) {
                log(LogLevel::Error, "frame verification: svo tree never finished within {}s",
                    kVerifyLoadTimeout.count());
                break;
            }
        } else {
            const double refreshBefore = collider.last_refresh_ms();
            const render::interface::Camera camera =
                update_camera_phase(s.registry, s.cameraEntity, *s.input, world.heightmap(),
                                    options.noclip ? nullptr : &collider, s.clock, options, walkViolations);
            phases.refreshed = collider.last_refresh_ms() != refreshBefore && !collider.refresh_pending();
            // Rebuild once the camera has left the inner half of the finest LOD ring: the tree is
            // still correct everywhere (coarser rings are conservative), just not at full detail
            // right around the camera until the new one lands.
            if (!world.building() &&
                world.distance_from_build_center(camera.position) > options.svo.lod_radius * 0.5f) {
                world.request_build(camera.position);
            }
            phases.camera = phase_ms(phaseClock);

            renderer.render(camera);
            phases.render = phase_ms(phaseClock);
            if (s.postProcess) {
                s.postProcess->execute(frame);
            }
            phases.post = phase_ms(phaseClock);

            telemetry.poll_budget(*s.context);
            telemetry.smooth(s.clock);
            render::diligent::OverlayStats stats;
            stats.fps = telemetry.smoothedFrameMs > 0.0f ? 1000.0f / telemetry.smoothedFrameMs : 0.0f;
            stats.frame_ms = telemetry.smoothedFrameMs;
            const app::SvoWorld::LastBuild last = world.last_build();
            stats.svo.active = true;
            stats.svo.bricks = last.bricks;
            stats.svo.internal_nodes = last.tree.internal_nodes;
            stats.svo.solid_leaves = last.tree.solid_leaves;
            stats.svo.memory_bytes = last.memory_bytes;
            stats.svo.build_seconds = last.stats.seconds;
            stats.svo.upload_ms = lastUploadMs;
            stats.svo.gpu_ms = renderer.last_gpu_ms();
            stats.svo.building = world.building();
            stats.svo.voxel_mm = static_cast<double>(geometry.finest_voxel_edge()) * 1000.0;
            stats.svo.levels = geometry.max_brick_level() + 1;
            stats.svo.trees = last.trees;
            stats.svo.uploads = uploads;
            const glm::vec3 aimDir = camera.orientation * glm::vec3(0.0f, 0.0f, -1.0f);
            const app::AimHit aim = app::query_aim(world.heightmap(), camera.position, aimDir);
            if (aim.hit) {
                std::snprintf(stats.aim_line, sizeof(stats.aim_line), "%s @ %.0f,%.0f,%.0f",
                              app::material_name(aim.material), static_cast<double>(aim.position.x),
                              static_cast<double>(aim.position.y), static_cast<double>(aim.position.z));
            }
            stats.gpu_self_bytes = renderer.gpu_memory().allocated_bytes();
            stats.gpu_self_peak_bytes = renderer.gpu_memory().peak_bytes();
            stats.budget = telemetry.budget;
            s.overlay->render(stats);
            phases.overlay = phase_ms(phaseClock);

            capture_phase(cap, options, frame, *s.context, 1, *s.input);

            telemetry.track_worst(s.clock);
            if (telemetry.report_due()) {
                log(LogLevel::Info,
                    "{:.1f} fps (worst {:.1f} ms, gpu {:.2f} ms), svo {} bricks / {:.1f} MB{}",
                    telemetry.report_fps(), telemetry.worstFrameMs, renderer.last_gpu_ms(), last.bricks,
                    static_cast<double>(last.memory_bytes) / 1.0e6, world.building() ? ", rebuilding" : "");
                telemetry.reset_report();
            }
        }

        phaseClock = std::chrono::steady_clock::now();
        s.context->present();
        phases.present = phase_ms(phaseClock);
        prevPhases = phases;
        FrameMark;
        ++frame;
        ++telemetry.framesSinceReport;

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
        autoflyOk = renderer.has_tree();
        log(autoflyOk ? LogLevel::Info : LogLevel::Error,
            "autofly: {} svo uploads, worst frame {:.1f} ms over the whole run, {:.1f} MiB GPU (peak {:.1f})",
            uploads, static_cast<double>(telemetry.worstFrameMsOverall),
            static_cast<double>(renderer.gpu_memory().allocated_bytes()) / (1024.0 * 1024.0),
            static_cast<double>(renderer.gpu_memory().peak_bytes()) / (1024.0 * 1024.0));
    }
    log(LogLevel::Info,
        "slow frames (> {:.0f} ms): {} of {} -- {} on a tree swap, {} while uploading a slice, {} while only "
        "building, {} other",
        kSlowFrameMs, slow.total, frame, slow.onSwap, slow.whileUploading, slow.whileBuilding, slow.other);
    log(LogLevel::Info, "exiting after {} frames on {}", frame,
        render::diligent::to_string(s.context->backend()));
    return cap.verifyOk && autoflyOk && walkOk ? EXIT_SUCCESS : EXIT_FAILURE;
}

int run(const AppOptions& options) {
    Session session(options);
    return options.renderer == RendererKind::Svo ? run_svo(session, options) : run_mesh(session, options);
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
