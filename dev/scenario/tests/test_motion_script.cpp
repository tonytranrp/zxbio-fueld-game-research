// Goal 208's Check: a two-segment script produces an exact InputFrame sequence for a given
// timestep, INCLUDING that jump_pressed is an edge on exactly one tick.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "dev/scenario/motion_script.hpp"

using Catch::Approx;
using namespace dev::scenario;

namespace {

constexpr float kStep = 1.0f / 60.0f;

// Runs a script against a pose that never moves, which is what makes the sequence exact: `hold`,
// `look` and `wait` are open-loop, so the only closed-loop segment (`goto`) is tested separately.
std::vector<InputFrame> run_open_loop(const std::vector<Segment>& segments, int ticks) {
    MotionDriver driver(segments, kStep);
    std::vector<InputFrame> frames;
    const Pose still{};
    for (int i = 0; i < ticks && !driver.finished(); ++i) {
        frames.push_back(driver.advance(still));
    }
    return frames;
}

} // namespace

TEST_CASE("a two-segment script produces the exact tick sequence", "[scenario]") {
    // 0.05 s of forward, then 0.05 s of nothing: 3 ticks each at 60 Hz.
    Segment forward;
    forward.kind = SegmentKind::Hold;
    forward.keys = Key::Forward;
    forward.seconds = 0.05f;
    Segment idle;
    idle.kind = SegmentKind::Wait;
    idle.seconds = 0.05f;

    const std::vector<InputFrame> frames = run_open_loop({forward, idle}, 20);
    REQUIRE(frames.size() == 6);
    for (int i = 0; i < 3; ++i) {
        CAPTURE(i);
        CHECK(frames[static_cast<std::size_t>(i)].intent.forward);
        CHECK_FALSE(frames[static_cast<std::size_t>(i)].intent.back);
    }
    for (int i = 3; i < 6; ++i) {
        CAPTURE(i);
        CHECK_FALSE(frames[static_cast<std::size_t>(i)].intent.forward);
    }
}

TEST_CASE("jump_pressed is an edge on exactly one tick", "[scenario]") {
    // world/player/README.md's contract: holding Space must not re-arm the jump buffer every tick.
    // A `hold ... jump 1.0` therefore presses once, not sixty times.
    Segment jumping;
    jumping.kind = SegmentKind::Hold;
    jumping.keys = Key::Forward | Key::Jump;
    jumping.seconds = 1.0f;

    const std::vector<InputFrame> frames = run_open_loop({jumping}, 120);
    REQUIRE(frames.size() >= 55);
    int edges = 0;
    for (std::size_t i = 0; i < frames.size(); ++i) {
        if (frames[i].intent.jump_pressed) {
            ++edges;
            CHECK(i == 0); // and it is the FIRST tick of the segment
        }
        CHECK(frames[i].intent.forward); // the level-triggered key stays held throughout
    }
    CHECK(edges == 1);
}

TEST_CASE("two jump segments each fire their own edge", "[scenario]") {
    Segment jump;
    jump.kind = SegmentKind::Hold;
    jump.keys = Key::Jump;
    jump.seconds = 0.05f;
    Segment gap;
    gap.kind = SegmentKind::Wait;
    gap.seconds = 0.05f;

    const std::vector<InputFrame> frames = run_open_loop({jump, gap, jump}, 40);
    int edges = 0;
    for (const InputFrame& frame : frames) {
        edges += frame.intent.jump_pressed ? 1 : 0;
    }
    CHECK(edges == 2);
}

