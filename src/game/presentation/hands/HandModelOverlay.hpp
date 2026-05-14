#pragma once

#include "engine/custom/procedural/hand/RobotHandModule.hpp"
#include "game/presentation/hands/HandPresenceAnimator.hpp"
#include <raylib.h>

namespace biofuel::game::presentation::hands {

void ensureModelOnlyHandTracking() noexcept;

class HandModelOverlay final {
public:
    void onEnter();
    void onExit() noexcept;
    void update(f32 dt, bool manualLeft = false, bool manualRight = false) noexcept;
    void render();

private:
    using HandEngine = ::biofuel::engine::custom::procedural::hand::RobotHandModule<
        ::biofuel::engine::custom::procedural::hand::BiofuelRobotHands>;
    using TrackedPose = ::biofuel::engine::custom::procedural::hand::TrackedRobotHandPose;

    void ensureReady();
    void renderPose(const TrackedPose& pose, const HandPresenceAnimator& animator);
    [[nodiscard]] Camera3D overlayCamera() const noexcept;

    HandEngine m_hands{};
    ::biofuel::engine::custom::procedural::hand::RobotHandPreset m_preset{};
    HandPresenceAnimator m_left{};
    HandPresenceAnimator m_right{};
    bool m_ready = false;
};

} // namespace biofuel::game::presentation::hands
