#include "grass_field.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

#include "world/generation/field/field_sampler.hpp"

namespace app {

void GrassField::refresh(glm::vec3 camera) {
    if (!options_.enabled || options_.radius_m <= 0.0f) {
        blades_.clear();
        tufts_ = 0;
        return;
    }
    if (built_ &&
        glm::length(glm::vec2(camera.x - builtAt_.x, camera.z - builtAt_.z)) < 0.25f * options_.radius_m) {
        return;
    }
    const auto start = std::chrono::steady_clock::now();

    // The SAME callables the sampler hands `grass_tufts_in_patch`. Sharing the call is the point;
    // sharing the inputs to it is what makes the sharing mean anything.
    const auto height_at = [this](float x, float z) { return heightmap_->height_at(x, z); };
    const auto slope_at = [this](float x, float z) {
        const glm::vec2 g = heightmap_->slope_at(x, z);
        return std::max(std::abs(g.x), std::abs(g.y));
    };
    const world::generation::field::TerrainField* macro = heightmap_->macro_field();
    const auto biome_at = [macro](float x, float z) {
        if (macro == nullptr) {
            return world::generation::field::Biome::Grassland;
        }
        const auto index = static_cast<std::uint8_t>(std::lround(
            world::generation::field::FieldSampler{*macro, world::generation::field::Plane::Biome}.value_at(
                x, z)));
        return index < static_cast<std::uint8_t>(world::generation::field::Biome::Count)
                   ? static_cast<world::generation::field::Biome>(index)
                   : world::generation::field::Biome::Grassland;
    };

    world::generation::GrassCoverParams cover = cover_;
    cover.plants_per_tuft = std::max(0.01f, cover_.plants_per_tuft / std::max(0.01f, options_.density_scale));

    blades_.clear();
    tufts_ = 0;
    truncated_ = false;
    const float radius = options_.radius_m;
    const auto patchOf = [&](float v) {
        return static_cast<std::int32_t>(std::floor(v / cover.patch_edge_m));
    };
    const auto blades = static_cast<std::size_t>(std::max(1, cover.blades_per_tuft));

    for (std::int32_t pz = patchOf(camera.z - radius); pz <= patchOf(camera.z + radius); ++pz) {
        for (std::int32_t px = patchOf(camera.x - radius); px <= patchOf(camera.x + radius); ++px) {
            const glm::vec2 origin{static_cast<float>(px) * cover.patch_edge_m,
                                   static_cast<float>(pz) * cover.patch_edge_m};
            for (const world::generation::GrassTuft& tuft : world::generation::grass_tufts_in_patch(
                     seed_, origin, cover, height_at, slope_at, biome_at)) {
                const float dx = tuft.base.x - camera.x;
                const float dz = tuft.base.z - camera.z;
                if (dx * dx + dz * dz > radius * radius) {
                    continue;
                }
                if (blades_.size() + blades > options_.max_blades) {
                    truncated_ = true;
                    break;
                }
                ++tufts_;
                for (std::size_t b = 0; b < blades; ++b) {
                    // THE SHARED CALL (goal 340). The voxel tier builds its capsule from this same
                    // function, so the two tiers cannot place the same tuft's blade in two places.
                    const world::generation::GrassBladeSegment seg = world::generation::grass_blade(
                        tuft, static_cast<int>(b), static_cast<int>(blades), cover);
                    const glm::vec3 axis = seg.end - seg.start;
                    render::diligent::GrassBladeInstance inst;
                    inst.base_height = glm::vec4(seg.start, axis.y);
                    // The raster blade's lean is the shared segment's own horizontal run over its
                    // rise, so its tip lands where the capsule's does.
                    const float rise = std::max(axis.y, 1.0e-4f);
                    inst.lean_width_phase = glm::vec4(axis.x / rise, axis.z / rise, 2.0f * seg.radius,
                                                      world::generation::grass_wind_phase(tuft));
                    blades_.push_back(inst);
                }
            }
            if (truncated_) {
                break;
            }
        }
        if (truncated_) {
            break;
        }
    }

    builtAt_ = camera;
    built_ = true;
    buildSeconds_ = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

} // namespace app
