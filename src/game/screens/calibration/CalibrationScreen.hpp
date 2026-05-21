#pragma once

#include "game/screens/GameScreenIds.hpp"
#include "engine/ui/Screen.hpp"
#include "game/presentation/hands/HandPreviewTexture.hpp"
#include <raylib.h>
#include <string_view>

namespace biofuel::game::screens {

class CalibrationScreen final : public ::biofuel::engine::ui::Screen {
public:
    void onEnter() override;
    void onExit() override;
    void onUpdate(f32 dt) override;
    void onRender() override;
    void onInput() override;

    [[nodiscard]] ::biofuel::engine::ui::typed::ScreenId screenId() const noexcept override {
        return ::biofuel::game::screens::screen_id::Calibration;
    }
    [[nodiscard]] std::string_view getName() const noexcept override { return "CalibrationScreen"; }

private:
    enum class Phase {
        FeatureDisabled,
        WaitingForConsent,
        Starting,
        Calibrating,
        SuccessHold,
        Outro,
        Cancelled,
        Failed,
    };

    void startTracking();
    void completeAndPop();
    void cancelAndPop();
    void failAndPop();
    [[nodiscard]] Rectangle cameraContentRect(Rectangle bounds) const noexcept;
    void drawCameraPreview(Rectangle bounds) const;
    void drawCalibrationGuide(Rectangle bounds) const;
    void drawStatusCard(Rectangle bounds) const;
    [[nodiscard]] f32 overlayProgress() const noexcept;

    game::presentation::hands::HandPreviewTexture m_preview{};
    Phase m_phase = Phase::WaitingForConsent;
    f32 m_phaseTime = 0.0f;
    f32 m_overlayTime = 0.0f;
    bool m_startedCalibration = false;
    bool m_popQueued = false;
};

} // namespace biofuel::game::screens
