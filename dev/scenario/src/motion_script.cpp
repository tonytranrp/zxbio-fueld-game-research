#include "dev/scenario/motion_script.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace dev::scenario {

namespace {

struct KeyName {
    std::string_view name;
    Key key;
};

constexpr std::array kKeyNames{
    KeyName{"forward", Key::Forward}, KeyName{"back", Key::Back}, KeyName{"left", Key::Left},
    KeyName{"right", Key::Right},     KeyName{"up", Key::Up},     KeyName{"down", Key::Down},
    KeyName{"boost", Key::Boost},     KeyName{"jump", Key::Jump},
};

// Shortest signed angular difference in degrees. A `look` from 350 to 10 turns 20 degrees the near
// way, not 340 the far way -- which is what a human writing the scenario means.
[[nodiscard]] float shortest_delta_deg(float from, float to) noexcept {
    float d = std::fmod(to - from + 180.0f, 360.0f);
    if (d < 0.0f) {
        d += 360.0f;
    }
    return d - 180.0f;
}

} // namespace

bool parse_keys(std::string_view text, Key& out) {
    if (text == "none" || text.empty()) {
        out = Key::None;
        return true;
    }
    Key result = Key::None;
    std::size_t begin = 0;
    while (begin <= text.size()) {
        const std::size_t plus = text.find('+', begin);
        const std::string_view part =
            plus == std::string_view::npos ? text.substr(begin) : text.substr(begin, plus - begin);
        bool found = false;
        for (const KeyName& candidate : kKeyNames) {
            if (candidate.name == part) {
                result = result | candidate.key;
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
        if (plus == std::string_view::npos) {
            break;
        }
        begin = plus + 1;
    }
    out = result;
    return true;
}

std::string keys_to_string(Key keys) {
    if (keys == Key::None) {
        return "none";
    }
    std::string text;
    for (const KeyName& candidate : kKeyNames) {
        if (has(keys, candidate.key)) {
            if (!text.empty()) {
                text += '+';
            }
            text += std::string{candidate.name};
        }
    }
    return text;
}

MotionDriver::MotionDriver(std::span<const Segment> segments, float stepSeconds)
    : segments_(segments.begin(), segments.end()), step_(stepSeconds > 0.0f ? stepSeconds : 1.0f / 60.0f) {}

void MotionDriver::begin_segment(const Pose& current) {
    inSegment_ = 0.0f;
    startPose_ = current;
    jumpFired_ = false;
    entered_ = true;
}

InputFrame MotionDriver::advance(const Pose& current) {
    InputFrame frame;
    if (finished()) {
        return frame;
    }
    if (!entered_) {
        begin_segment(current);
    }
    const Segment& segment = segments_[index_];

    switch (segment.kind) {
    case SegmentKind::Wait:
        break;
    case SegmentKind::Hold: {
        world::player::PlayerIntent& i = frame.intent;
        i.forward = has(segment.keys, Key::Forward);
        i.back = has(segment.keys, Key::Back);
        i.left = has(segment.keys, Key::Left);
        i.right = has(segment.keys, Key::Right);
        i.up = has(segment.keys, Key::Up);
        i.down = has(segment.keys, Key::Down);
        i.boost = has(segment.keys, Key::Boost);
        // The edge: exactly one tick, the first of this segment.
        if (has(segment.keys, Key::Jump) && !jumpFired_) {
            i.jump_pressed = true;
            jumpFired_ = true;
        }
        break;
    }
    case SegmentKind::Look: {
        // Turn a constant fraction of the remaining angle per tick, so the turn lands exactly on
        // the target at the segment's end regardless of how the step divides the duration.
        const float ticks = std::max(1.0f, segment.seconds / step_);
        const float yawDelta = shortest_delta_deg(startPose_.yaw_deg, segment.yaw_deg) / ticks;
        const float pitchDelta = (segment.pitch_deg - startPose_.pitch_deg) / ticks;
        // The app's mouse-look maps pixels to radians; a script says degrees, so convert here --
        // once, in the place that knows the script's units.
        frame.look_delta_pixels.x = -glm::radians(yawDelta) / kScriptLookSensitivityRadPerPixel;
        frame.look_delta_pixels.y = -glm::radians(pitchDelta) / kScriptLookSensitivityRadPerPixel;
        break;
    }
    case SegmentKind::Goto: {
        const glm::vec3 delta = segment.target - current.position;
        const float distance = glm::length(delta);
        if (distance <= segment.radius) {
            // Arrived: end the segment now, and give this tick no input. The closed loop is the
            // point -- a `goto` that ran a fixed number of ticks would be a `hold` with extra
            // arithmetic, and would sail past its target the moment a slope slowed the body.
            ++index_;
            entered_ = false;
            return frame;
        }
        // Steer with the movement intents the player has, expressed in the body's own frame: the
        // scenario cannot ask for a motion the controller does not offer.
        const float yaw = glm::radians(current.yaw_deg);
        const glm::vec3 forward{-std::sin(yaw), 0.0f, -std::cos(yaw)};
        const glm::vec3 right{std::cos(yaw), 0.0f, -std::sin(yaw)};
        const glm::vec3 flat = glm::vec3{delta.x, 0.0f, delta.z};
        const float flatLength = glm::length(flat);
        if (flatLength > 1e-4f) {
            const glm::vec3 dir = flat / flatLength;
            const float alongForward = glm::dot(dir, forward);
            const float alongRight = glm::dot(dir, right);
            frame.intent.forward = alongForward > 0.35f;
            frame.intent.back = alongForward < -0.35f;
            frame.intent.right = alongRight > 0.35f;
            frame.intent.left = alongRight < -0.35f;
        }
        // Vertical only matters in fly/swim; the controller ignores it while walking on land.
        frame.intent.up = delta.y > segment.radius;
        frame.intent.down = delta.y < -segment.radius;
        break;
    }
    }

    inSegment_ += step_;
    elapsed_ += step_;
    if (inSegment_ >= segment.seconds - 1e-6f) {
        ++index_;
        entered_ = false;
    }
    return frame;
}

std::size_t MotionDriver::worst_case_ticks() const noexcept {
    double ticks = 0.0;
    for (const Segment& segment : segments_) {
        ticks += static_cast<double>(segment.seconds) / static_cast<double>(step_);
    }
    return static_cast<std::size_t>(ticks + 1.0);
}

} // namespace dev::scenario
