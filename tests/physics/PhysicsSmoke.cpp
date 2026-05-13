#include "engine/events/EventManager.hpp"
#include "engine/custom/procedural/hand/HandPhysicsInteraction.hpp"
#include "engine/physics/PhysicsSystem.hpp"
#include <array>
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
    using ::biofuel::engine::custom::procedural::hand::HandPhysicsInteraction3D;
    using ::biofuel::engine::custom::procedural::hand::HandSide;
    using ::biofuel::engine::custom::procedural::hand::TrackedRobotHandPose;

    ::biofuel::engine::events::EventManager::instance().init();

    PhysicsSystem physics;
    physics.init();

    auto world2D = physics.world2D();
    world2D.setGravity(Vector2{0.0f, -9.8f});
    const PhysicsBody2D ground2D = world2D.createBody({.kind = PhysicsBodyKind::Fixed, .position = Vector2{0.0f, -1.0f}});
    const PhysicsCollider2D groundCollider2D = world2D.attachBox(ground2D, {.halfExtents = Vector2{4.0f, 0.25f}});
    const PhysicsBody2D ball2D = world2D.createBody({.kind = PhysicsBodyKind::Dynamic, .position = Vector2{0.0f, 1.0f}});
    const PhysicsCollider2D ballCollider2D = world2D.attachCircle(ball2D, {.radius = 0.25f});

    auto world3D = physics.world3D();
    world3D.setGravity(Vector3{0.0f, -9.8f, 0.0f});
    const PhysicsBody3D ground3D = world3D.createBody({.kind = PhysicsBodyKind::Fixed, .position = Vector3{0.0f, -1.0f, 0.0f}});
    const PhysicsCollider3D groundCollider3D = world3D.attachCuboid(ground3D, {.halfExtents = Vector3{4.0f, 0.25f, 4.0f}});
    const PhysicsBody3D ball3D = world3D.createBody({.kind = PhysicsBodyKind::Dynamic, .position = Vector3{0.0f, 1.0f, 0.0f}});
    const PhysicsCollider3D ballCollider3D = world3D.attachBall(ball3D, {.radius = 0.25f});

    bool ok = true;
    ok = check(static_cast<bool>(groundCollider2D), "2D ground collider was not created") && ok;
    ok = check(static_cast<bool>(ballCollider2D), "2D ball collider was not created") && ok;
    ok = check(static_cast<bool>(groundCollider3D), "3D ground collider was not created") && ok;
    ok = check(static_cast<bool>(ballCollider3D), "3D ball collider was not created") && ok;

    bool sawContact = false;
    for (int i = 0; i < 180; ++i) {
        physics.stepFixed(1.0f / 60.0f);
        sawContact = sawContact || !physics.recentContacts().empty();
    }

    const auto pose2D = world2D.bodyPose(ball2D);
    const auto pose3D = world3D.bodyPose(ball3D);
    const std::array<PhysicsBody2D, 2> bodies2D{{ground2D, ball2D}};
    std::array<PhysicsBodyPose2D, 2> poses2D{};
    const std::array<PhysicsBody3D, 2> bodies3D{{ground3D, ball3D}};
    std::array<PhysicsBodyPose3D, 2> poses3D{};
    ok = check(world2D.bodyPoses(bodies2D, poses2D) == poses2D.size(), "2D batch pose readback count failed") && ok;
    ok = check(world3D.bodyPoses(bodies3D, poses3D) == poses3D.size(), "3D batch pose readback count failed") && ok;
    ok = check(poses2D[1].valid, "2D batch pose readback failed") && ok;
    ok = check(poses3D[1].valid, "3D batch pose readback failed") && ok;
    ok = check(pose2D.valid, "2D pose readback failed") && ok;
    ok = check(pose3D.valid, "3D pose readback failed") && ok;
    ok = check(pose2D.position.y < 1.0f, "2D dynamic body did not move") && ok;
    ok = check(pose3D.position.y < 1.0f, "3D dynamic body did not move") && ok;
    physics.stepFixed(0.0f);
    const auto zeroStepPose2D = world2D.bodyPose(ball2D);
    const auto zeroStepPose3D = world3D.bodyPose(ball3D);
    ok = check(zeroStepPose2D.position.y == pose2D.position.y, "2D zero dt step moved a body") && ok;
    ok = check(zeroStepPose3D.position.y == pose3D.position.y, "3D zero dt step moved a body") && ok;

    const auto hit2D = world2D.raycast(Vector2{0.0f, 2.0f}, Vector2{0.0f, -1.0f}, 8.0f);
    const auto hit3D = world3D.raycast(Vector3{0.0f, 2.0f, 0.0f}, Vector3{0.0f, -1.0f, 0.0f}, 8.0f);
    ok = check(hit2D.has_value(), "2D raycast missed") && ok;
    ok = check(hit3D.has_value(), "3D raycast missed") && ok;
    ok = check(!world2D.raycast(Vector2{0.0f, 2.0f}, Vector2{0.0f, -1.0f}, 0.0f).has_value(), "2D zero-distance raycast hit") && ok;
    ok = check(!world3D.raycast(Vector3{0.0f, 2.0f, 0.0f}, Vector3{0.0f, -1.0f, 0.0f}, 0.0f).has_value(), "3D zero-distance raycast hit") && ok;
    ok = check(sawContact, "No physics contacts were captured") && ok;

    HandPhysicsInteraction3D interaction;
    interaction.init(world3D);
    TrackedRobotHandPose grabPose{
        .valid = true,
        .side = HandSide::Left,
        .confidence = 1.0f,
        .landmarks = {},
    };
    const Vector3 cubeCenter = interaction.state().cubeCenter;
    for (Vector3& landmark : grabPose.landmarks) {
        landmark = cubeCenter;
    }
    grabPose.landmarks[0] = Vector3{cubeCenter.x, cubeCenter.y - 0.030f, cubeCenter.z};
    grabPose.landmarks[4] = Vector3{cubeCenter.x - 0.038f, cubeCenter.y + 0.015f, cubeCenter.z};
    grabPose.landmarks[8] = Vector3{cubeCenter.x + 0.038f, cubeCenter.y + 0.015f, cubeCenter.z};
    grabPose.landmarks[5] = Vector3{cubeCenter.x - 0.050f, cubeCenter.y, cubeCenter.z};
    grabPose.landmarks[9] = Vector3{cubeCenter.x, cubeCenter.y, cubeCenter.z};
    grabPose.landmarks[13] = Vector3{cubeCenter.x + 0.035f, cubeCenter.y, cubeCenter.z};
    grabPose.landmarks[17] = Vector3{cubeCenter.x + 0.070f, cubeCenter.y, cubeCenter.z};
    interaction.update(world3D, &grabPose, nullptr, 1.0f / 60.0f);
    ok = check(interaction.state().grabbed, "hand physics interaction did not grab the cube") && ok;

    for (Vector3& landmark : grabPose.landmarks) {
        landmark = Vector3{5.0f, 5.0f, 5.0f};
    }
    grabPose.landmarks[4] = Vector3{5.0f, 5.0f, 5.0f};
    grabPose.landmarks[8] = Vector3{5.035f, 5.0f, 5.0f};
    interaction.update(world3D, &grabPose, nullptr, 1.0f / 60.0f);
    ok = check(interaction.state().cubeCenter.x < 0.8f, "grabbed cube escaped interaction volume") && ok;
    ok = check(interaction.state().cubeCenter.y < 0.6f, "grabbed cube escaped vertical interaction volume") && ok;
    interaction.shutdown(world3D);

    physics.shutdown();
    ::biofuel::engine::events::EventManager::instance().shutdown();
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