TEST_CASE("a look segment lands exactly on its target", "[scenario]") {
    // The driver emits pixel deltas; summing them back through the app's own sensitivity must give
    // the requested turn, or a scenario's `look 90 -20 1` would mean something different at a
    // different timestep.
    Segment look;
    look.kind = SegmentKind::Look;
    look.yaw_deg = 90.0f;
    look.pitch_deg = -20.0f;
    look.seconds = 0.5f;

    const std::vector<InputFrame> frames = run_open_loop({look}, 100);
    REQUIRE(!frames.empty());
    float yaw = 0.0f;
    float pitch = 0.0f;
    for (const InputFrame& frame : frames) {
        // The app's mapping: yaw -= dx * sensitivity, pitch -= dy * sensitivity (see
        // app/src/spectator_camera.cpp's apply_look).
        yaw -= frame.look_delta_pixels.x * kScriptLookSensitivityRadPerPixel;
        pitch -= frame.look_delta_pixels.y * kScriptLookSensitivityRadPerPixel;
    }
    CHECK(glm::degrees(yaw) == Approx(90.0f).margin(0.01));
    CHECK(glm::degrees(pitch) == Approx(-20.0f).margin(0.01));
}

TEST_CASE("a look takes the near way around", "[scenario]") {
    Segment look;
    look.kind = SegmentKind::Look;
    look.yaw_deg = 10.0f;
    look.pitch_deg = 0.0f;
    look.seconds = 0.2f;

    MotionDriver driver(std::vector<Segment>{look}, kStep);
    Pose pose;
    pose.yaw_deg = 350.0f;
    float yaw = 0.0f;
    while (!driver.finished()) {
        yaw -= driver.advance(pose).look_delta_pixels.x * kScriptLookSensitivityRadPerPixel;
    }
    // 20 degrees the near way, not -340 the far way.
    CHECK(glm::degrees(yaw) == Approx(20.0f).margin(0.01));
}

TEST_CASE("goto is closed-loop: it ends on arrival, not on a tick count", "[scenario]") {
    Segment go;
    go.kind = SegmentKind::Goto;
    go.target = glm::vec3{100.0f, 0.0f, 0.0f};
    go.radius = 2.0f;
    go.seconds = 10.0f; // timeout it must not need

    MotionDriver driver(std::vector<Segment>{go}, kStep);
    Pose pose;
    pose.yaw_deg = 0.0f; // facing -Z; the target is +X, so it should press `right`
    int ticks = 0;
    while (!driver.finished() && ticks < 600) {
        const InputFrame frame = driver.advance(pose);
        if (ticks == 0) {
            CHECK(frame.intent.right);
            CHECK_FALSE(frame.intent.left);
        }
        // Teleport the "body" 1 m per tick toward the target, standing in for the simulation.
        pose.position.x += 1.0f;
        ++ticks;
    }
    CHECK(driver.finished());
    CHECK(ticks < 100); // arrived at ~98 m, long before the 600-tick timeout
}

TEST_CASE("goto gives up at its timeout instead of running forever", "[scenario]") {
    Segment go;
    go.kind = SegmentKind::Goto;
    go.target = glm::vec3{1000.0f, 0.0f, 0.0f};
    go.radius = 1.0f;
    go.seconds = 0.1f; // 6 ticks

    MotionDriver driver(std::vector<Segment>{go}, kStep);
    const Pose stuck{}; // a body that never moves: a wall, a tree, a hill
    int ticks = 0;
    while (!driver.finished() && ticks < 1000) {
        (void)driver.advance(stuck);
        ++ticks;
    }
    CHECK(driver.finished());
    CHECK(ticks <= 7);
}

TEST_CASE("key sets round-trip through their text spelling", "[scenario]") {
    Key keys{};
    REQUIRE(parse_keys("forward+boost+jump", keys));
    CHECK(has(keys, Key::Forward));
    CHECK(has(keys, Key::Boost));
    CHECK(has(keys, Key::Jump));
    CHECK_FALSE(has(keys, Key::Back));
    CHECK(keys_to_string(keys) == "forward+boost+jump");

    Key none{};
    REQUIRE(parse_keys("none", none));
    CHECK(none == Key::None);
    CHECK(keys_to_string(none) == "none");

    Key bad{};
    CHECK_FALSE(parse_keys("forward+sideways", bad));
}
