#pragma once

#include "engine/core/Types.hpp"

namespace biofuel::game::presentation::hands {

class HandPresenceAnimator final {
public:
    void reset() noexcept { m_presence = 0.0f; }
    void update(bool detected, f32 confidence, bool manualOverride, f32 dt) noexcept;

    [[nodiscard]] f32 alpha() const noexcept { return m_presence; }
    [[nodiscard]] f32 scale() const noexcept { return 0.92f + 0.08f * m_presence; }
    [[nodiscard]] bool visible() const noexcept { return m_presence > 0.01f; }

private:
    f32 m_presence = 0.0f;
};

} // namespace biofuel::game::presentation::hands
