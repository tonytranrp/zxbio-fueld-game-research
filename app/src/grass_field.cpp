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
                    // The same fan the voxel tier builds, from the same id and the same arithmetic
                    // (world/generation/grass_cover.cpp's `grass_patch_volume`). Two tiers drawing
                    // the same tuft in two different places is exactly the ring goal 340 forbids.
                    const float turn = 6.2831853f * (static_cast<float>(b) / static_cast<float>(blades) +
                                                     static_cast<float>(tuft.id & 0xFFu) / 256.0f);
                    render::diligent::GrassBladeInstance inst;
                    inst.base_height =
                        glm::vec4(tuft.base.x + 0.25f * cover.tuft_radius_m * std::cos(turn), tuft.base.y,
                                  tuft.base.z + 0.25f * cover.tuft_radius_m * std::sin(turn), tuft.height);
                    // The lean carries the fan direction as well as the tuft's own tilt, so a raster
                    // blade ends where the voxel capsule of the same index ends.
                    inst.lean_width_phase =
                        glm::vec4(tuft.lean_x + cover.tuft_radius_m * std::cos(turn) / tuft.height,
                                  tuft.lean_z + cover.tuft_radius_m * std::sin(turn) / tuft.height,
                                  2.0f * cover.blade_radius_m,
                                  // A per-tuft wind phase, from the same id: neighbours are not in
                                  // lockstep, and the offset is reproducible in every tier.
                                  static_cast<float>(tuft.id & 0xFFFFu) / 65535.0f * 6.0f);
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
