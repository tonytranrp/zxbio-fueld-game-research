#pragma once

#include "Core/Types.hpp"
#include <functional>

namespace biofuel::systems::idle {

// ------------------------------------------------------------------------------
// IdleTrigger — Reusable idle detection for any screen.
//
// Tracks time since the last input event. When the timer exceeds the timeout
// and `active` is true, fires the `onIdle` callback exactly once.
// Call `onInput()` whenever any user input is detected to reset the timer.
//
// Usage:
//   IdleTrigger trigger{5.0f, []{ /* push IdleScreen */ }};
//   // In update:   trigger.update(dt, true);
//   // In input:    if (anyInput) trigger.onInput();
//   // In onExit:   trigger.reset();
// ------------------------------------------------------------------------------
class IdleTrigger final {
public:
    using Callback = std::function<void()>;

    explicit IdleTrigger(f32 timeoutSeconds = 5.0f, Callback onIdle = {}) noexcept
        : m_timeout(timeoutSeconds)
        , m_onIdle(std::move(onIdle))
    {}

    void setCallback(Callback cb) noexcept { m_onIdle = std::move(cb); }
    void setTimeout(f32 seconds) noexcept { m_timeout = seconds; }

    void reset() noexcept {
        m_timer = 0.0f;
        m_fired = false;
    }

    void update(f32 dt, bool active) noexcept {
        if (!active || m_fired) return;
        m_timer += dt;
        if (m_timer >= m_timeout && m_onIdle) {
            m_fired = true;
            m_onIdle();
        }
    }

    void onInput() noexcept {
        m_timer = 0.0f;
        m_fired = false;
    }

    [[nodiscard]] bool hasFired() const noexcept { return m_fired; }

private:
    f32 m_timer   = 0.0f;
    f32 m_timeout = 5.0f;
    bool m_fired  = false;
    Callback m_onIdle;
};

} // namespace biofuel::systems::idle
