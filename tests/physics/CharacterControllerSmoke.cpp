#include "engine/events/EventManager.hpp"
#include "engine/physics/PhysicsSystem.hpp"
#include <cstdlib>
#include <iostream>

namespace {

bool check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        return false;
    }
    return true;
}

} // namespace

int main() {
    using namespace ::biofuel::engine::physics;
    using biofuel::f32;

    ::biofuel::engine::events::EventManager::instance().init();

    PhysicsSystem physics;
    physics.init();
    auto world = physics.world3D();
    world.setGravity(Vector3{0.0f, -9.8f, 0.0f});

    constexpr f32 kDt = 1.0f / 60.0f;
    constexpr f32 kCapsuleHalfHeight = 0.5f;
    constexpr f32 kCapsuleRadius = 0.35f;
    const CharacterControllerDesc3D desc{}; // defaults: autostep off, snap-to-ground on

    // --- Grounded detection: capsule resting just above a fixed floor ---
    const PhysicsBody3D floorBody = world.createBody({.kind = PhysicsBodyKind::Fixed, .position = Vector3{0.0f, -0.25f, 0.0f}});
    const PhysicsCollider3D floorCollider = world.attachCuboid(floorBody, {.halfExtents = Vector3{10.0f, 0.25f, 10.0f}});

    const PhysicsBody3D characterBody = world.createBody({.kind = PhysicsBodyKind::KinematicPosition, .position = Vector3{0.0f, 1.0f, 0.0f}});
    const PhysicsCollider3D characterCollider = world.attachCapsule(characterBody, {.halfHeight = kCapsuleHalfHeight, .radius = kCapsuleRadius});

    bool ok = true;
    ok = check(static_cast<bool>(floorCollider), "floor collider was not created") && ok;
    ok = check(static_cast<bool>(characterCollider), "character collider was not created") && ok;

    // Capsule bottom sits at y=0 (top of floor) when its center is at
    // halfHeight+radius = 0.85. Fall it there with repeated downward moves,
    // same shape as an idle KCC tick with only gravity applied.
    //
    // stepFixed() must run each tick even though the character itself moves
    // via moveCharacter(), not the pipeline: static colliders (the floor,
    // later the wall) are only inserted into the broad-phase's query
    // structure as part of a pipeline step -- PhysicsSmoke.cpp's raycast
    // test needs the same 180-step warmup before its first raycast hits
    // anything, for the same reason. setBodyPosition keeps the character's
    // actual Rapier body in sync with our locally-tracked position, matching
    // what a real caller (CharacterController3D) does every tick.
    Vector3 pos{0.0f, 1.0f, 0.0f};
    bool grounded = false;
    for (int i = 0; i < 120 && !grounded; ++i) {
        physics.stepFixed(kDt);
        const CharacterMovement3D move = world.moveCharacter(
            characterCollider, characterBody, pos, Vector3{0.0f, -9.8f * kDt, 0.0f}, kDt, desc);
        ok = check(move.valid, "moveCharacter returned invalid for a valid grounded-fall step") && ok;
        pos.x += move.translation.x;
        pos.y += move.translation.y;
        pos.z += move.translation.z;
        world.setBodyPosition(characterBody, pos);
        grounded = move.grounded;
    }
    ok = check(grounded, "character never reported grounded after falling onto a fixed floor") && ok;

    // --- Wall blocking: walking straight into a wall should not fully penetrate it ---
    const PhysicsBody3D wallBody = world.createBody({.kind = PhysicsBodyKind::Fixed, .position = Vector3{3.0f, 1.0f, 0.0f}});
    const PhysicsCollider3D wallCollider = world.attachCuboid(wallBody, {.halfExtents = Vector3{0.25f, 2.0f, 10.0f}});
    ok = check(static_cast<bool>(wallCollider), "wall collider was not created") && ok;

    const Vector3 desiredIntoWall{1.0f, 0.0f, 0.0f}; // 1 m desired step directly toward the wall
    Vector3 wallPos{0.0f, pos.y, 0.0f};
    world.setBodyPosition(characterBody, wallPos);
    f32 totalMovedTowardWall = 0.0f;
    for (int i = 0; i < 300; ++i) {
        physics.stepFixed(kDt); // inserts the just-created wall collider into the broad-phase
        const CharacterMovement3D move = world.moveCharacter(
            characterCollider, characterBody, wallPos, desiredIntoWall, kDt, desc);
        ok = check(move.valid, "moveCharacter returned invalid during wall-approach step") && ok;
        wallPos.x += move.translation.x;
        wallPos.y += move.translation.y;
        wallPos.z += move.translation.z;
        world.setBodyPosition(characterBody, wallPos);
        totalMovedTowardWall += move.translation.x;
    }
    // Wall face is at x=2.75 (3.0 - 0.25 half-extent); capsule radius 0.35 plus a
    // small offset means the center should stop noticeably short of the wall's
    // far side (x=3.25) instead of tunnelling through to 300 * desiredIntoWall.x.
    ok = check(wallPos.x < 3.0f, "character penetrated through the wall instead of being blocked") && ok;
    ok = check(totalMovedTowardWall > 1.0f, "character did not move toward the wall at all") && ok;

    // --- Invalid-input handling: must report invalid, never crash ---
    const CharacterMovement3D zeroDt = world.moveCharacter(
        characterCollider, characterBody, pos, Vector3{1.0f, 0.0f, 0.0f}, 0.0f, desc);
    ok = check(!zeroDt.valid, "dt <= 0 did not report invalid") && ok;

    const CharacterMovement3D badCollider = world.moveCharacter(
        PhysicsCollider3D{999999}, PhysicsBody3D{}, pos, Vector3{1.0f, 0.0f, 0.0f}, kDt, desc);
    ok = check(!badCollider.valid, "nonexistent collider handle did not report invalid") && ok;

    const CharacterMovement3D badOffset = world.moveCharacter(
        characterCollider, characterBody, pos, Vector3{1.0f, 0.0f, 0.0f}, kDt,
        CharacterControllerDesc3D{.offset = 0.0f});
    ok = check(!badOffset.valid, "zero offset (must be > 0 for numerical stability) did not report invalid") && ok;

    physics.shutdown();
    ::biofuel::engine::events::EventManager::instance().shutdown();
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
