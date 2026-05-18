#pragma once

#include "engine/core/Types.hpp"
#include "engine/physics/PhysicsSystem.hpp"
#include "engine/world/TerrainGenerator.hpp"
#include "engine/world/VoxelChunkRenderer.hpp"
#include <raylib.h>
#include <vector>

namespace biofuel::engine::world {

// =============================================================================
// OrbitCamera — a simple orbital camera controller for viewing the 3D world.
// The camera orbits a target point; scroll wheel adjusts distance, arrow keys
// or mouse drag adjusts azimuth/elevation.
// =============================================================================
struct OrbitCamera {
    Camera3D camera{};
    Vector3 target{0.0f, 0.0f, 0.0f};
    f32 distance = 30.0f;
    f32 azimuthDeg = 45.0f;     // horizontal rotation around Y axis
    f32 elevationDeg = 35.0f;   // vertical angle from XZ plane
    f32 minDistance = 5.0f;
    f32 maxDistance = 100.0f;
    f32 minElevation = 5.0f;
    f32 maxElevation = 85.0f;
    f32 zoomSpeed = 2.0f;
    f32 rotateSpeed = 90.0f;    // degrees per second
    f32 fovyDeg = 60.0f;
    f32 nearPlane = 0.1f;
    f32 farPlane = 500.0f;

    // -------------------------------------------------------------------------
    // Recompute camera.position from target + spherical coords; update
    // camera struct. Call after changing azimuth, elevation, or distance.
    // -------------------------------------------------------------------------
    void recompute() noexcept;

    // -------------------------------------------------------------------------
    // Handle mouse input for orbit: right-drag to rotate, scroll to zoom.
    // dt is frame delta time in seconds.
    // -------------------------------------------------------------------------
    void handleInput(f32 dt) noexcept;
};

// =============================================================================
// WorldTile3D — runtime data for one extruded tile.
// =============================================================================
struct WorldTile3D {
    Vector3 position{0.0f, 0.0f, 0.0f};   // center of cuboid
    f32 height = 0.0f;
    Color color{200, 200, 200, 255};
    physics::PhysicsBody3D body{};
    physics::PhysicsCollider3D collider{};
    bool hasPhysics = false;
};

// =============================================================================
// HeightmapWorld3D — the main 2.5D world module.
//
// Takes a HeightmapData + BiomeMap and extrudes each grid cell into a 3D
// cuboid with:
//   - A fixed physics body + cuboid collider for collision detection
//   - A color-matched cube rendered through VoxelChunkRenderer
//   - An orbit camera for viewing
//
// Supports switching between 2D top-down and 3D perspective rendering.
// =============================================================================
class HeightmapWorld3D {
public:
    HeightmapWorld3D() = default;

    // -------------------------------------------------------------------------
    // Generate the full 3D world from a heightmap and biome map.
    // Creates physics bodies for every tile via the supplied PhysicsWorld3D.
    // Populates the internal VoxelChunkRenderer for efficient drawing.
    // -------------------------------------------------------------------------
    void generate(
        const HeightmapData& heightmap,
        const BiomeMap& biomes,
        physics::PhysicsWorld3D& physicsWorld);

    // -------------------------------------------------------------------------
    // Destroy all physics bodies created by this world.
    // -------------------------------------------------------------------------
    void destroyPhysics(physics::PhysicsWorld3D& physicsWorld);

    // -------------------------------------------------------------------------
    // Render the solid world using the chunk renderer.
    // Call between BeginMode3D / EndMode3D.
    // -------------------------------------------------------------------------
    void renderSolid() const;

    // -------------------------------------------------------------------------
    // Render the world as wireframe cubes.
    // Call between BeginMode3D / EndMode3D.
    // -------------------------------------------------------------------------
    void renderWireframe() const;

    // -------------------------------------------------------------------------
    // Update chunk visibility based on camera position.
    // -------------------------------------------------------------------------
    void updateVisibility();

    // -------------------------------------------------------------------------
    // Camera access
    // -------------------------------------------------------------------------
    [[nodiscard]] Camera3D& camera() noexcept { return m_orbitCamera.camera; }
    [[nodiscard]] const Camera3D& camera() const noexcept { return m_orbitCamera.camera; }
    [[nodiscard]] OrbitCamera& orbit() noexcept { return m_orbitCamera; }
    [[nodiscard]] const OrbitCamera& orbit() const noexcept { return m_orbitCamera; }

    void updateCamera(f32 dt);

    // -------------------------------------------------------------------------
    // 2D / 3D toggle
    // -------------------------------------------------------------------------
    [[nodiscard]] bool is3DMode() const noexcept { return m_3dMode; }
    void set3DMode(bool enabled) noexcept { m_3dMode = enabled; }

    // -------------------------------------------------------------------------
    // Query
    // -------------------------------------------------------------------------
    [[nodiscard]] i32 worldWidth() const noexcept { return m_width; }
    [[nodiscard]] i32 worldHeight() const noexcept { return m_height; }
    [[nodiscard]] const VoxelChunkRenderer& renderer() const noexcept { return m_chunkRenderer; }
    [[nodiscard]] usize physicsBodyCount() const noexcept { return m_tiles.size(); }

private:
    [[nodiscard]] static Color biomeColor(BiomeMaterial mat) noexcept;

    std::vector<WorldTile3D> m_tiles;
    i32 m_width = 0;
    i32 m_height = 0;
    bool m_3dMode = true;

    OrbitCamera m_orbitCamera;
    VoxelChunkRenderer m_chunkRenderer;
};

} // namespace biofuel::engine::world
