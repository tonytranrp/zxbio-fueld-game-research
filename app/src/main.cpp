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
#include "app_options.hpp"
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
#include "world/player/fixed_step.hpp"
#include "world/player/view_polish.hpp"
#include "world/streaming/chunk_events.hpp"
#include "world/streaming/world_bounds.hpp"
#include "world/water/gerstner.hpp"
#include "world/wind/wind_field.hpp"
#include "world_loader.hpp"

#if defined(TRACY_ENABLE)
#include <tracy/Tracy.hpp>
#else
#define FrameMark
#endif

namespace {

using engine::core::log;
using engine::core::LogLevel;

using app::AppOptions;
using app::RendererKind;

// How long --verify-frame keeps waiting for the world to finish loading before declaring failure.
// Wall-clock, not a frame count: a loading-screen frame's cost is dominated by the world build,
// which varies with --radius / the svo region, so a frame-count budget has no fixed meaning.
constexpr std::chrono::seconds kVerifyLoadTimeout{600};

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
                    const AppOptions& options, std::uint32_t& walkViolations,
                    world::player::FixedStepper& stepper, float& viewOffsetY,
                    const world::water::WaveField& waveField, float waveTime) {
    auto [transform, lens, spectator] =
        registry.get<engine::ecs::Transform, engine::ecs::CameraLens, app::SpectatorCameraState>(
            cameraEntity);
    if (input.take_walk_toggle()) {
        // Deliberate transition handling (Group V task 25): position is untouched, vertical
        // velocity zeroed -- entering walk mid-air simply starts a clean fall; leaving it
        // freezes wherever you are. No snap in either direction.
        spectator.physics.mode = spectator.physics.mode == app::CameraMoveMode::Fly
                                     ? app::CameraMoveMode::Walk
                                     : app::CameraMoveMode::Fly;
        spectator.physics.vertical_velocity = 0.0f;
        log(LogLevel::Info, "camera mode: {}",
            spectator.physics.mode == app::CameraMoveMode::Walk ? "walk" : "fly");
    }

    // Mouse look runs at RENDER cadence: a 144 Hz mouse must not be sampled at the sim's 60 Hz.
    app::apply_look(transform, spectator, input.take_look_delta());

    const bool walking = spectator.physics.mode == app::CameraMoveMode::Walk;
    // The jump press is an EDGE, taken once per frame and handed to exactly one fixed tick below
    // (world/player's contract: holding Space must not re-arm the buffer every tick).
    bool jumpEdge = input.take_jump();

    // Prompt 001 A1: the simulation runs at a fixed 60 Hz regardless of frame rate, so the jump
    // apex, the coyote window and the smoothing time constant mean the same thing on every machine.
    const int ticks = stepper.begin_frame(clock.delta_seconds());
    const float dt = stepper.step_seconds_f();
    for (int i = 0; i < ticks; ++i) {
        if (options.autofly) {
            // Constant sideways travel (at boost speed in fly mode; ground-bound in walk mode) --
            // goal 133's mechanical re-check: a static, fully-loaded world should show zero
            // generation-driven frame spikes crossing it, unlike the pre-redesign log's collapse
            // to 1-2 fps under the old per-tick streaming system.
            //
            // Applied INSIDE the tick, before the physics that has to answer for it. It used to be
            // a per-frame teleport after the physics, which stopped being equivalent the moment the
            // sim went fixed-step: at 150 fps most frames run zero ticks, so the body was shoved
            // into a new column with nothing to settle it, and a 900-frame walk reported 74 ground
            // violations that were the harness's, not the world's.
            transform.position.x += (options.walk ? 20.0f : 160.0f) * dt;
        }
        const float groundHeight = heightmap.height_at(transform.position.x, transform.position.z);
        world::player::WorldSense sense;
        sense.ground_height = groundHeight;
        // E2: the swimmer rides the ACTUAL surface, not a constant sea level -- the same Gerstner
        // sum the marcher draws, evaluated on the CPU from the same field. Only where there is sea
        // to swim in: over land the wave height is meaningless and would shift the water plane the
        // buoyancy test uses.
        if (groundHeight < world::player::kSeaLevelWorld) {
            sense.water_surface_y =
                world::player::kSeaLevelWorld +
                world::water::wave_height(waveField, transform.position.x, transform.position.z, waveTime);
        }
        const world::player::PlayerIntent intent = app::to_intent(input.state(), jumpEdge);
        jumpEdge = false; // consumed by the first tick of this frame

        const glm::vec3 before = transform.position;
        world::player::StepResult step;
        if (collider != nullptr) {
            collider->refresh(transform.position);
            step = app::step_camera(transform, spectator, *collider, intent, sense, dt);
        } else {
            const app::OpenWorld open; // --noclip: nothing is solid, the backstop still applies
            step = app::step_camera(transform, spectator, open, intent, sense, dt);
        }

        // A6: render-only feel. Off under the mechanical checks so --autofly/--verify-frame keep
        // measuring exactly the body the physics moved.
        world::player::ViewPolishSense polish;
        const glm::vec3 travelled = transform.position - before;
        polish.horizontal_speed = glm::length(glm::vec2{travelled.x, travelled.z}) / dt;
        polish.boosting = input.state().speed_boost;
        const bool polishOn = options.view_polish && !options.autofly && !options.verify_frame;
        viewOffsetY = world::player::update_view_polish(spectator.physics, spectator.tuning, polish, step,
                                                        polishOn, dt);

        if (options.walk && walking) {
            // Group V task 27's mechanical check, now evaluated every fixed TICK rather than every
            // frame (strictly more often, and at a cadence that does not vary with load): the
            // camera must never end an update below the ground surface.
            if (transform.position.y < groundHeight + app::kEyeHeight - 0.01f) {
                ++walkViolations;
            }
        }
    }
    // A frame shorter than one tick simply renders the pose (and the polish offset) it already had.

    render::interface::Camera camera;
    camera.position = transform.position;
    // A3/A6: the rendered eye lags the physical one (voxel-stair smoothing) and carries the polish
    // offset. The BODY is never moved by either -- the walk-violation counter above, --autofly and
    // --verify-frame all measure transform.position, not this.
    camera.position.y += spectator.physics.eye_smooth_offset + viewOffsetY;
    camera.orientation = transform.orientation;
    camera.fov_y_radians = lens.fov_y_radians + world::player::view_polish_fov_offset(spectator.physics);
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
                   const ChunkEventCounters& chunkCounters, const render::interface::Camera& camera,
                   const app::TreeLookup& trees, bool crosshair, const world::wind::WindParams& wind,
                   float windTime) {
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
    // Goal 84 + Prompt 001 A4: what the crosshair (view center) is aiming at -- analytic ray march,
    // now including trees and the hit distance.
    const glm::vec3 aimDir = camera.orientation * glm::vec3(0.0f, 0.0f, -1.0f);
    const app::AimHit aim = app::query_aim(world.heightmap(), camera.position, aimDir, 300.0f, &trees);
    if (aim.hit) {
        std::snprintf(stats.aim_line, sizeof(stats.aim_line), "%s @ %.0f,%.0f,%.0f (%.0f m)",
                      app::material_name(aim.material), static_cast<double>(aim.position.x),
                      static_cast<double>(aim.position.y), static_cast<double>(aim.position.z),
                      static_cast<double>(aim.distance));
    }
    stats.gpu_self_bytes = renderer.gpu_memory().allocated_bytes();
    stats.gpu_self_peak_bytes = renderer.gpu_memory().peak_bytes();
    stats.budget = t.budget;
    stats.crosshair = crosshair;
    const world::wind::WindSample windHere = world::wind::sample_wind(wind, camera.position, windTime);
    stats.wind_speed = windHere.speed;
    stats.wind_gust = windHere.gust;
    stats.wind_angle_deg = glm::degrees(wind.base_angle_radians);
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
        // VOXEL_DUMP_FRAME, when set, gives the numbered dumps a DIRECTORY as well as a name.
        // Without it these are bare relative paths resolved against whatever the process's working
        // directory happens to be, and the write can fail there with nothing to show for it --
        // which is how a --dump-every capture session produced no files at all and no message
        // saying so. The result is logged now either way.
        char dumpPath[512];
        // getenv_s, not getenv: MSVC treats the latter as a deprecation error under /WX (the same
        // reason frame_verify.cpp reads this variable the same way).
        char base[256] = {};
        std::size_t baseLen = 0;
        const bool haveBase = getenv_s(&baseLen, base, sizeof(base), "VOXEL_DUMP_FRAME") == 0 && baseLen > 1;
        if (haveBase) {
            const std::string_view whole{base};
            const std::size_t dot = whole.rfind('.');
            const std::string_view stem = dot == std::string_view::npos ? whole : whole.substr(0, dot);
            std::snprintf(dumpPath, sizeof(dumpPath), "%.*s_%05u.png", static_cast<int>(stem.size()),
                          stem.data(), frame);
        } else {
            std::snprintf(dumpPath, sizeof(dumpPath), "frame_%05u.png", frame);
        }
        const bool dumped = render::diligent::dump_frame(context, dumpPath);
        log(dumped ? LogLevel::Info : LogLevel::Error, "frame dump: {} ({})", dumpPath,
            dumped ? "written" : "FAILED");
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
        if (options.post) {
            postProcess = std::make_unique<render::diligent::PostProcessor>(*context);
            postProcess->set_bloom_enabled(options.bloom);
            postProcess->set_tonemap_enabled(options.tonemap);
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
            spectator.physics.mode = app::CameraMoveMode::Walk; // starts mid-air and falls to the ground
        }
        // A3: the svo path's step allowance is a smoothing budget, not a ledge climb.
        spectator.tuning.step_height = options.renderer == RendererKind::Svo
                                           ? world::player::kSvoStepHeight
                                           : world::player::kDefaultTuning.step_height;
        if (options.step_height) {
            spectator.tuning.step_height = *options.step_height;
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
    renderer.set_sky_enabled(options.sky);
    // C7: --wind-speed / --no-wind mean the same thing on both renderer paths.
    renderer.set_wind(options.svo_settings.wind);

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
    std::uint32_t walkViolations = 0; // ticks ending below the ground surface in walk mode
    // Prompt 001 A1: the simulation's own clock, carried across frames (never a frame-loop
    // local), plus the render-only view-polish offset it produces.
    world::player::FixedStepper stepper;
    float viewOffsetY = 0.0f;
    // A4: the aim query's tree source, kept across frames so its per-column placement cache is
    // built once rather than per frame. Off under the mechanical frame checks, along with the
    // crosshair, so --verify-frame's contrast metric measures the world and not the HUD.
    const app::TreeLookup aimTrees(world.heightmap(), options.seed);
    // E2: the swimmer's surface. Derived once from the wind, like the renderer's copy -- one field,
    // two readers, rather than two derivations that agree only approximately.
    const world::water::WaveField waveField = world::water::make_wave_field(options.svo_settings.wind);
    const bool crosshairOn = options.crosshair.value_or(!options.verify_frame);
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
            const float waveTime = static_cast<float>(s.clock.elapsed_seconds());
            const render::interface::Camera camera = update_camera_phase(
                s.registry, s.cameraEntity, *s.input, world.heightmap(), options.noclip ? nullptr : &collider,
                s.clock, options, walkViolations, stepper, viewOffsetY, waveField, waveTime);

            renderer.render(camera);
            if (s.postProcess) {
                s.postProcess->execute(frame);
            }
            overlay_phase(telemetry, s.clock, *s.context, renderer, world, *s.overlay, chunkCounters, camera,
                          aimTrees, crosshairOn, options.svo_settings.wind, waveTime);
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
    world::player::FixedStepper stepper;
    float viewOffsetY = 0.0f;
    // A4: the aim query's tree source, kept across frames so its per-column placement cache is
    // built once rather than per frame. Off under the mechanical frame checks, along with the
    // crosshair, so --verify-frame's contrast metric measures the world and not the HUD.
    const app::TreeLookup aimTrees(world.heightmap(), options.seed);
    // E2: the swimmer's surface. Derived once from the wind, like the renderer's copy -- one field,
    // two readers, rather than two derivations that agree only approximately.
    const world::water::WaveField waveField = world::water::make_wave_field(options.svo_settings.wind);
    const bool crosshairOn = options.crosshair.value_or(!options.verify_frame);
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
            // The renderer's OWN animation clock, so the swimmer rides the surface being drawn
            // rather than one that agrees with it only approximately.
            const float waveTime = renderer.anim_seconds();
            const render::interface::Camera camera = update_camera_phase(
                s.registry, s.cameraEntity, *s.input, world.heightmap(), options.noclip ? nullptr : &collider,
                s.clock, options, walkViolations, stepper, viewOffsetY, waveField, waveTime);
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
            const app::AimHit aim =
                app::query_aim(world.heightmap(), camera.position, aimDir, 300.0f, &aimTrees);
            if (aim.hit) {
                std::snprintf(stats.aim_line, sizeof(stats.aim_line), "%s @ %.0f,%.0f,%.0f (%.0f m)",
                              app::material_name(aim.material), static_cast<double>(aim.position.x),
                              static_cast<double>(aim.position.y), static_cast<double>(aim.position.z),
                              static_cast<double>(aim.distance));
            }
            stats.gpu_self_bytes = renderer.gpu_memory().allocated_bytes();
            stats.gpu_self_peak_bytes = renderer.gpu_memory().peak_bytes();
            stats.budget = telemetry.budget;
            stats.crosshair = crosshairOn;
            const world::wind::WindSample windHere =
                world::wind::sample_wind(options.svo_settings.wind, camera.position, renderer.anim_seconds());
            stats.wind_speed = windHere.speed;
            stats.wind_gust = windHere.gust;
            stats.wind_angle_deg = glm::degrees(options.svo_settings.wind.base_angle_radians);
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
    // Everything is inside the try, including argument parsing, and there is a catch-all: escaping
    // main is std::terminate, which loses the message. The crash handler below reports what it can
    // for the failures it hooks, but a thrown exception that never reaches a catch is not one of
    // them. (clang-tidy's bugprone-exception-escape; the same fix tools/svo_render got.)
    try {
        app::install_crash_handler();
        // Unbuffered stdout: when output is redirected to a file (smoke runs, CI), full buffering
        // would otherwise eat the final log lines -- including exception reports -- if the process
        // dies without flushing. Cost is irrelevant at this log volume.
        std::setvbuf(stdout, nullptr, _IONBF, 0);
        AppOptions options;
        const engine::cli::ParseOutcome parsed = app::parse_app_options(argc, argv, options);
        if (!parsed.ok) {
            log(LogLevel::Error, "{}", parsed.message);
            std::fputs("\n", stderr);
            std::fputs(app::app_help_text().c_str(), stderr);
            return EXIT_FAILURE;
        }
        if (parsed.help_requested) {
            std::fputs(app::app_help_text().c_str(), stdout);
            return EXIT_SUCCESS;
        }
#ifndef NDEBUG
        // Group J task 20's check: deliberately exercise each crash-handler hook. Debug-only by
        // construction -- the flag does not exist in a release build's table. It fires AFTER the
        // parse now rather than inside it, which is what lets the option be a row like any other.
        if (!options.crash_test.empty()) {
            log(LogLevel::Info, "crash-test: triggering \"{}\"", options.crash_test);
            if (options.crash_test == "av") {
                int* p = nullptr;
                *p = 42; // NOLINT(clang-analyzer-core.NullDereference) -- the point of the test
            } else if (options.crash_test == "abort") {
                std::abort();
            } else if (options.crash_test == "terminate") {
                std::terminate();
            }
            log(LogLevel::Error, "--crash-test expects av|abort|terminate, got \"{}\"", options.crash_test);
            return EXIT_FAILURE;
        }
#endif
        return run(options);
    } catch (const std::exception& e) {
        // fprintf, not log(): a handler in main must not itself be able to throw, and log()
        // formats. tools/svo_render's main reports the same way for the same reason.
        std::fprintf(stderr, "fatal: %s\n", e.what());
        return EXIT_FAILURE;
    } catch (...) {
        std::fprintf(stderr, "fatal: unknown exception\n");
        return EXIT_FAILURE;
    }
}
