#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "dev/scenario/input_frame.hpp"
#include "dev/scenario/pose.hpp"

namespace dev::scenario {

enum class SegmentKind : std::uint8_t {
    Hold, // press these keys for N seconds
    Look, // turn to a target yaw/pitch over N seconds
    Goto, // drive toward a world point until within R (or the timeout)
    Wait, // N seconds of no input at all
};

// Which intents a `hold` presses. A bitmask rather than eight bools so the .scn spelling
// ("hold forward+boost 3") and the model are the same list.
enum class Key : std::uint16_t {
    None = 0,
    Forward = 1u << 0,
    Back = 1u << 1,
    Left = 1u << 2,
    Right = 1u << 3,
    Up = 1u << 4,
    Down = 1u << 5,
    Boost = 1u << 6,
    Jump = 1u << 7,
};

[[nodiscard]] constexpr Key operator|(Key a, Key b) noexcept {
    return static_cast<Key>(static_cast<std::uint16_t>(a) | static_cast<std::uint16_t>(b));
}
[[nodiscard]] constexpr bool has(Key set, Key one) noexcept {
    return (static_cast<std::uint16_t>(set) & static_cast<std::uint16_t>(one)) != 0;
}

// "forward+boost" <-> Key. Both directions live here so the parser and emit() cannot disagree.
[[nodiscard]] bool parse_keys(std::string_view text, Key& out);
[[nodiscard]] std::string keys_to_string(Key keys);

struct Segment {
    SegmentKind kind = SegmentKind::Wait;
    float seconds = 0.0f; // Hold / Look / Wait; for Goto this is the timeout
    Key keys = Key::None; // Hold
    float yaw_deg = 0.0f; // Look
    float pitch_deg = 0.0f;
    glm::vec3 target{0.0f}; // Goto
    float radius = 1.0f;    // Goto

    [[nodiscard]] friend bool operator==(const Segment&, const Segment&) = default;
};

// Turns a segment list into one InputFrame per fixed tick.
//
// Stateful on purpose: `goto` is closed-loop (it needs the body's actual position, because a hill
// or a tree can stop it) and `look` needs the pose it started from. A pure "sequence of frames"
// function could express neither, and expressing them outside the driver would put simulation
// feedback in the harness's frame loop, which is where a second copy of the game's rules starts.
class MotionDriver {
public:
    MotionDriver(std::span<const Segment> segments, float stepSeconds);

    // One fixed tick. `current` is the pose the simulation is at BEFORE this tick.
    [[nodiscard]] InputFrame advance(const Pose& current);

    [[nodiscard]] bool finished() const noexcept { return index_ >= segments_.size(); }
    [[nodiscard]] std::size_t segment_index() const noexcept { return index_; }
    [[nodiscard]] float elapsed_seconds() const noexcept { return elapsed_; }
    // How many ticks the script will take, ignoring the closed-loop early exit of `goto`. Used to
    // size a run's frame budget, not to decide when it ends.
    [[nodiscard]] std::size_t worst_case_ticks() const noexcept;

private:
    void begin_segment(const Pose& current);

    std::vector<Segment> segments_;
    float step_ = 1.0f / 60.0f;
    std::size_t index_ = 0;
    float inSegment_ = 0.0f; // seconds spent in the current segment
    float elapsed_ = 0.0f;   // seconds since the script started
    bool entered_ = false;   // the current segment has had begin_segment() run
    Pose startPose_{};       // pose when the current segment began (Look interpolates from it)
    // The jump EDGE: set on the first tick of a Hold naming Jump, cleared for every later tick of
    // that segment. world/player/README.md's contract -- holding Space must not re-arm the buffer.
    bool jumpFired_ = false;
};

// Sensible-default pixel sensitivity for a scripted `look`. The app's own look sensitivity is a
// user setting (SpectatorCameraState::look_sensitivity, 0.0025 rad/px); a script says degrees and
// the harness converts, so a scenario's turn does not change meaning if that setting is retuned.
inline constexpr float kScriptLookSensitivityRadPerPixel = 0.0025f;

} // namespace dev::scenario
