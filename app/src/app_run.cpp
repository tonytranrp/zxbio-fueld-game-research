#include "app_run.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <format>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

#include "aim_query.hpp"
#include "dev/telemetry/frame_record.hpp"
#include "dev/telemetry/frame_report.hpp"
#include "engine/core/clock.hpp"
#include "engine/core/log.hpp"
#include "engine/core/math.hpp"
#include "engine/ecs/components.hpp"
#include "engine/ecs/registry.hpp"
#include "engine/input/glfw_input.hpp"
#include "engine/jobs/thread_pool.hpp"
#include "render/diligent/auto_exposure.hpp"
#include "render/diligent/debug_overlay.hpp"
#include "render/diligent/frame_verify.hpp"
#include "render/diligent/gpu_passes.hpp"
#include "render/diligent/gpu_tools.hpp"
#include "render/diligent/post_process.hpp"
#include "render/diligent/render_context.hpp"
#include "render/diligent/renderdoc_trigger.hpp"
#include "render/diligent/svo_renderer.hpp"
#include "render/diligent/terrain_renderer.hpp"
#include "render/interface/camera.hpp"
#include "spectator_camera.hpp"
#include "svo_world.hpp"

#include "world/svo/cell_marks.hpp"
#include "world/collision/aabb_sweep.hpp"
#include "world/collision/octree_collider.hpp"
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

namespace app {

namespace {

using dev::telemetry::FrameCauses;
using dev::telemetry::FrameCounters;
using dev::telemetry::FramePhases;
using dev::telemetry::FrameRecord;
using engine::core::log;
using engine::core::LogLevel;

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

// Is the body standing on the ground right now? The `capture event grounded` point needs it, and
// so does the walk-violation accounting; reading it from the ECS keeps the frame loop from having
// to thread a second out-parameter through update_camera_phase.

// Goal 230: what does asking the octree actually cost per tick? Accumulated in nanoseconds around
// the ONE call that does all of it -- step_camera plus the counter's own overlaps_solid -- because
// a budget stated per tick has to be measured per tick, not inferred from a frame phase whose
// printed resolution is 0.1 ms and which also contains the input, the look and the wave field.
struct CollisionCost {
    std::uint64_t nanos = 0;
    std::uint64_t ticks = 0;
    std::uint64_t worst_nanos = 0;
    std::uint64_t queries = 0;     // overlaps_solid calls
    std::uint64_t node_visits = 0; // octree nodes descended across them

