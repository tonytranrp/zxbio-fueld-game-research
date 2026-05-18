#include "engine/world/HeightmapWorld3D.hpp"

#include <algorithm>
#include <cmath>

namespace biofuel::engine::world {

// =============================================================================
// OrbitCamera
// =============================================================================

void OrbitCamera::recompute() noexcept {
    const f32 azRad = azimuthDeg * (3.14159265f / 180.0f);
    const f32 elRad = elevationDeg * (3.14159265f / 180.0f);

    const f32 x = target.x + distance * std::cos(elRad) * std::sin(azRad);
    const f32 y = target.y + distance * std::sin(elRad);
    const f32 z = target.z + distance * std::cos(elRad) * std::cos(azRad);

    camera.position = Vector3{x, y, z};
    camera.target = target;
    camera.up = Vector3{0.0f, 1.0f, 0.0f};
    camera.fovy = fovyDeg;
    camera.projection = CAMERA_PERSPECTIVE;
}

void OrbitCamera::handleInput(const f32 dt) noexcept {
    // --- Right mouse button drag: orbit ---
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        const Vector2 delta = GetMouseDelta();
        azimuthDeg -= delta.x * rotateSpeed * dt;
        elevationDeg += delta.y * rotateSpeed * dt;
        elevationDeg = std::clamp(elevationDeg, minElevation, maxElevation);
    }

    // --- Scroll wheel: zoom ---
    const f32 scroll = GetMouseWheelMove();
    if (scroll != 0.0f) {
        distance -= scroll * zoomSpeed;
        distance = std::clamp(distance, minDistance, maxDistance);
    }

    // --- Arrow keys: pan target ---
    const f32 panSpeed = 10.0f * dt;
    if (IsKeyDown(KEY_LEFT))  { target.x -= panSpeed; }
    if (IsKeyDown(KEY_RIGHT)) { target.x += panSpeed; }
    if (IsKeyDown(KEY_UP))    { target.z -= panSpeed; }
    if (IsKeyDown(KEY_DOWN))  { target.z += panSpeed; }

    recompute();
}

// =============================================================================
// HeightmapWorld3D
// =============================================================================

void HeightmapWorld3D::generate(
    const HeightmapData& heightmap,
    const BiomeMap& biomes,
    physics::PhysicsWorld3D& physicsWorld)
{
    m_width = heightmap.width;
    m_height = heightmap.height;

    // --- Center the camera target on the world ---
    m_orbitCamera.target = {
        static_cast<f32>(m_width) * 0.5f,
        0.0f,
        static_cast<f32>(m_height) * 0.5f,
    };
    m_orbitCamera.distance = std::max(
        static_cast<f32>(m_width), static_cast<f32>(m_height)) * 1.2f;
    m_orbitCamera.recompute();

    // --- Initialize chunk renderer ---
    m_chunkRenderer.init(m_width, m_height);
    m_chunkRenderer.populateFromHeightmap(heightmap, biomes);

    // --- Create per-tile physics bodies ---
    const usize tileCount = static_cast<usize>(m_width) * static_cast<usize>(m_height);
    m_tiles.clear();
    m_tiles.reserve(tileCount);

    for (i32 z = 0; z < m_height; ++z) {
        for (i32 x = 0; x < m_width; ++x) {
            const f32 h = heightmap.at(x, z);
            const BiomeMaterial mat = biomes.at(x, z).material;

            WorldTile3D tile;
            tile.position = {
                static_cast<f32>(x) + 0.5f,
                h * 0.5f,
                static_cast<f32>(z) + 0.5f,
            };
            tile.height = h;
            tile.color = biomeColor(mat);

            // Create a fixed physics body at the tile position
            physics::PhysicsBodyDesc3D bodyDesc;
            bodyDesc.kind = physics::PhysicsBodyKind::Fixed;
            bodyDesc.position = tile.position;
            bodyDesc.canSleep = true;

            tile.body = physicsWorld.createBody(bodyDesc);

            // Attach a cuboid collider
            physics::CuboidColliderDesc colliderDesc;
            colliderDesc.halfExtents = {
                0.5f,
                h * 0.5f,
                0.5f,
            };
            colliderDesc.density = 0.0f; // static body, density irrelevant

            tile.collider = physicsWorld.attachCuboid(tile.body, colliderDesc);
            tile.hasPhysics = true;

            m_tiles.push_back(tile);
        }
    }
}

void HeightmapWorld3D::destroyPhysics(physics::PhysicsWorld3D& physicsWorld) {
    for (auto& tile : m_tiles) {
        if (tile.hasPhysics) {
            if (tile.collider) {
                // Colliders are implicitly removed with body; just clear handle
            }
            if (tile.body) {
                physicsWorld.removeBody(tile.body);
            }
            tile.hasPhysics = false;
        }
    }
    m_tiles.clear();
}

void HeightmapWorld3D::renderSolid() const {
    m_chunkRenderer.renderSolid();
}

void HeightmapWorld3D::renderWireframe() const {
    m_chunkRenderer.renderWireframe();
}

void HeightmapWorld3D::updateVisibility() {
    m_chunkRenderer.updateVisibility(
        m_orbitCamera.camera.position,
        m_orbitCamera.distance * 2.0f);
}

void HeightmapWorld3D::updateCamera(const f32 dt) {
    m_orbitCamera.handleInput(dt);
}

Color HeightmapWorld3D::biomeColor(const BiomeMaterial mat) noexcept {
    // Mirror VoxelChunkRenderer::biomeColor to stay consistent
    switch (mat) {
    case BiomeMaterial::DeepWater:    return Color{ 18,  72, 140, 255};
    case BiomeMaterial::ShallowWater: return Color{ 40, 120, 180, 255};
    case BiomeMaterial::Sand:         return Color{238, 214, 175, 255};
    case BiomeMaterial::Grass:        return Color{ 76, 153,   0, 255};
    case BiomeMaterial::Forest:       return Color{ 34, 102,  34, 255};
    case BiomeMaterial::Dirt:         return Color{139,  90,  43, 255};
    case BiomeMaterial::Stone:        return Color{128, 128, 128, 255};
    case BiomeMaterial::Snow:         return Color{245, 245, 250, 255};
    }
    return Color{200, 200, 200, 255};
}

} // namespace biofuel::engine::world
