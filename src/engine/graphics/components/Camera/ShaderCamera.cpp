#include "ShaderCamera.hpp"
#include <algorithm>

namespace biofuel::engine::graphics::component {

void ShaderCameraController::reset() noexcept {
    m_current  = {};
    m_from     = {};
    m_to       = {};
    m_elapsed  = 0.0f;
    m_duration = 0.0f;
    m_animating = false;
    m_easing = ::biofuel::engine::animation::Easing::easeInOutCubic;
}

void ShaderCameraController::setTarget(
    const ShaderCameraState& target,
    const f32 duration,
    ::biofuel::engine::animation::Easing::Fn easing) noexcept
{
    m_from     = m_current;
    m_to       = target;
    m_elapsed  = 0.0f;
    m_duration = std::max(0.001f, duration);  // prevent division by zero
    m_animating = true;
    m_easing   = easing ? easing : ::biofuel::engine::animation::Easing::easeInOutCubic;
}

void ShaderCameraController::update(const f32 dt) noexcept {
    if (!m_animating) {
        return;
    }

    m_elapsed += dt;

    if (m_elapsed >= m_duration) {
        m_elapsed = m_duration;
        m_current = m_to;
        m_animating = false;
        return;
    }

    const f32 rawT = m_elapsed / m_duration;
    const f32 easedT = m_easing(rawT);
    m_current = ShaderCameraState::lerp(m_from, m_to, easedT);
}

const ShaderCameraState& ShaderCameraController::current() const noexcept {
    return m_current;
}

bool ShaderCameraController::isAnimating() const noexcept {
    return m_animating;
}

bool ShaderCameraController::isComplete() const noexcept {
    return !m_animating && m_elapsed >= m_duration && m_duration > 0.0f;
}

} // namespace biofuel::engine::graphics::component