    [[nodiscard]] double mean_ms() const noexcept {
        return ticks == 0 ? 0.0 : static_cast<double>(nanos) / static_cast<double>(ticks) / 1.0e6;
    }
    [[nodiscard]] double worst_ms() const noexcept { return static_cast<double>(worst_nanos) / 1.0e6; }
};
[[nodiscard]] bool spectator_grounded(engine::ecs::Registry& registry, engine::ecs::Entity camera) {
    return registry.get<app::SpectatorCameraState>(camera).physics.stance == world::player::Stance::Grounded;
}

// ---- per-frame phases (goal 50: run() split into independently-readable pieces) ----------------

// Input handling + camera movement + walk-mode invariant accounting. Returns the camera snapshot
// the render pass consumes. `heightmap` is whichever world's analytic ground function is live --
// both world representations sample the same one, so walk mode is representation-agnostic.
// `query` sweeps the camera's body against the world in BOTH modes -- fly mode stops at mountains
// and trunks too, walk mode additionally climbs ledges; null = --noclip.
//
// TEMPLATED on the query (Prompt 003 goal 227), because the two renderer paths now collide against
// two different worlds: the mesh path against the analytic TerrainCollider's 1 m voxel columns, and
// the svo path against the OctreeCollider -- the SAME immutable tree the marcher is drawing, which
// is what removes the cache edge a fast camera used to outrun. Two instantiations in one TU, both
// zero-overhead; a virtual BodyQuery would have put a call in the sweep's innermost loop for no
// benefit, since the set of query types is closed and known here.
template <world::collision::SolidQuery Q>
render::interface::Camera update_camera_phase(
    engine::ecs::Registry& registry, engine::ecs::Entity cameraEntity, FrameInput& input,
    const world::generation::HeightmapGenerator& heightmap, Q* query, const engine::core::Clock& clock,
    const AppOptions& options, std::uint32_t& walkViolations, world::player::FixedStepper& stepper,
    float& viewOffsetY, const world::water::WaveField& waveField, float waveTime,
    std::uint32_t& insideSolidEvents, std::uint32_t& insideSolidStartedInside,
    std::uint32_t& insideSolidSteppedUp, CollisionCost& collisionCost, std::uint32_t& stanceChanges,
    std::array<std::uint32_t, 9>& stanceTransitions, float& smoothOffsetY) {
    auto [transform, lens, spectator] =
        registry.get<engine::ecs::Transform, engine::ecs::CameraLens, app::SpectatorCameraState>(
            cameraEntity);
    // The G toggle is a DEV tool now (goal 231): one door, not four.
    if (options.dev && input.take_walk_toggle()) {
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
    // A SCRIPTED look turns at the fixed timestep instead and leaves this zero -- both paths go
    // through the same apply_look, so a scenario's turn and a player's turn are the same code.
    apply_look(transform, spectator, input.take_look_delta());

    const bool walking = spectator.physics.mode == app::CameraMoveMode::Walk;
    // The jump press is an EDGE, taken once per frame and handed to exactly one fixed tick below
    // (world/player's contract: holding Space must not re-arm the buffer every tick).
    bool jumpEdge = true; // FrameInput::tick() decides whether the frame actually saw a press

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
            transform.position.x +=
                (spectator.physics.mode == app::CameraMoveMode::Walk ? 20.0f : 160.0f) * dt;
        }
        const float groundHeight = heightmap.height_at(transform.position.x, transform.position.z);
        world::player::WorldSense sense;
        sense.ground_height = groundHeight;
        // Goal 235: the macroscopic slope, by central difference on the ANALYTIC field. One metre
        // is the right probe distance -- far enough to skip the 7.8 mm staircase the body actually
        // collides with, near enough to follow a hill rather than average it away. Four extra
        // `height_at` calls per tick; the analytic sampler is FastNoise, not an octree descent.
        {
            constexpr float kProbe = 1.0f;
            const float hx = heightmap.height_at(transform.position.x + kProbe, transform.position.z) -
                             heightmap.height_at(transform.position.x - kProbe, transform.position.z);
            const float hz = heightmap.height_at(transform.position.x, transform.position.z + kProbe) -
                             heightmap.height_at(transform.position.x, transform.position.z - kProbe);
            const glm::vec2 gradient{hx / (2.0f * kProbe), hz / (2.0f * kProbe)};
            const float steepness = glm::length(gradient);
            sense.ground_slope_radians = std::atan(steepness);
            sense.ground_uphill = steepness > 1.0e-5f ? gradient / steepness : glm::vec2{0.0f};
        }
        // E2: the swimmer rides the ACTUAL surface, not a constant sea level -- the same Gerstner
        // sum the marcher draws, evaluated on the CPU from the same field. Only where there is sea
        // to swim in: over land the wave height is meaningless and would shift the water plane the
        // buoyancy test uses.
        if (groundHeight < world::player::kSeaLevelWorld) {
            sense.water_surface_y =
                world::player::kSeaLevelWorld +
                world::water::wave_height(waveField, transform.position.x, transform.position.z, waveTime);
        }
        const TickInput ticked =
            input.tick(transform.position, spectator.yaw_radians, spectator.pitch_radians, jumpEdge);
        jumpEdge = false; // consumed by the first tick of this frame
        // A scripted `look` applies here, inside the tick, because that is the cadence it was
        // generated at. The live path's delta is zero here and non-zero above.
        apply_look(transform, spectator, ticked.look_delta_pixels);
        const world::player::PlayerIntent& intent = ticked.intent;

        const glm::vec3 before = transform.position;
        const world::player::Stance stanceBefore = spectator.physics.stance;
        world::player::StepResult step;
        if (query != nullptr) {
            // A query that maintains a cache gets told where the body is; one that does not (the
            // octree) has nothing to refresh, and `if constexpr` says so at compile time rather
            // than by a virtual no-op.
            if constexpr (requires(Q& q, glm::vec3 p) { q.refresh(p); }) {
                query->refresh(transform.position);
            }
            const auto collisionBegin = std::chrono::steady_clock::now();
            step = app::step_camera(transform, spectator, *query, intent, sense, dt);
            // Goal 228: the analytic backstop in step_player is gone, so a body that ends a tick
            // inside solid is now VISIBLE instead of silently lifted. This is the counter that
            // makes it visible; clip_stress asserts it stays zero.
            const world::collision::Aabb body = world::collision::Aabb::upright(
                transform.position - glm::vec3{0.0f, spectator.tuning.eye_height, 0.0f},
                spectator.tuning.body_half_width, spectator.tuning.body_height);
            // Ask about a box INSET by the sweep's own contact skin, not the raw one. A body
            // resting exactly on a surface reads as marginally inside it -- the caller stores the
            // eye and rebuilds the feet as (eye - eye_height) every tick, and a voxel world puts
            // surfaces at exact coordinates constantly. SweepParams::skin already says this and
            // already tolerates it; a counter that does NOT tolerate it measures the float round
            // trip instead of the bug. Measured on clip_stress at 4x speed: 358 "inside solid"
            // ticks whose deepest corner was 0.0001 m in -- one ten-thousandth of a metre, against
            // a 0.0078 m voxel. The real embeddings this counter was built to find were 0.048 m.
            constexpr float kContactSkin = world::collision::SweepParams{}.skin;
            const world::collision::Aabb contactBody{body.min + glm::vec3{kContactSkin},
                                                     body.max - glm::vec3{kContactSkin}};
            const bool endedInside = query->overlaps_solid(contactBody);
            const auto collisionNanos =
                static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                               std::chrono::steady_clock::now() - collisionBegin)
                                               .count());
            collisionCost.nanos += collisionNanos;
            ++collisionCost.ticks;
            collisionCost.worst_nanos = std::max(collisionCost.worst_nanos, collisionNanos);
            // Attribution: cost per tick is (how many times we asked) x (how deep each ask went),
            // and a budget miss is only actionable once you know which factor is the large one.
            if constexpr (requires(const Q& q) { q.query_count(); }) {
                collisionCost.queries += query->query_count();
                collisionCost.node_visits += query->node_visit_total();
                query->reset_query_counters();
            }
            if (endedInside) {
                ++insideSolidEvents;
                insideSolidStartedInside += step.started_inside ? 1u : 0u;
                insideSolidSteppedUp += step.stepped_up ? 1u : 0u;
                if (insideSolidEvents == 1) {
                    log(LogLevel::Error,
                        "body ended a tick INSIDE solid: eye ({:.4f}, {:.4f}, {:.4f}), feet y {:.4f}, "
                        "box y [{:.4f}, {:.4f}], stance {}",
                        transform.position.x, transform.position.y, transform.position.z, body.min.y,
                        body.min.y, body.max.y, static_cast<int>(spectator.physics.stance));
                    if constexpr (requires(const Q& q, float a) { q.voxel_top(a, a, a); }) {
                        log(LogLevel::Error,
                            "  voxel_top under the body: centre {:.4f}, corners {:.4f} {:.4f} {:.4f} {:.4f}",
                            query->voxel_top(transform.position.x, transform.position.z, body.max.y),
                            query->voxel_top(body.min.x, body.min.z, body.max.y),
                            query->voxel_top(body.max.x, body.min.z, body.max.y),
                            query->voxel_top(body.min.x, body.max.z, body.max.y),
                            query->voxel_top(body.max.x, body.max.z, body.max.y));
                    }
                }
            }
        } else {
            const app::OpenWorld open; // --noclip: nothing is solid
            step = app::step_camera(transform, spectator, open, intent, sense, dt);
        }

        // Goal 236: every Grounded/Airborne/Swimming transition, counted. The waterline is the
        // place this can go wrong -- the `inWater` predicate compares the feet against a surface
        // that now MOVES (the Gerstner sum), so a body floating at a crest can cross the predicate
        // twice per wave and flicker between Swimming and Grounded.
        if (spectator.physics.stance != stanceBefore) {
            ++stanceChanges;
            // Attribution, not just a count. Which PAIR oscillates says which subsystem is
            // responsible, and this pass has now been wrong twice by guessing that from a total.
            const auto pair = [](world::player::Stance a, world::player::Stance b) {
                return static_cast<int>(a) * 3 + static_cast<int>(b);
            };
            ++stanceTransitions[static_cast<std::size_t>(pair(stanceBefore, spectator.physics.stance))];
        }

        // A6: render-only feel. Off under the mechanical checks so --autofly/--verify-frame keep
        // measuring exactly the body the physics moved.
        world::player::ViewPolishSense polish;
        const glm::vec3 travelled = transform.position - before;
        polish.horizontal_speed = glm::length(glm::vec2{travelled.x, travelled.z}) / dt;
        polish.boosting = intent.boost;
        // `!verify_frame` used to be in this condition too, and it made goal 240 impossible: the
        // harness sets verify_frame on every scenario (that is where the contrast metric comes from),
        // so a capture sequence photographing the view polish photographed it switched OFF -- measured,
        // ten frames across a landing, polish +0.0000 on every one.
        //
        // It was over-cautious rather than wrong-headed. The mechanical checks read the PHYSICAL body:
        // the walk-violation counter and the inside-solid counter both test `transform.position`, and
        // the polish is added to the camera COPY a few lines below, never to the transform. So polish
        // cannot move what those measure. `--autofly` keeps its exclusion because it is a streaming
        // smoke test whose documented behaviour is a bare camera.
        const bool polishOn = options.view_polish && !options.autofly;
        viewOffsetY = world::player::update_view_polish(spectator.physics, spectator.tuning, polish, step,
                                                        polishOn, dt);

