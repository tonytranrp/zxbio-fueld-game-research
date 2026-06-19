#pragma once

#include "engine/core/Types.hpp"
#include <raylib.h>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace biofuel::engine::world::voxel {

// Block materials. Air is empty space (never meshed).
enum class Block : u8 {
    Air = 0,
    Grass,
    Dirt,
    Stone,
    Sand,
    Snow,
    Wood,
    Leaves,
};

// =============================================================================
// VoxelWorld — an infinite, chunked, Minecraft-style block world.
//
// Terrain is a pure deterministic function of world position (noise heightmap),
// so chunks generate and mesh independently. Each chunk becomes ONE GPU mesh
// built with hidden-face culling (only faces touching air are emitted). Chunks
// stream in/out around the player every frame within a view radius.
//
// Engine-owned (raw Raylib mesh/model lifetime lives in the .cpp).
// =============================================================================
class VoxelWorld {
public:
    static constexpr i32 kChunkSize = 16;   // blocks per chunk on X and Z

    struct Config {
        i32 viewRadiusChunks = 6;   // chunks kept meshed around the player
        i32 maxBuildsPerFrame = 4;  // chunk meshes built per frame (anti-hitch)
        u32 seed = 1337U;
        f32 frequency = 0.013f;     // base noise frequency (smaller = larger hills)
        i32 baseHeight = 22;        // average surface height
        i32 amplitude = 26;         // peak-to-trough variation
        i32 octaves = 4;
        f32 persistence = 0.5f;
        i32 seaLevel = 23;          // valleys below this flood with water
        f32 waveAmplitude = 0.12f;  // water surface bob height
        f32 waveSpeed = 1.4f;       // water bob speed
    };

    VoxelWorld() = default;
    ~VoxelWorld() noexcept;
    VoxelWorld(const VoxelWorld&) = delete;
    VoxelWorld& operator=(const VoxelWorld&) = delete;

    void configure(const Config& config) noexcept { m_config = config; }
    [[nodiscard]] const Config& config() const noexcept { return m_config; }

    // Stream chunks around the player: generate/mesh nearby missing chunks
    // (bounded per frame) and unload chunks beyond the view radius.
    void update(Vector3 playerPosition);

    // Draw all loaded chunks. Must run between BeginMode3D / EndMode3D.
    void render() const noexcept;

    // Draw the translucent water sheets. Call after render(), still between
    // BeginMode3D / EndMode3D. timeSeconds drives the wave bob.
    void renderWater(f32 timeSeconds) const noexcept;

    // Release every chunk mesh.
    void unloadAll() noexcept;

    // World Y the player stands on at world (x, z): top of the surface block.
    [[nodiscard]] f32 groundHeight(f32 worldX, f32 worldZ) const noexcept;

    [[nodiscard]] usize loadedChunkCount() const noexcept { return m_chunks.size(); }
    [[nodiscard]] usize lastBuiltThisFrame() const noexcept { return m_builtThisFrame; }

    // Terrain sampling — pure deterministic functions. Public so a GPU volume
    // (VoxelVolume) can be baked from the same world definition the mesher uses.
    [[nodiscard]] Block blockAt(i32 worldX, i32 worldY, i32 worldZ) const noexcept;
    [[nodiscard]] i32 surfaceHeight(i32 worldX, i32 worldZ) const noexcept;

private:
    struct Chunk {
        i32 cx = 0;
        i32 cz = 0;
        Model model{};
        bool hasMesh = false;
        Model waterModel{};
        bool hasWater = false;
    };

    using ChunkKey = std::int64_t;
    [[nodiscard]] static ChunkKey keyOf(i32 cx, i32 cz) noexcept {
        return (static_cast<std::int64_t>(cx) << 32) ^ (static_cast<std::int64_t>(static_cast<std::uint32_t>(cz)));
    }

    void buildChunkMesh(Chunk& chunk) const;
    void buildChunkWater(Chunk& chunk) const;
    void destroyChunk(Chunk& chunk) noexcept;
    void ensureAtlas();           // lazily build the pixel-art block texture atlas
    void destroyAtlas() noexcept;

    Config m_config{};
    std::unordered_map<ChunkKey, Chunk> m_chunks;
    usize m_builtThisFrame = 0;
    Texture2D m_atlas{};          // pixel-art block textures (point-filtered)
    bool m_atlasReady = false;
};

} // namespace biofuel::engine::world::voxel
