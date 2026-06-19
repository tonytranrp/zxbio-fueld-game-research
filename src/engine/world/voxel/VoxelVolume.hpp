#pragma once

#include "engine/core/Types.hpp"
#include "engine/world/voxel/VoxelWorld.hpp"
#include <raylib.h>
#include <vector>

namespace biofuel::engine::world::voxel {

// =============================================================================
// VoxelVolume — a bounded dense voxel grid around the player, baked from
// VoxelWorld's terrain and uploaded to the GPU for the fullscreen raymarcher.
//
// GL 3.3 (via raylib) has no 3D textures, so the W*H*D grid is flattened into a
// 2D R8 texture of size W x (H*D): voxel (x,y,z) lives at texel (x, y + z*H),
// value = Block id. The volume re-centres (rebuild + re-upload) when the player
// moves past a threshold so the world appears effectively infinite within the
// bounded view distance.
// =============================================================================
class VoxelVolume {
public:
    struct Config {
        i32 width = 96;
        i32 height = 64;
        i32 depth = 96;
        i32 recenterThreshold = 16;   // re-bake after the player moves this many blocks
    };

    VoxelVolume() = default;
    ~VoxelVolume() noexcept;
    VoxelVolume(const VoxelVolume&) = delete;
    VoxelVolume& operator=(const VoxelVolume&) = delete;

    void configure(const Config& config) noexcept { m_config = config; }

    // Re-bake + re-upload the volume if the player has moved far from its centre
    // (or on the first call). Returns true if the texture was (re)built.
    bool update(const VoxelWorld& world, Vector3 playerPosition);

    void unload() noexcept;

    [[nodiscard]] bool ready() const noexcept { return m_loaded; }
    [[nodiscard]] Texture2D texture() const noexcept { return m_tex; }
    [[nodiscard]] Vector3 originWorld() const noexcept {
        return Vector3{static_cast<f32>(m_originX), static_cast<f32>(m_originY), static_cast<f32>(m_originZ)};
    }
    [[nodiscard]] i32 width() const noexcept { return m_config.width; }
    [[nodiscard]] i32 height() const noexcept { return m_config.height; }
    [[nodiscard]] i32 depth() const noexcept { return m_config.depth; }

private:
    void rebuild(const VoxelWorld& world, i32 originX, i32 originY, i32 originZ);

    Config m_config{};
    std::vector<u8> m_data;   // W * (H*D) block ids, row-major for the 2D texture
    Texture2D m_tex{};
    bool m_loaded = false;
    i32 m_originX = 0;
    i32 m_originY = 0;
    i32 m_originZ = 0;
    i32 m_centerX = 0;
    i32 m_centerZ = 0;
    bool m_hasCenter = false;
};

} // namespace biofuel::engine::world::voxel
