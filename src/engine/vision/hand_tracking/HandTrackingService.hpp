#pragma once

#include "engine/core/Types.hpp"
#include "engine/vision/hand_tracking/HandTrackingTypes.hpp"
#include <memory>
#include <optional>

namespace biofuel::engine::vision::hand_tracking {

class HandTrackingService final {
public:
    HandTrackingService();
    ~HandTrackingService() noexcept;

    HandTrackingService(const HandTrackingService&) = delete;
    HandTrackingService& operator=(const HandTrackingService&) = delete;
    HandTrackingService(HandTrackingService&&) = delete;
    HandTrackingService& operator=(HandTrackingService&&) = delete;

    void init();
    void shutdown() noexcept;
    void update(f32 dt);

    void requestCameraAccess();
    void approveCameraAccess();
    void denyCameraAccess();

    bool start();
    void stop() noexcept;
    void setPreviewEnabled(bool enabled);

    [[nodiscard]] bool featureEnabled() const noexcept;
    [[nodiscard]] bool cameraConsentGranted() const noexcept;
    [[nodiscard]] bool cameraConsentRequested() const noexcept;
    [[nodiscard]] bool running() const noexcept;
    [[nodiscard]] bool previewEnabled() const noexcept;
    [[nodiscard]] HandTrackingStatus status() const;
    [[nodiscard]] std::optional<HandTrackingFrame> latestFrame() const;
    [[nodiscard]] std::optional<HandTrackingPreviewFrame> latestPreviewFrame() const;
    [[nodiscard]] std::optional<HandTrackingPreviewFrame> latestPreviewFrameAfter(u64 sequence) const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace biofuel::engine::vision::hand_tracking
