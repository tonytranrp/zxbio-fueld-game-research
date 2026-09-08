#pragma once

// Prompt 004 goal 272: trigger a RenderDoc capture from inside the application.
//
// WHY NO VENDORED HEADER. Research §6.7: "The recommended way to access the RenderDoc API is to
// passively check if the module is loaded, and use the API if it is... When your program is launched
// independently it will see that the RenderDoc module is not present and safely fall back."
// `GetModuleHandleA("renderdoc.dll")` never LOADS the module -- it only asks whether the injector
// already did -- so this costs one failed handle lookup per process when RenderDoc is absent, which
// is the overwhelmingly common case.
//
// WHAT IS TESTED HERE AND WHAT IS NOT, stated plainly because it matters for how much this can be
// trusted:
//
//   * THE ABSENT PATH IS TESTED. RenderDoc is not installed on this machine (no install directory,
//     no registry entry, no renderdoc.dll anywhere on the volume -- checked, not assumed), so every
//     run of every scenario exercises `available() == false`, and the frame output and frame time
//     are unchanged.
//   * THE ACTIVE PATH IS NOT TESTED, and cannot be here. The struct layout below is written from
//     the documented v1.6.0 API and could not be verified against the real header on this machine.
//     So it VALIDATES ITSELF at runtime rather than trusting that: it requests exactly version
//     1.6.0 (RenderDoc's compatibility promise is that a requested version's layout never changes),
//     checks `GetAPIVersion` reports 1.6 or newer, and after starting a capture asks
//     `IsFrameCapturing()` and DISABLES ITSELF if the answer is no. A wrong offset therefore turns
//     the feature off and says so, instead of calling an arbitrary function pointer.

#include <cstdint>
#include <string>

namespace render::diligent {

class RenderDocTrigger {
public:
    /// Looks for an already-injected RenderDoc. Never loads it.
    RenderDocTrigger();

    /// True when RenderDoc is present AND its API validated. False on this machine.
    [[nodiscard]] bool available() const noexcept { return api_ != nullptr; }
    /// Why it is unavailable, for the log line -- "not injected" reads very differently from
    /// "injected but the API did not validate".
    [[nodiscard]] const std::string& status() const noexcept { return status_; }

    /// Bracket one frame. Both are no-ops when unavailable, so callers need no branch.
    void begin_capture();
    /// Ends the capture and returns whether one was actually taken.
    bool end_capture();
    [[nodiscard]] bool capturing() const noexcept;
    /// Captures taken this session, which is what a harness asserts on.
    [[nodiscard]] std::uint32_t captures() const noexcept { return captures_; }

private:
    void* api_ = nullptr; // RENDERDOC_API_1_6_0*, opaque here so the layout stays in the .cpp
    std::string status_;
    std::uint32_t captures_ = 0;
    bool active_ = false;
};

/// The process's one trigger. RenderDoc injection is a property of the PROCESS, not of a renderer
/// or a frame loop, so a single accessor is the honest shape -- and it lets the shared dump helpers
/// bracket their own captures without every caller threading a reference through.
[[nodiscard]] RenderDocTrigger& renderdoc_trigger();

} // namespace render::diligent