        if (walking) {
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
    // The TOTAL render-only offset, which is what goal 240's strip needs reported: the smoothing
    // and the polish are both "the eye is not exactly where the body is" and a still cannot
    // separate them.
    smoothOffsetY = spectator.physics.eye_smooth_offset;
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
                   const app::TreeLookup& trees, bool crosshair, bool showOverlay,
                   const world::wind::WindParams& wind, float windTime) {
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
    // Goal 242: the range is `kAimResolvableRange` (34 m), not 300 -- see its derivation. The mesh
    // path has no octree, so it keeps the analytic march.
    const app::AimHit aim =
        app::query_aim(world.heightmap(), camera.position, aimDir, app::kAimResolvableRange, &trees);
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
    if (showOverlay) {
        overlay.render(stats);
    }
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
    // Goal 240: the render-only eye offset in effect at the moment of a capture. The POLISH and the
    // SMOOTHING are held apart, because the captured landing sequence found them disagreeing about
    // which way the eye should go and a single total hid that. Held here rather than passed, because
    // capture_phase already takes eight arguments and these are written once per frame.
    float viewOffsetY = 0.0f;
    float smoothOffsetY = 0.0f;
    // Goal 237's exposure trace at the capture.
    bool exposureValid = false;
    bool exposureBrightening = false;
    float exposureMeasured = 0.0f;
    float exposureAdapted = 0.0f;
    float exposureMultiplier = 1.0f;
    bool verifyOk = false;
    bool verifyRan = false;
    std::uint32_t screenshotCounter = 0; // F2 capture numbering (goal 9)
};

// Goal 272: the frames a RenderDoc capture brackets are exactly the frames that write a numbered
// dump, so "the artefact is on frame 340" and "the capture of frame 340" are the same selection and
// an unattended run produces both. Shared with capture_phase so the two can never disagree.
[[nodiscard]] bool frame_dumps(const AppOptions& options, std::uint32_t frame) {
    return options.dump_every > 0 && frame % options.dump_every == 0;
}

// A capture must open BEFORE the frame's draw calls and close after Present -- opening it in
// capture_phase (which runs after rendering, next to the readback) would have captured the readback
// and nothing else. Both are no-ops when RenderDoc is not injected, which is why neither call site
// tests available(): the branch would only duplicate the one inside.
void renderdoc_frame_begin(const AppOptions& options, std::uint32_t frame) {
    if (frame_dumps(options, frame)) {
        render::diligent::renderdoc_trigger().begin_capture();
    }
}
void renderdoc_frame_end() { (void)render::diligent::renderdoc_trigger().end_capture(); }

bool capture_phase(CaptureState& cap, const AppOptions& options, std::uint32_t frame,
                   render::diligent::RenderContext& context, std::size_t sceneReady, FrameInput& input,
                   const RunHooks& hooks, const std::string& scenarioCapture) {
    if (options.verify_frame && !cap.verifyRan && sceneReady > 0) {
        const float fraction = render::diligent::sample_non_reference_pixel_fraction(context);
        // Local-contrast metric (see frame_verify.cpp for the two prior metrics it replaced and
        // why): terrain texture measures 12.3% at this pose, sky-only 0.9%. 6% keeps the
        // winding-bug lesson's discrimination -- sliver-band frames fail hard.
        // A debug view (one flat shading term) has no terrain texture to measure: it captures only.
        const bool debugView = options.svo_settings.debug_view != render::diligent::SvoDebugView::None;
        cap.verifyOk = debugView || fraction >= 0.06f;
        cap.verifyRan = true;
        if (hooks.on_verify) {
            hooks.on_verify(fraction);
        }
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
    // A scenario's capture points (goal 208) land here rather than in a second capture path, so a
    // harness PNG and an F2 screenshot are written by the same code from the same back buffer.
    if (!scenarioCapture.empty()) {
        const std::string path = scenarioCapture + ".png";
        // The contrast metric is sampled at EVERY capture point, not once on the first ready
        // frame the way --verify-frame does it. A moving scenario's first ready frame is whatever
        // the camera happened to be looking at while the world appeared -- for walk_shoreline that
        // was the inside of a hill, and the assertion read 0.0% about a run whose later frames
        // were fine. Sampling where the scenario asked for a picture is sampling where it meant.
        if (hooks.on_verify) {
            hooks.on_verify(render::diligent::sample_non_reference_pixel_fraction(context));
        }
        const bool written = render::diligent::dump_frame(context, path.c_str());
        // Goal 237: the exposure state at the capture, so a scenario that looks from sky to shadow
        // has its adaptation TRACE in the log rather than only in the pixels.
        if (cap.exposureValid) {
            log(LogLevel::Info,
                "capture {}: exposure measured {:+.3f} EV, adapted {:+.3f} EV, multiplier {:.4f} ({})",
                scenarioCapture, static_cast<double>(cap.exposureMeasured),
                static_cast<double>(cap.exposureAdapted), static_cast<double>(cap.exposureMultiplier),
                cap.exposureBrightening ? "brightening" : "darkening");
        }
        // Goal 240: the render-only eye offset AT the capture, so a strip of stills can be read as
        // numbers as well as looked at. A 2 cm effect at a 0.12 s time constant is exactly the case
        // where a picture alone is not evidence -- which is the reason Prompt 001 shipped A6 without
        // one and the reason this line exists.
        log(written ? LogLevel::Info : LogLevel::Error,
            "capture {}: {} ({}) eye offset {:+.4f} m (polish {:+.4f}, smoothing {:+.4f})", scenarioCapture,
            path, written ? "written" : "FAILED", static_cast<double>(cap.viewOffsetY + cap.smoothOffsetY),
            static_cast<double>(cap.viewOffsetY), static_cast<double>(cap.smoothOffsetY));
        if (hooks.on_capture) {
            hooks.on_capture(scenarioCapture, path, written);
        }
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

} // namespace

// ---- Session -----------------------------------------------------------------------------------

Session::Session(const AppOptions& options, bool visible) : window(1280, 720, "voxel_app", visible) {
    {
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
            // Auto-exposure needs the HDR scene target, so it only exists when post does.
            autoExposure = std::make_unique<render::diligent::AutoExposure>(*context);
            render::diligent::ExposureSettings exposure;
            exposure.enabled = options.auto_exposure;
            exposure.crosshair_metering = options.exposure_metering_crosshair;
            exposure.key = options.exposure_key.value_or(exposure.key);
            exposure.pooling_half_angle_deg =
                options.exposure_pool_deg.value_or(exposure.pooling_half_angle_deg);
            if (options.exposure_pin_ev) {
                exposure.pinned_log2 = *options.exposure_pin_ev;
            }
            autoExposure->set_settings(exposure);
        }
        context->set_vsync(options.vsync);
        render::diligent::attach_gpu_profiler(
            *context); // Tracy GPU zones (Vulkan only; safe no-op elsewhere)
        render::diligent::set_gpu_timers_enabled(*context, options.gpu_timers);

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
        // Walk is PlayerState's own default now; --fly is the dev opt-in and needs --dev to have
        // been given, which main() has already checked.
        if (options.fly) {
            spectator.physics.mode = app::CameraMoveMode::Fly;
        }
        // Goal 229: the sub-step rule is claimed to be speed-independent. This is the knob that
        // lets clip_stress put that claim under 40x the shipped speed.
        //
        // It has to reach the BODY's speeds, not just the fly camera's. Goal 232 moved walking off
        // `move_speed` onto its own m/s fields, and for one build this flag silently stopped
        // scaling the thing clip_stress exists to stress -- an instrument that quietly measures
        // nothing is worse than no instrument, which is this pass's recurring lesson.
        // `ground_accel`/`ground_decel` scale with it so TIME-to-speed stays constant and the ramp
        // does not swallow the run at 40x.
        spectator.move_speed *= options.speed_scale;
        spectator.tuning.walk_speed *= options.speed_scale;
        spectator.tuning.sprint_speed *= options.speed_scale;
        spectator.tuning.ground_accel *= options.speed_scale;
        spectator.tuning.ground_decel *= options.speed_scale;
        // A3: the svo path's step allowance is a smoothing budget, not a ledge climb.
        spectator.tuning.step_height = options.renderer == RendererKind::Svo
                                           ? world::player::kSvoStepHeight
                                           : world::player::kDefaultTuning.step_height;
        if (options.step_height) {
            spectator.tuning.step_height = *options.step_height;
        }
        if (options.max_walk_slope_deg) {
            spectator.tuning.max_walk_slope_radians = glm::radians(*options.max_walk_slope_deg);
        }
        spawnPosition = transform.position;
    }
}

Session::~Session() = default;

void Session::attach_overlay() {
    // ImGui's GLFW backend chain-installs on top of whatever input callbacks are already there, so
    // this runs AFTER a LiveInput has installed GlfwInput's. A scripted run installs none and the
    // chain is simply shorter -- which is also why the harness's window receives no keyboard.
    overlay = std::make_unique<render::diligent::DebugOverlay>(*context, window.handle());
}

bool Session::begin_frame() {
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

// ---- LiveInput ---------------------------------------------------------------------------------

LiveInput::LiveInput(GlfwWindow& window)
    : input_(std::make_unique<engine::input::GlfwInput>(window.handle())) {}
LiveInput::~LiveInput() = default;

void LiveInput::begin_frame() {
    // The jump press is taken ONCE per frame here and handed to at most the first tick, which is
    // world/player's own contract. Taking it inside tick() would re-arm the buffer every tick.
    jumpEdgeThisFrame_ = input_->take_jump();
}
bool LiveInput::quit_requested() const {
    return input_->state().quit_requested;
}
bool LiveInput::take_walk_toggle() {
    return input_->take_walk_toggle();
}
bool LiveInput::take_screenshot() {
    return input_->take_screenshot();
}
glm::vec2 LiveInput::take_look_delta() {
    return input_->take_look_delta();
}
TickInput LiveInput::tick(const glm::vec3&, float, float, bool jumpEdge) {
    TickInput out;
    out.intent = to_intent(input_->state(), jumpEdge && jumpEdgeThisFrame_);
    if (jumpEdge) {
        jumpEdgeThisFrame_ = false;
    }
    return out;
}

// ---- the two frame loops -----------------------------------------------------------------------

int run_mesh(Session& s, const AppOptions& options, FrameInput& input, const RunHooks& hooks) {
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
    std::uint32_t walkViolations = 0;                 // ticks ending below the ground surface in walk mode
    std::uint32_t insideSolidEvents = 0;              // goal 228: ticks that ended with the body in solid
    std::uint32_t insideSolidStartedInside = 0;       // ...of which the sweep already found embedded
    std::uint32_t insideSolidSteppedUp = 0;           // ...of which stepped up this tick
    CollisionCost collisionCost;                      // goal 230: nanoseconds per fixed tick
    std::uint32_t stanceChanges = 0;                  // goal 236: stance transitions over the run
    std::array<std::uint32_t, 9> stanceTransitions{}; // ...broken down by (from, to)
    // Prompt 001 A1: the simulation's own clock, carried across frames (never a frame-loop
    // local), plus the render-only view-polish offset it produces.
    world::player::FixedStepper stepper;
    float viewOffsetY = 0.0f;
    float smoothOffsetY = 0.0f; // goal 240: reported separately from the polish, since they differ
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
        if (input.quit_requested() || input.exhausted()) {
            break;
        }
        input.begin_frame();
        renderdoc_frame_begin(options, frame);

        if (!world.finished()) {
            // Goal 128/130: one increment of the one-time generation/mesh/upload pass, then a
            // real, moving loading-screen frame -- no interactive camera control, no capture/
            // verify logic, none of that is meaningful before the world exists to look at.
            world.pump();
            renderer.render(render::interface::Camera{}); // sky-only backdrop; zero chunks uploaded yet
            // No metering on a loading frame: there is no camera and no world, only the sky
            // backdrop, and letting the adaptation seed itself off that would open every run with
            // the exposure walking back from whatever the sky happened to be.
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
                s.registry, s.cameraEntity, input, world.heightmap(), options.noclip ? nullptr : &collider,
                s.clock, options, walkViolations, stepper, viewOffsetY, waveField, waveTime,
                insideSolidEvents, insideSolidStartedInside, insideSolidSteppedUp, collisionCost,
                stanceChanges, stanceTransitions, smoothOffsetY);

            renderer.render(camera);
            if (s.postProcess) {
                // Meter BEFORE the composite: the composite is what consumes the exposure, and the
                // scene target it reads is the same one the metering pass just measured.
                if (s.autoExposure) {
                    s.autoExposure->measure(camera.fov_y_radians,
                                            static_cast<float>(s.clock.delta_seconds()));
                    s.postProcess->set_exposure(s.autoExposure->exposure());
                    // Goal 238: bloom follows the same adaptation the exposure does.
                    const render::diligent::ExposureSample& e = s.autoExposure->last_sample();
                    s.postProcess->set_adaptation_log2(e.valid && options.bloom_follows_exposure
                                                           ? e.adapted_log2
                                                           : std::numeric_limits<float>::quiet_NaN());
                }
                s.postProcess->execute(frame);
            }
            overlay_phase(telemetry, s.clock, *s.context, renderer, world, *s.overlay, chunkCounters, camera,
                          aimTrees, crosshairOn, options.overlay, options.svo_settings.wind, waveTime);
            cap.viewOffsetY = viewOffsetY;
            cap.smoothOffsetY = smoothOffsetY;
            if (s.autoExposure) {
                const render::diligent::ExposureSample& e = s.autoExposure->last_sample();
                cap.exposureValid = e.valid;
                cap.exposureBrightening = e.brightening;
                cap.exposureMeasured = e.measured_log2;
                cap.exposureAdapted = e.adapted_log2;
                cap.exposureMultiplier = e.exposure;
            }
            capture_phase(cap, options, frame, *s.context, world.ready_chunk_count(), input, hooks,
                          hooks.capture_name
                              ? hooks.capture_name(frame, input.script_seconds(), true, false, false,
                                                   spectator_grounded(s.registry, s.cameraEntity))
                              : std::string{});
            report_phase(telemetry, s.clock, renderer, world, chunkCounters);
        }

        s.context->present();
        renderdoc_frame_end();
        FrameMark;
        ++frame;
        ++telemetry.framesSinceReport;

        // A verify-only run (no explicit frame budget) has done its job once the readback ran.
        if (options.verify_frame && cap.verifyRan && options.frames == 0) {
            break;
        }
    }

    bool walkOk = true;
    {
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

    if (insideSolidEvents > 0) {
        log(LogLevel::Error,
            "collision: {} ticks ended with the body INSIDE solid (0 required) -- {} of them the sweep "
            "had ALREADY found embedded (started_inside: it moved unblocked), {} stepped up",
            insideSolidEvents, insideSolidStartedInside, insideSolidSteppedUp);
    }
    if (collisionCost.ticks > 0) {
        log(collisionCost.mean_ms() <= 0.20 ? LogLevel::Info : LogLevel::Error,
            "collision: {:.4f} ms mean per tick over {} ticks, worst {:.4f} ms (budget 0.20 ms) -- "
            "{:.1f} queries/tick, {:.1f} nodes/query",
            collisionCost.mean_ms(), collisionCost.ticks, collisionCost.worst_ms(),
            collisionCost.ticks == 0
                ? 0.0
                : static_cast<double>(collisionCost.queries) / static_cast<double>(collisionCost.ticks),
            collisionCost.queries == 0 ? 0.0
                                       : static_cast<double>(collisionCost.node_visits) /
                                             static_cast<double>(collisionCost.queries));
    }
    // Goal 236: the flicker count is only readable next to the wave period it should be compared
    // against -- "31 stance changes" means nothing until you know the run saw 25 crests.
    {
        static constexpr std::array<const char*, 3> kStanceNames{"grounded", "airborne", "swimming"};
        std::string breakdown;
        for (std::size_t from = 0; from < 3; ++from) {
            for (std::size_t to = 0; to < 3; ++to) {
                const std::uint32_t n = stanceTransitions[from * 3 + to];
                if (n > 0) {
                    breakdown += std::format(" {}->{}:{}", kStanceNames[from], kStanceNames[to], n);
                }
            }
        }
        log(LogLevel::Info, "stance: {} transitions over the run;{} -- dominant wave period {:.2f} s",
            stanceChanges, breakdown.empty() ? std::string{" none"} : breakdown,
            world::water::dominant_period(waveField));
    }
    if (hooks.on_invariants) {
        hooks.on_invariants(walkViolations, insideSolidEvents, stanceChanges);
    }
    log(LogLevel::Info, "exiting after {} frames on {}", frame,
        render::diligent::to_string(s.context->backend()));
    return cap.verifyOk && autoflyOk && walkOk ? EXIT_SUCCESS : EXIT_FAILURE;
}

// The micro-voxel path (docs/goals.md Group X): no chunks, no meshes -- a SvoWorld builds
// sparse-brick octrees around the camera on a background thread, SvoRenderer marches them.
int run_svo(Session& s, const AppOptions& options, FrameInput& input, const RunHooks& hooks) {
    render::diligent::SvoRenderer renderer(*s.context);
    renderer.set_settings(options.svo_settings);
    app::SvoWorld world(options.svo);
    // Prompt 003 goals 226/227, closing goal 173: the body collides against the SAME immutable
    // octree the marcher is drawing, not against a 16 m cached height grid a fast camera outruns.
    // There is no cache to refresh and no edge to cross. Measured: inside the finest LOD ring the
    // tree and the sampler agree exactly (0 disagreements in 10,000 voxel boxes), and the body is
    // always inside that ring because the LOD centre IS the camera -- see
    // research/player-embodiment-log.md for the hole rate binned by distance.
    world::collision::OctreeCollider collider;

    CaptureState cap;
    cap.verifyOk = !options.verify_frame;
    FrameTelemetry telemetry;
    std::uint32_t frame = 0;
    std::uint32_t walkViolations = 0;
    std::uint32_t insideSolidEvents = 0; // goal 228
    std::uint32_t insideSolidStartedInside = 0;
    std::uint32_t insideSolidSteppedUp = 0;
    CollisionCost collisionCost;     // goal 230, the mesh path too
    std::uint32_t stanceChanges = 0; // goal 236, the mesh path too
    std::array<std::uint32_t, 9> stanceTransitions{};
    world::player::FixedStepper stepper;
    float viewOffsetY = 0.0f;
    float smoothOffsetY = 0.0f; // goal 240: reported separately from the polish, since they differ
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
    // Goal 215: these two structs used to be declared HERE, inside this function body, which is
    // why nothing but this function could report a frame's breakdown -- and why a harness would
    // have had to copy the loop to get one. They are dev::telemetry::FramePhases / FrameCauses
    // now, and voxel_app's own stats line and voxel_harness's report read the same object.
    constexpr double kSlowFrameMs = dev::telemetry::kSlowFrameMs;
    FramePhases prevPhases;
    FrameCauses prevCauses;
    dev::telemetry::SlowFrameCounts slow;
    // The record for the frame BEFORE this one, waiting for its wall time (see the note at the
    // emission site).
    FrameRecord pending;
    bool pendingValid = false;
    bool pendingReady = false;
    const auto phase_ms = [](std::chrono::steady_clock::time_point& t) {
        const auto now = std::chrono::steady_clock::now();
        const double ms = std::chrono::duration<double, std::milli>(now - t).count();
        t = now;
        return ms;
    };

    // Hands a finished build to the renderer's staged upload, and pumps that upload one slice per
    // frame; logs the tree the frame it lands.
    glm::vec3 lastCameraPos{0.0f};
    // Goals 263-265: the streaming path. Started once, then fed a bounded number of finished cells
    // per frame -- the producer never touches the render thread's critical path, which is the
    // starvation failure research 1.9(a) warns about and goal 170 measured as `present` stalls.
    // Goal 272: the RenderDoc trigger, wired to the SAME frames --dump-every and the capture
    // points already fire on, so a scenario can capture the exact frame an artefact appears on
    // unattended. A no-op when RenderDoc is not injected, which is every run on this machine.
    log(LogLevel::Info, "renderdoc: {}", render::diligent::renderdoc_trigger().status());
    bool streamStarted = false;
    std::uint64_t streamedCells = 0;
    std::uint64_t streamBytes = 0;
    // Goal 262: the words the marcher wrote, read back three frames later.
    std::vector<std::uint32_t> usageWords;
    world::svo::CellMarks usageMarks;
    std::uint64_t usageReadbacks = 0;
    const auto adopt_finished = [&]() {
        // Goal 256: the grid path, first, because a build produces exactly one of the two. Same
        // shape as the tree path below -- both owners take the SAME immutable structure in one
        // statement, which is what makes "you cannot pass through anything the renderer draws"
        // true by construction rather than by a tolerance.
        if (std::shared_ptr<const world::svo::FlatCellGrid> grid = world.take_finished_grid()) {
            world.note_adopted(s.clock.elapsed_seconds(), lastCameraPos);
            collider.set_grid(grid);
            collider.bump_generation();
            renderer.begin_upload(std::move(grid));
        } else if (std::shared_ptr<const world::svo::BrickTree> tree = world.take_finished()) {
            world.note_adopted(s.clock.elapsed_seconds(), lastCameraPos);
            // Both owners take the handle in the same statement: the renderer stages it onto the
            // GPU across frames, the simulation queries it. One immutable object, two owners.
            // The swap happens on the main thread between ticks, so a plain assignment is correct
            // and an atomic would advertise a contract that does not exist.
            collider.set_tree(tree);
            collider.bump_generation();
            renderer.begin_upload(std::move(tree));
        }
        if (!renderer.pump_upload()) {
            return false;
        }
        lastUploadMs = renderer.last_upload_ms();
        ++uploads;
        const app::SvoWorld::LastBuild last = world.last_build();
        log(LogLevel::Info,
            "svo tree #{}: {} bricks, {} internal, {} solid leaves, {:.1f} MB, build {:.2f}s (sampler "
            "{:.2f}s, {} classified, {} bricks sampled), staged upload {:.1f} ms over {} frames, {} trees"
            ", adopt lag {:.1f} m{}",
            uploads, last.bricks, last.tree.internal_nodes, last.tree.solid_leaves,
            static_cast<double>(last.memory_bytes) / 1.0e6, last.stats.seconds, last.sampler_seconds,
            last.stats.boxes_classified, last.stats.bricks_sampled, lastUploadMs,
            renderer.last_upload_frames(), last.trees, world.last_adopt_lag_metres(),
            // Goal 257: what the incremental rebuild actually saved. Empty on the single-tree path,
            // where there is nothing to reuse and printing "0 reused" would imply there was.
            last.cells > 0 ? std::format(", cells {} rebuilt / {} reused", last.cells_rebuilt,
                                         last.cells_reused)
                           : std::string{});
        return true;
    };

    while (!s.window.should_close() && (options.frames == 0 || frame < options.frames)) {
        // begin_frame() is INSIDE the accounting now. It was not, and on a tree-swap frame that
        // hid 186 of 205 ms -- see FramePhases::frame_start.
        auto frameStartClock = std::chrono::steady_clock::now();
        if (!s.begin_frame()) {
            continue;
        }
        const double frameStartMs = phase_ms(frameStartClock);
        if (input.quit_requested() || input.exhausted()) {
            break;
        }
        input.begin_frame();
        renderdoc_frame_begin(options, frame);
        // The clock's delta is the PREVIOUS frame's duration: attribute it to that frame's phases,
        // and hand the held-over record its true wall time.
        if (pendingValid) {
            pending.wall_ms = s.clock.delta_seconds() * 1000.0;
            if (pendingReady) {
                hooks.on_frame(pending);
            } else if (hooks.on_warmup_frame) {
                hooks.on_warmup_frame(pending.wall_ms);
            }
            pendingValid = false;
        }
        if (frame > 0 && renderer.has_tree()) {
            const double frameMs = s.clock.delta_seconds() * 1000.0;
            if (frameMs > kSlowFrameMs) {
                ++slow.total;
                if (prevCauses.swapped) {
                    ++slow.on_swap;
                } else if (prevCauses.uploading) {
                    ++slow.while_uploading;
                } else if (prevCauses.building) {
                    ++slow.while_building;
                } else {
                    ++slow.other;
                }
                // `capture` is IN this line. It was not, and that cost a hypothesis: a 180 ms frame
                // whose seven printed phases summed to 0.7 ms looks like a mysterious stall, and is
                // actually the harness writing a PNG (a staging copy + WaitForIdle + libpng, which
                // CLAUDE.md already documents at 200+ ms). A breakdown that does not add up to the
                // total is not a breakdown.
                log(LogLevel::Warn,
                    "slow frame {}: {:.1f} ms = start {:.1f} + upload {:.1f} + camera {:.1f} + render "
                    "{:.1f} + post {:.1f} + overlay {:.1f} + present {:.1f} + capture {:.1f}{}{}{}{}",
                    frame - 1, frameMs, prevPhases.frame_start, prevPhases.upload, prevPhases.camera,
                    prevPhases.render, prevPhases.post, prevPhases.overlay, prevPhases.present,
                    prevPhases.capture, prevCauses.swapped ? " [tree swapped]" : "",
                    prevCauses.uploading ? " [uploading]" : "", prevCauses.building ? " [building]" : "",
                    prevCauses.refreshed ? " [cache refreshed]" : "");
            }
        }
        FramePhases phases;
        FrameCauses causes;
        phases.frame_start = frameStartMs;
        auto phaseClock = std::chrono::steady_clock::now();
        causes.uploading = renderer.upload_pending() || world.take_finished_pending();
        causes.building = world.building();
        causes.swapped = adopt_finished();
        phases.upload = phase_ms(phaseClock);

        // Goal 220's whole-frame range: the four per-pass ranges must sum inside it. Opened here
        // and closed just before present, so it brackets exactly the GPU work of this frame.
        std::optional<render::diligent::GpuPassScope> frameScope;
        frameScope.emplace(*s.context, render::diligent::GpuPass::Frame);
        if (!renderer.has_tree()) {
            // Loading screen until the first tree lands: sky only, no camera control.
            renderer.render(render::interface::Camera{});
            // No metering on a loading frame -- see the mesh path's own note: sky only, no camera,
            // and seeding the adaptation off that opens every run with the exposure walking back.
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
            // The renderer's OWN animation clock, so the swimmer rides the surface being drawn
            // rather than one that agrees with it only approximately.
            const float waveTime = renderer.anim_seconds();
            const render::interface::Camera camera =
                update_camera_phase(s.registry, s.cameraEntity, input, world.heightmap(),
                                    options.noclip || !collider.has_tree() ? nullptr : &collider, s.clock,
                                    options, walkViolations, stepper, viewOffsetY, waveField, waveTime,
                                    insideSolidEvents, insideSolidStartedInside, insideSolidSteppedUp,
                                    collisionCost, stanceChanges, stanceTransitions, smoothOffsetY);
            // Rebuild once the camera has left the inner half of the finest LOD ring: the tree is
            // still correct everywhere (coarser rings are conservative), just not at full detail
            // right around the camera until the new one lands.
            // Goals 249/250: the trigger is SvoWorld's policy now, not a rule inlined here. It
            // carries a named distance with hysteresis, a minimum interval measured from the last
            // adoption, and a speed gate -- the old `> lod_radius * 0.5f` tied how often the world
            // was rebuilt to a DETAIL parameter and asked for a 400 MB rebuild every 2 metres.
            const float cameraSpeed = glm::length(glm::vec2{camera.position.x - lastCameraPos.x,
                                                            camera.position.z - lastCameraPos.z}) /
                                      std::max(static_cast<float>(s.clock.delta_seconds()), 1.0e-4f);
            lastCameraPos = camera.position;
            if (options.rebuild &&
                world.should_rebuild(camera.position, cameraSpeed, s.clock.elapsed_seconds())) {
                world.request_build(camera.position);
            }
            phases.camera = phase_ms(phaseClock);

            renderer.render(camera);
            phases.render = phase_ms(phaseClock);
            if (s.postProcess) {
                // Meter BEFORE the composite: the composite is what consumes the exposure, and the
                // scene target it reads is the same one the metering pass just measured.
                if (s.autoExposure) {
                    s.autoExposure->measure(camera.fov_y_radians,
                                            static_cast<float>(s.clock.delta_seconds()));
                    s.postProcess->set_exposure(s.autoExposure->exposure());
                    // Goal 238: bloom follows the same adaptation the exposure does.
                    const render::diligent::ExposureSample& e = s.autoExposure->last_sample();
                    s.postProcess->set_adaptation_log2(e.valid && options.bloom_follows_exposure
                                                           ? e.adapted_log2
                                                           : std::numeric_limits<float>::quiet_NaN());
                }
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
            // Goal 241: on the svo path the crosshair asks the OCTREE -- the same structure the
            // body collides against and the same `trace_ray` the shader mirrors -- so it cannot
            // disagree with either about what is there. The analytic march is the fallback for the
            // seconds before the first tree lands, and nothing else.
            const float aimRange = options.aim_range.value_or(app::kAimResolvableRange);
            const app::AimHit aim =
                collider.grid() != nullptr
                    ? app::query_aim_octree(*collider.grid(), camera.position, aimDir, aimRange)
                    : (collider.tree() != nullptr
                           ? app::query_aim_octree(*collider.tree(), camera.position, aimDir, aimRange)
                           : app::query_aim(world.heightmap(), camera.position, aimDir, aimRange,
                                            &aimTrees));
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
            if (options.overlay) {
                s.overlay->render(stats);
            }
            phases.overlay = phase_ms(phaseClock);

            phaseClock = std::chrono::steady_clock::now();
            cap.viewOffsetY = viewOffsetY;
            cap.smoothOffsetY = smoothOffsetY;
            if (s.autoExposure) {
                const render::diligent::ExposureSample& e = s.autoExposure->last_sample();
                cap.exposureValid = e.valid;
                cap.exposureBrightening = e.brightening;
                cap.exposureMeasured = e.measured_log2;
                cap.exposureAdapted = e.adapted_log2;
                cap.exposureMultiplier = e.exposure;
            }
            capture_phase(cap, options, frame, *s.context, 1, input, hooks,
                          hooks.capture_name
                              ? hooks.capture_name(frame, input.script_seconds(), true, causes.swapped,
                                                   prevPhases.sum() > kSlowFrameMs,
                                                   spectator_grounded(s.registry, s.cameraEntity))
                              : std::string{});
            phases.capture = phase_ms(phaseClock);

            telemetry.track_worst(s.clock);
            if (telemetry.report_due()) {
                log(LogLevel::Info,
                    "{:.1f} fps (worst {:.1f} ms, gpu {:.2f} ms), svo {} bricks / {:.1f} MB{}",
                    telemetry.report_fps(), telemetry.worstFrameMs, renderer.last_gpu_ms(), last.bricks,
                    static_cast<double>(last.memory_bytes) / 1.0e6, world.building() ? ", rebuilding" : "");
                telemetry.reset_report();
            }
        }

        frameScope.reset(); // closes the whole-frame GPU range before Present
        phaseClock = std::chrono::steady_clock::now();
        s.context->present();
        renderdoc_frame_end();
        phases.present = phase_ms(phaseClock);
        render::diligent::gpu_passes_end_frame(*s.context);
        prevPhases = phases;
        prevCauses = causes;

        // The harness's per-frame record. Held over to the TOP of the next frame, because that is
        // the only place this frame's true wall time exists: the clock's delta is measured in
        // begin_frame(), so `phases.sum()` is what the loop MEASURED and `clock.delta_seconds()`
        // is how long the frame actually took. Recording the sum as the wall time would have made
        // goal 215's phase-coverage check circular -- it would have read 100% by construction and
        // proved nothing. It is emitted honestly instead, and the gap between the two is the
        // finding (research/dev-harness-log.md).
        if (options.svo.stream_cells && options.svo.cell_size_log2 > 0) {
            if (!streamStarted) {
                const world::svo::CellGrid shape = world.stream_shape(lastCameraPos);
                renderer.begin_stream(shape, options.svo.brick_slots > 0
                                                 ? static_cast<std::size_t>(options.svo.brick_slots)
                                                 : 1200000u,
                                      world.build_proxy(shape));
                world.start_stream(shape, lastCameraPos);
                streamStarted = true;
            }
            // BOUNDED per frame. An unbounded drain would put the whole grid's installs on one
            // frame and reproduce exactly the stall this architecture exists to remove.
            // Submit a bounded slice of the queue, then take a bounded slice of what finished.
            world.pump_stream(static_cast<std::size_t>(std::max(1, options.svo.cells_per_frame)) * 2u);
            for (const app::SvoWorld::BuiltCell& built :
                 world.take_built_cells(static_cast<std::size_t>(std::max(1, options.svo.cells_per_frame)))) {
                const world::svo::BrickTree empty;
                if (renderer.install_cell(built.index, built.tree ? *built.tree : empty)) {
                    ++streamedCells;
                }
            }
            streamBytes += renderer.flush_cells();
        }


        // Goal 262: the usage readback, pipelined three frames deep so it never stalls. OUTSIDE
        // the hooks.on_frame block on purpose -- that block only runs under the harness, and a
        // readback that only happens when something is watching is not a readback.
        if (renderer.read_cell_usage(usageWords)) {
            usageMarks = world::svo::CellMarks(usageWords.size());
            ++usageReadbacks;
        }
        if (hooks.on_frame) {
            pending.index = frame;
            pending.wall_ms = 0.0; // filled in at the top of the next frame
            pending.scenario_seconds = input.script_seconds();
            pending.phases = phases;
            pending.causes = causes;
            const app::SvoWorld::LastBuild lastBuild = world.last_build();
            pending.counters.bricks = lastBuild.bricks;
            pending.counters.internal_nodes = lastBuild.tree.internal_nodes;
            pending.counters.solid_leaves = lastBuild.tree.solid_leaves;
            pending.counters.resident_bytes = lastBuild.memory_bytes;
            pending.counters.gpu_bytes = renderer.gpu_memory().allocated_bytes();
            pending.counters.uploads = uploads;
            pending.counters.gpu_ms = renderer.last_gpu_ms();
            using render::diligent::gpu_pass_ms;
            using render::diligent::GpuPass;
            pending.counters.gpu_frame_ms = gpu_pass_ms(*s.context, GpuPass::Frame);
            pending.counters.gpu_beam_ms = gpu_pass_ms(*s.context, GpuPass::Beam);
            pending.counters.gpu_march_ms = gpu_pass_ms(*s.context, GpuPass::March);
            pending.counters.gpu_resolve_ms = gpu_pass_ms(*s.context, GpuPass::Resolve);
            pending.counters.gpu_post_ms = gpu_pass_ms(*s.context, GpuPass::Post);
            pending.counters.gpu_overlay_ms = gpu_pass_ms(*s.context, GpuPass::Overlay);
            pendingReady = renderer.has_tree();
            pendingValid = true;
        }
        FrameMark;
        ++frame;
        ++telemetry.framesSinceReport;

        if (options.verify_frame && cap.verifyRan && options.frames == 0) {
            break;
        }
    }

    bool walkOk = true;
    {
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
        kSlowFrameMs, slow.total, frame, slow.on_swap, slow.while_uploading, slow.while_building, slow.other);
    if (insideSolidEvents > 0) {
        log(LogLevel::Error,
            "collision: {} ticks ended with the body INSIDE solid (0 required) -- {} of them the sweep "
            "had ALREADY found embedded (started_inside: it moved unblocked), {} stepped up",
            insideSolidEvents, insideSolidStartedInside, insideSolidSteppedUp);
    }
    if (collisionCost.ticks > 0) {
        log(collisionCost.mean_ms() <= 0.20 ? LogLevel::Info : LogLevel::Error,
            "collision: {:.4f} ms mean per tick over {} ticks, worst {:.4f} ms (budget 0.20 ms) -- "
            "{:.1f} queries/tick, {:.1f} nodes/query",
            collisionCost.mean_ms(), collisionCost.ticks, collisionCost.worst_ms(),
            collisionCost.ticks == 0
                ? 0.0
                : static_cast<double>(collisionCost.queries) / static_cast<double>(collisionCost.ticks),
            collisionCost.queries == 0 ? 0.0
                                       : static_cast<double>(collisionCost.node_visits) /
                                             static_cast<double>(collisionCost.queries));
    }
    // Goal 236: the flicker count is only readable next to the wave period it should be compared
    // against -- "31 stance changes" means nothing until you know the run saw 25 crests.
    {
        static constexpr std::array<const char*, 3> kStanceNames{"grounded", "airborne", "swimming"};
        std::string breakdown;
        for (std::size_t from = 0; from < 3; ++from) {
            for (std::size_t to = 0; to < 3; ++to) {
                const std::uint32_t n = stanceTransitions[from * 3 + to];
                if (n > 0) {
                    breakdown += std::format(" {}->{}:{}", kStanceNames[from], kStanceNames[to], n);
                }
            }
        }
        log(LogLevel::Info, "stance: {} transitions over the run;{} -- dominant wave period {:.2f} s",
            stanceChanges, breakdown.empty() ? std::string{" none"} : breakdown,
            world::water::dominant_period(waveField));
    }
    if (hooks.on_invariants) {
        hooks.on_invariants(walkViolations, insideSolidEvents, stanceChanges);
    }
    if (streamStarted) {
        // Goals 264/265: what the streaming path actually moved, against the 400 MB per 2 m of the
        // staged whole-tree upload it replaces.
        log(LogLevel::Info,
            "cell stream: {} cells installed, {} resident, {:.1f} MB uploaded total ({:.2f} MB/frame "
            "mean), {} still pending",
            streamedCells, renderer.resident_cells(),
            static_cast<double>(renderer.stream_bytes_total()) / 1.0e6,
            static_cast<double>(renderer.stream_bytes_total()) / 1.0e6 / std::max(1.0, double(frame)),
            world.stream_pending());
    }
    if (usageReadbacks > 0) {
        // Goal 262's headline: bytes per frame from GPU to CPU, against the 400 MB per 2 m of the
        // architecture this replaces.
        log(LogLevel::Info, "cell usage readback: {} landed, {} bytes/frame ({:.1f} KB), enqueue {:.3f} ms",
            usageReadbacks, renderer.last_usage_readback_bytes(),
            static_cast<double>(renderer.last_usage_readback_bytes()) / 1024.0,
            renderer.last_usage_readback_ms());
    }
    log(LogLevel::Info, "exiting after {} frames on {}", frame,
        render::diligent::to_string(s.context->backend()));
    return cap.verifyOk && autoflyOk && walkOk ? EXIT_SUCCESS : EXIT_FAILURE;
}

int run(const AppOptions& options, bool visible) {
    Session session(options, visible);
    LiveInput live(session.window);
    session.attach_overlay(); // after LiveInput: ImGui chains on top of its callbacks
    const RunHooks hooks;
    return options.renderer == RendererKind::Svo ? run_svo(session, options, live, hooks)
                                                 : run_mesh(session, options, live, hooks);
}

} // namespace app