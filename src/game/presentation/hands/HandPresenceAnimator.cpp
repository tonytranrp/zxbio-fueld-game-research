#include "game/presentation/hands/HandPresenceAnimator.hpp"

#include <algorithm>

namespace biofuel::game::presentation::hands {

void HandPresenceAnimator::update(
    const bool detected,
    const f32 confidence,
    const bool manualOverride,
    const f32 dt) noexcept
{
    const bool targetVisible = manualOverride || (detected && confidence > 0.05f);
    const f32 safeDt = std::clamp(dt, 0.0f, 0.10f);
    const f32 rate = targetVisible ? (1.0f / 0.25f) : (1.0f / 0.18f);
    const f32 direction = targetVisible ? 1.0f : -1.0f;
    m_presence = std::clamp(m_presence + direction * safeDt * rate, 0.0f, 1.0f);
}

} // namespace biofuel::game::presentation::hands
