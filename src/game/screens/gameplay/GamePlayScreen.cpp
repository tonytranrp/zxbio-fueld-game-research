#include "GamePlayScreen.hpp"

#include "engine/graphics/Render.hpp"
#include "engine/physics/PhysicsSystem.hpp"
#include "engine/runtime/Runtime.hpp"
#include "game/gameplay/SampleFarm.hpp"
#include <raylib.h>
#include <string_view>

namespace biofuel::game::screens {

using namespace ::biofuel::engine::physics;
using namespace ::biofuel::game::gameplay;

// =============================================================================
// WASD Direction helpers
// =============================================================================

namespace {

[[nodiscard]] presentation::sprites::Direction readWASDDirection() noexcept {
    using presentation::sprites::Direction;

    const bool w = IsKeyDown(KEY_W);
    const bool a = IsKeyDown(KEY_A);
    const bool s = IsKeyDown(KEY_S);
    const bool d = IsKeyDown(KEY_D);

    if (w && d)  return Direction::UpRight;
    if (w && a)  return Direction::UpLeft;
    if (s && d)  return Direction::DownRight;
    if (s && a)  return Direction::DownLeft;
    if (w)       return Direction::Up;
    if (s)       return Direction::Down;
    if (a)       return Direction::Left;
    if (d)       return Direction::Right;

    return Direction::Idle;
}

/// Convert NekoCat screen-pixel position to physics meters.
[[nodiscard]] constexpr Vector2 pixelsToMeters(f32 px, f32 py, f32 scale) noexcept {
    return Vector2{px / scale, py / scale};
}

/// Convert physics meters to NekoCat screen-pixel position.
[[nodiscard]] constexpr Vector2 metersToPixels(Vector2 meters, f32 scale) noexcept {
    return Vector2{meters.x * scale, meters.y * scale};
}

[[nodiscard]] constexpr Color tileColorFor(const TileType type) noexcept {
    const TileRenderColor color = tileRenderColor(type);
    return Color{color.r, color.g, color.b, color.a};
}

} // namespace

// =============================================================================
// Lifecycle
// =============================================================================

GamePlayScreen::GamePlayScreen() = default;

GamePlayScreen::~GamePlayScreen() noexcept {
    shutdownPhysicsWorld();
}

void GamePlayScreen::onEnter() {
    ensureHandTrackingForModelOverlay();
    m_handOverlay.onEnter();

    // Initialize NekoCat at screen center.
    m_neko.load();

    // Set up the physics world and farm grid.
    initPhysicsWorld();

    // Initialize NekoCat pixel position from physics.
    syncNekoCatFromPhysics();
}

void GamePlayScreen::onExit() {
    shutdownPhysicsWorld();
    m_neko.unload();
    m_handOverlay.onExit();
}

void GamePlayScreen::onUpdate(const f32 dt) {
    const presentation::sprites::Direction direction = readWASDDirection();

    // Apply velocity to the player physics body from WASD input.
    applyWASDVelocity(dt);

    // Step the physics simulation.
    if (m_physicsSystem) {
        m_physicsSystem->stepFixed(dt);
    }

    // Sync NekoCat's screen position from the physics body.
    syncNekoCatFromPhysics();

    // Update NekoCat animation with the WASD direction for sprite orientation.
    m_neko.update(dt, direction);

    m_handOverlay.update(dt);
}

void GamePlayScreen::onRender() {
    using namespace ::biofuel::engine::graphics;

    ClearBackground(Color{18, 24, 28, 255});

    Renderer::drawText(
        "FUEL FARM — Physics Test",
        20, 20, 24,
        Color{215, 190, 96, 255});

    // Render farm grid debug overlay
    if (m_farmState) {
        const usize w = m_farmState->width();
        const usize h = m_farmState->height();
        const f32 tilePx = m_metersToPixels; // 1 tile = 1 meter = tilePx pixels
        const i32 tilePxI32 = static_cast<i32>(tilePx);

        static constexpr Color kGridLineColor{60, 60, 60, 100};

        for (usize y = 0U; y < h; ++y) {
            const f32 ry = static_cast<f32>(y) * tilePx;
            const i32 ryI32 = static_cast<i32>(ry);
            for (usize x = 0U; x < w; ++x) {
                const Tile& tile = m_farmState->atUnsafe(x, y);
                const Color tileColor = tileColorFor(tile.type);

                const i32 rxI32 = static_cast<i32>(static_cast<f32>(x) * tilePx);
                DrawRectangle(rxI32, ryI32, tilePxI32, tilePxI32, tileColor);
                DrawRectangleLines(rxI32, ryI32, tilePxI32, tilePxI32, kGridLineColor);
            }
        }
    }

    // Render NekoCat at physics-driven position.
    m_neko.render();

    m_handOverlay.render();
}

void GamePlayScreen::onInput() {}

// =============================================================================
// Physics initialization
// =============================================================================

void GamePlayScreen::initPhysicsWorld() {
    m_physicsSystem = std::make_unique<PhysicsSystem>();
    m_physicsSystem->init();
    m_physicsSystem->setFixedTimestep(1.0f / 60.0f);
    m_physicsSystem->setMaxSubSteps(4);

    PhysicsWorld2D world = m_physicsSystem->world2D();
    world.setGravity(Vector2{0.0f, 0.0f}); // top-down — no gravity

    // Configure world-physics integration
    m_worldPhysics.setTileSizeMeters(1.0f);
    m_worldPhysics.setBuildingFootprint(2, 2);
    m_metersToPixels = 64.0f; // 64 pixels per meter

    // Create sample farm and bake colliders
    m_farmState = createSampleFarm();
    m_worldPhysics.bakeTileColliders(*m_farmState, world);

    // Create the player's dynamic physics body
    createPlayerBody();
}

void GamePlayScreen::shutdownPhysicsWorld() noexcept {
    if (m_physicsSystem) {
        PhysicsWorld2D world = m_physicsSystem->world2D();
        m_worldPhysics.clearAllColliders(world);
        if (m_playerBody) {
            world.removeBody(m_playerBody);
            m_playerBody = PhysicsBody2D{};
        }
        m_physicsSystem->shutdown();
        m_physicsSystem.reset();
    }
    m_farmState.reset();
}

void GamePlayScreen::createPlayerBody() {
    if (!m_physicsSystem) return;

    PhysicsWorld2D world = m_physicsSystem->world2D();

    // Place the player at tile (5, 5) center → physics (5.5, 5.5) meters.
    PhysicsBodyDesc2D desc{};
    desc.kind = PhysicsBodyKind::Dynamic;
    desc.position = Vector2{5.5f, 5.5f};
    desc.linearDamping = 8.0f;   // friction-like deceleration
    desc.canSleep = false;
    desc.lockRotation = true;

    m_playerBody = world.createBody(desc);

    // Attach a circle collider slightly smaller than a tile.
    CircleColliderDesc colliderDesc{};
    colliderDesc.radius = 0.35f;    // 70% of half-tile width
    colliderDesc.density = 1.0f;
    colliderDesc.sensor = false;
    colliderDesc.collisionGroup = CollisionGroup::all();

    [[maybe_unused]] const PhysicsCollider2D playerCollider = world.attachCircle(m_playerBody, colliderDesc);
}

// =============================================================================
// Per-frame sync
// =============================================================================

void GamePlayScreen::syncNekoCatFromPhysics() noexcept {
    if (!m_physicsSystem || !m_playerBody) return;

    const PhysicsWorld2D world = m_physicsSystem->world2D();
    const PhysicsBodyPose2D pose = world.bodyPose(m_playerBody);

    if (pose.valid) {
        const Vector2 pixelPos = metersToPixels(pose.position, m_metersToPixels);
        // Offset by half the sprite size so the sprite center aligns with the physics body.
        constexpr f32 kSpriteHalf = 32.0f * 3.0f * 0.5f; // half of 96px sprite
        m_neko.setPosition(pixelPos.x - kSpriteHalf, pixelPos.y - kSpriteHalf);
    }
}

void GamePlayScreen::applyWASDVelocity(const f32 dt) noexcept {
    if (!m_physicsSystem || !m_playerBody) return;

    const presentation::sprites::Direction dir = readWASDDirection();
    const PhysicsWorld2D world = m_physicsSystem->world2D();

    // Compute velocity vector from WASD input.
    f32 vx = 0.0f;
    f32 vy = 0.0f;

    switch (dir) {
    case presentation::sprites::Direction::Up:        vy = -1.0f; break;
    case presentation::sprites::Direction::Down:       vy =  1.0f; break;
    case presentation::sprites::Direction::Left:       vx = -1.0f; break;
    case presentation::sprites::Direction::Right:      vx =  1.0f; break;
    case presentation::sprites::Direction::UpLeft:     vx = -0.707f; vy = -0.707f; break;
    case presentation::sprites::Direction::UpRight:    vx =  0.707f; vy = -0.707f; break;
    case presentation::sprites::Direction::DownLeft:   vx = -0.707f; vy =  0.707f; break;
    case presentation::sprites::Direction::DownRight:  vx =  0.707f; vy =  0.707f; break;
    case presentation::sprites::Direction::Idle:       break;
    }

    const Vector2 velocity{
        vx * m_playerSpeed,
        vy * m_playerSpeed,
    };

    world.setBodyLinearVelocity(m_playerBody, velocity);

    // Suppress unused warning when dt is not used (velocity is absolute, not accumulated).
    (void)dt;
}

// =============================================================================
// Hand tracking
// =============================================================================

void GamePlayScreen::ensureHandTrackingForModelOverlay() {
    game::presentation::hands::ensureModelOnlyHandTracking();
}

} // namespace biofuel::game::screens
