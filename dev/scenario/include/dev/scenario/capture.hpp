#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace dev::scenario {

// When to grab a frame.
enum class CaptureWhen : std::uint8_t {
    Frame,   // at frame N
    Seconds, // at T seconds of scenario time
    Event,   // on a named event, the first time it happens
    End,     // the last frame of the run
};

// The events worth waiting for, all of which the previous passes waited for by hand.
enum class CaptureEvent : std::uint8_t {
    FirstTreeSwapped, // the first octree finished uploading and was swapped in
    FirstGrounded,    // the body reached Stance::Grounded
    FirstSlowFrame,   // the first frame over the slow-frame threshold
    WorldReady,       // the first frame with something to look at
};

struct CapturePoint {
    CaptureWhen when = CaptureWhen::End;
    std::uint32_t frame = 0;
    float seconds = 0.0f;
    CaptureEvent event = CaptureEvent::WorldReady;
    std::string name; // becomes <name>.png beside the report, and the golden's file name

    [[nodiscard]] friend bool operator==(const CapturePoint&, const CapturePoint&) = default;
};

[[nodiscard]] bool parse_capture_when(std::string_view text, CapturePoint& out) noexcept;
[[nodiscard]] std::string capture_when_to_string(const CapturePoint& point);

} // namespace dev::scenario
