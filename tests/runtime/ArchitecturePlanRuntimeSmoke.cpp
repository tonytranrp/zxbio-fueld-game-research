#include "engine/vision/hand_tracking/HandTrackingService.hpp"

#include <cstdlib>
#include <iostream>
#include <optional>

namespace {

bool check(const bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        return false;
    }
    return true;
}

bool hand_tracking_disabled_public_api_is_nonblocking_contract() {
    using ::biofuel::engine::vision::hand_tracking::HandTrackingConnectionState;
    using ::biofuel::engine::vision::hand_tracking::HandTrackingService;

    HandTrackingService service;
    service.init();

    if (service.featureEnabled()) {
        service.shutdown();
        std::cout << "Hand tracking runtime API deadlock test skipped: BIOFUEL_ENABLE_HAND_TRACKING is enabled and start() may require external Python/camera.\n";
        return true;
    }

    bool ok = true;
    service.requestCameraAccess();
    ok = check(service.cameraConsentRequested(), "HandTrackingService did not record camera access request") && ok;
    service.approveCameraAccess();
    ok = check(service.cameraConsentGranted(), "HandTrackingService did not record camera access approval") && ok;
    service.setPreviewEnabled(true);
    ok = check(service.previewEnabled(), "HandTrackingService did not record preview enable request") && ok;
    service.denyCameraAccess();
    ok = check(!service.cameraConsentGranted(), "HandTrackingService did not record camera access denial") && ok;
    ok = check(!service.running(), "HandTrackingService reported running while feature disabled") && ok;
    ok = check(service.latestFrame() == std::nullopt, "HandTrackingService exposed a tracking frame while feature disabled") && ok;
    ok = check(service.latestPreviewFrame() == std::nullopt, "HandTrackingService exposed a preview frame while feature disabled") && ok;
    ok = check(service.latestPreviewFrameAfter(0) == std::nullopt, "HandTrackingService exposed a preview frame-after result while feature disabled") && ok;
    ok = check(!service.start(), "HandTrackingService start succeeded while feature disabled") && ok;
    service.update(0.016f);
    const auto status = service.status();
    ok = check(status.state == HandTrackingConnectionState::Disabled, "HandTrackingService disabled build did not report Disabled status") && ok;
    service.stop();
    service.shutdown();
    return ok;
}

} // namespace

int main() {
    return hand_tracking_disabled_public_api_is_nonblocking_contract() ? EXIT_SUCCESS : EXIT_FAILURE;
}
