#include "svo_world.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <utility>

#include "engine/core/log.hpp"
#include "world/svo/terrain_sampler.hpp"

#if defined(TRACY_ENABLE)
#include <tracy/Tracy.hpp>
#else
#define ZoneScopedN(name)
#endif

namespace app {

using engine::core::log;
using engine::core::LogLevel;

namespace {

// Goal 170's measured rule: a build on EVERY hardware thread starved the render thread -- 12 of
// the 13 frames over 20 ms in a 900-frame walk were `present` stalls of 20-30 ms with a build
// running and nothing else happening; on 12 of 16 threads the same walk had one slow frame (the
// tree swap). Three quarters of the machine builds, the rest keeps the frame moving.
std::size_t default_build_threads() noexcept {
    const unsigned int hw = std::jthread::hardware_concurrency();
    return hw == 0 ? 1 : std::max<std::size_t>(1, static_cast<std::size_t>(hw) * 3 / 4);
}

} // namespace

SvoWorld::SvoWorld(const SvoWorldOptions& options)
    : options_(options), heightmap_(options.seed),
      pool_(options.worker_threads == 0 ? default_build_threads() : options.worker_threads) {}

SvoWorld::~SvoWorld() = default; // worker_ joins first (declared last), then pool_ drains

world::svo::TreeGeometry SvoWorld::geometry_for(glm::vec3 camera) const noexcept {
    world::svo::TreeGeometry g;
    g.root_size_log2 = options_.root_size_log2;
    g.voxel_size_log2 = options_.voxel_size_log2;
    const float half = g.root_edge() * 0.5f;
    g.origin = glm::vec3{std::floor((camera.x - half) / 8.0f) * 8.0f, 8.0f - half,
                         std::floor((camera.z - half) / 8.0f) * 8.0f};
    return g;
}

float SvoWorld::distance_from_build_center(glm::vec3 camera) const noexcept {
    return glm::length(camera - buildCenter_);
}

bool SvoWorld::request_build(glm::vec3 camera) {
    if (building_.load()) {
        return false;
    }
    if (worker_.joinable()) {
        worker_.join(); // the previous build has finished (building_ is false); reap its thread
    }
    building_.store(true);
    requested_ = true;
    buildCenter_ = camera;
    worker_ = std::jthread([this, camera] { build_job(camera); });
    return true;
}

void SvoWorld::build_job(glm::vec3 camera) {
    ZoneScopedN("svo build");
    try {
        const world::svo::TreeGeometry g = geometry_for(camera);
        world::svo::TerrainSamplerParams sp;
        sp.seed = options_.seed;
        sp.trees = options_.trees;
        const world::svo::Box region{g.origin, g.max_corner()};
        const auto samplerStart = std::chrono::steady_clock::now();
        world::svo::TerrainSampler sampler(heightmap_, sp, region);
        sampler.set_focus(camera, 4.0f * options_.lod_radius);
        const double samplerSeconds =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - samplerStart).count();

        world::svo::BuildParams bp;
        bp.lod_center = camera;
        bp.lod_radius = options_.lod_radius;
        world::svo::BuildStats stats;
        world::svo::BrickTree tree = world::svo::build_tree(sampler, g, bp, &pool_, &stats);

        LastBuild last;
        last.stats = stats;
        last.tree = tree.stats();
        last.bricks = tree.brick_count();
        last.memory_bytes = tree.memory_bytes();
        last.trees = sampler.trees().size();
        last.sampler_seconds = samplerSeconds;
        last.valid = true;

        const std::lock_guard guard(mutex_);
        finished_ = std::move(tree);
        lastBuild_ = last;
    } catch (const std::exception& e) {
        log(LogLevel::Error, "svo build failed: {}", e.what());
    }
    building_.store(false);
}

std::optional<world::svo::BrickTree> SvoWorld::take_finished() {
    const std::lock_guard guard(mutex_);
    std::optional<world::svo::BrickTree> out;
    if (finished_) {
        out = std::move(finished_);
        finished_.reset();
    }
    return out;
}

SvoWorld::LastBuild SvoWorld::last_build() const {
    const std::lock_guard guard(mutex_);
    return lastBuild_;
}

} // namespace app
