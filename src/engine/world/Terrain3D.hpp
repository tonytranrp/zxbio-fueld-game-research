#pragma once

#include "engine/core/Types.hpp"
#include <raylib.h>
#include <vector>

namespace biofuel::engine::world {

// =============================================================================
// Terrain3D — a single procedurally generated, walkable terrain surface.
//
// Owns one GPU mesh (built from layered value-noise) plus the CPU-side height
// field used for ground queries. Vertices carry height-based colors and proper
// normals so the hills read as 3D even under the default unlit shader. Raw
// Raylib model lifetime lives here (engine-owned) so game code never touches it.
// =============================================================================
class Terrain3D {
public:
    struct Config {
        i32 width = 96;          // world span along X (cells)
        i32 depth = 96;          // world span along Z (cells)
        f32 maxHeight = 16.0f;   // peak terrain height in world units
        f32 frequency = 0.045f;  // base noise frequency
        u32 octaves = 4U;        // fractal layers
        f32 persistence = 0.5f;  // amplitude falloff per octave
        u32 seed = 1337U;
        Color lowColor{74, 122, 58, 255};    // grassy valleys
        Color midColor{96, 142, 66, 255};    // hillsides
        Color highColor{150, 140, 116, 255}; // rocky tops
        Color peakColor{226, 230, 235, 255}; // snow caps
    };

    Terrain3D() = default;
    ~Terrain3D() noexcept;
    Terrain3D(const Terrain3D&) = delete;
    Terrain3D& operator=(const Terrain3D&) = delete;
    Terrain3D(Terrain3D&&) = delete;
    Terrain3D& operator=(Terrain3D&&) = delete;

    // Build the height field and upload the terrain mesh. Replaces any existing
    // terrain.
    void generate(const Config& config);

    // Release the GPU mesh. Safe to call when nothing is loaded.
    void unload() noexcept;

    // Bilinear-interpolated ground height at world (x, z). Clamped to bounds so
    // it is always safe to call. Matches the rendered surface.
    [[nodiscard]] f32 heightAt(f32 worldX, f32 worldZ) const noexcept;

    // Draw the terrain. Must be called between BeginMode3D / EndMode3D.
    void render() const noexcept;

    [[nodiscard]] f32 worldWidth() const noexcept { return static_cast<f32>(m_config.width); }
    [[nodiscard]] f32 worldDepth() const noexcept { return static_cast<f32>(m_config.depth); }
    [[nodiscard]] bool ready() const noexcept { return m_loaded; }

private:
    [[nodiscard]] f32 sampleHeight(i32 col, i32 row) const noexcept;

    Config m_config{};
    std::vector<f32> m_heights;   // (cols * rows) world-unit heights, row-major
    i32 m_cols = 0;               // width + 1 vertices along X
    i32 m_rows = 0;               // depth + 1 vertices along Z
    Model m_model{};
    bool m_loaded = false;
};

} // namespace biofuel::engine::world
