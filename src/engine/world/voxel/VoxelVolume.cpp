#include "engine/world/voxel/VoxelVolume.hpp"

#include <algorithm>
#include <cmath>

namespace biofuel::engine::world::voxel {

VoxelVolume::~VoxelVolume() noexcept {
    unload();
}

void VoxelVolume::unload() noexcept {
    if (m_loaded) {
        UnloadTexture(m_tex);
        m_tex = Texture2D{};
        m_loaded = false;
    }
    m_data.clear();
    m_hasCenter = false;
}

bool VoxelVolume::update(const VoxelWorld& world, const Vector3 playerPosition) {
    const i32 px = static_cast<i32>(std::floor(playerPosition.x));
    const i32 pz = static_cast<i32>(std::floor(playerPosition.z));
    if (m_loaded && m_hasCenter
        && std::abs(px - m_centerX) < m_config.recenterThreshold
        && std::abs(pz - m_centerZ) < m_config.recenterThreshold) {
        return false;
    }
    rebuild(world, px - m_config.width / 2, 0, pz - m_config.depth / 2);
    m_centerX = px;
    m_centerZ = pz;
    m_hasCenter = true;
    return true;
}

void VoxelVolume::rebuild(const VoxelWorld& world, const i32 originX, const i32 originY, const i32 originZ) {
    const i32 W = m_config.width;
    const i32 H = m_config.height;
    const i32 D = m_config.depth;
    m_originX = originX;
    m_originY = originY;
    m_originZ = originZ;

    const usize texW = static_cast<usize>(W);
    const usize texH = static_cast<usize>(H) * static_cast<usize>(D);
    m_data.assign(texW * texH, 0U);

    for (i32 z = 0; z < D; ++z) {
        for (i32 x = 0; x < W; ++x) {
            const i32 wx = originX + x;
            const i32 wz = originZ + z;
            const i32 h = world.surfaceHeight(wx, wz);
            // Cells well above the surface (and any tree canopy) are always Air;
            // skip them so the bake stays cheap.
            const i32 top = std::min(H - 1, h + 12);
            for (i32 y = 0; y <= top; ++y) {
                const Block b = world.blockAt(wx, originY + y, wz);
                if (b != Block::Air) {
                    m_data[(static_cast<usize>(y) + static_cast<usize>(z) * static_cast<usize>(H)) * texW
                           + static_cast<usize>(x)] = static_cast<u8>(b);
                }
            }
        }
    }

    Image img{};
    img.data = m_data.data();
    img.width = W;
    img.height = static_cast<i32>(texH);
    img.mipmaps = 1;
    img.format = PIXELFORMAT_UNCOMPRESSED_GRAYSCALE;   // 8-bit single channel (R8)

    if (!m_loaded) {
        m_tex = LoadTextureFromImage(img);
        SetTextureFilter(m_tex, TEXTURE_FILTER_POINT);
        m_loaded = true;
    } else {
        UpdateTexture(m_tex, m_data.data());            // same dimensions -> in-place re-upload
    }
}

} // namespace biofuel::engine::world::voxel
