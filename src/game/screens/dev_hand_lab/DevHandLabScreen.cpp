#include "DevHandLabScreen.hpp"

#ifdef BIOFUEL_ENABLE_DEV_SCREENS

#include "engine/core/LoadingTask.hpp"
#include "engine/custom/procedural/hand/HandTypes.hpp"
#include "engine/custom/procedural/ik/IkTypes.hpp"
#include "engine/graphics/Render.hpp"
#include "engine/runtime/Runtime.hpp"
#include "engine/ui/ScreenManager.hpp"
#include "game/screens/main_menu/MainMenuScreen.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <raymath.h>

namespace biofuel::game::screens {

namespace {

using ::biofuel::engine::graphics::Renderer;
using ::biofuel::engine::custom::procedural::hand::DefaultRobotHandPreset;
using ::biofuel::engine::custom::procedural::hand::HandSide;
using ::biofuel::engine::custom::procedural::hand::RobotHandRenderOptions;
using ::biofuel::engine::custom::procedural::hand::TrackedRobotHandPose;
#ifdef BIOFUEL_ENABLE_HAND_TRACKING
using ::biofuel::engine::custom::procedural::pose::CalibrationHandPhase;
using ::biofuel::engine::custom::procedural::pose::MirrorPolicy;
using ::biofuel::engine::custom::procedural::pose::StageLayoutPolicy;
#endif

[[nodiscard]] Vector3 add(const Vector3 a, const Vector3 b) noexcept {
    return Vector3Add(a, b);
}

#ifdef BIOFUEL_ENABLE_HAND_TRACKING
[[nodiscard]] Rectangle previewPanelBounds() noexcept {
    const f32 width = std::min(500.0f, std::max(280.0f, static_cast<f32>(Renderer::screenWidth()) * 0.38f));
    const f32 height = width * 0.5625f + 62.0f;
    return Rectangle{24.0f, 24.0f, width, height};
}

[[nodiscard]] Rectangle previewImageBounds(const Rectangle panel, const Texture2D texture) noexcept {
    const f32 previewWidth = panel.width;
    const f32 previewHeight = texture.id != 0U
        ? previewWidth * static_cast<f32>(texture.height) / std::max(static_cast<f32>(texture.width), 1.0f)
        : previewWidth * 0.5625f;
    return Rectangle{panel.x, panel.y + 38.0f, previewWidth, previewHeight};
}

void drawPreviewMessage(
    const Rectangle preview,
    const std::string_view primary,
    const std::string_view secondary,
    const Color color) noexcept
{
    const i32 x = static_cast<i32>(preview.x + 16.0f);
    const i32 y = static_cast<i32>(preview.y + preview.height * 0.5f - (secondary.empty() ? 7.0f : 17.0f));
    Renderer::drawText(primary, x, y, 13, color);
    if (!secondary.empty()) {
        Renderer::drawText(secondary, x, y + 18, 12, Color{158, 184, 178, 255});
    }
}
#endif

} // namespace

void DevHandLabScreen::buildLoadingTasks(::biofuel::LoadingTaskQueue& tasks) {
    // Preset load: JSON file read + texture creation (sync, main-thread-safe)
    tasks.add({"Loading robot hand preset...", 1.5f, [this]() {
        reloadPreset();
    }});

    // Physics init: creates ~15 Rapier bodies (sync, main-thread-safe)
    tasks.add({"Initializing hand physics...", 1.2f, [this]() {
        m_handPhysics.init(::biofuel::engine::runtime::Runtime::physics().world3D());
    }});

#ifdef BIOFUEL_ENABLE_HAND_TRACKING
    // Hand tracking start: camera + network (async, runs on worker thread)
    tasks.add(::biofuel::LoadingTask::async("Starting hand tracking...", 0.5f, [this](std::stop_token token) {
        if (token.stop_requested()) {
            return;
        }
        startHandTrackingWithPreview();
    }));
#endif
}

DevHandLabScreen::~DevHandLabScreen() {
    if (m_handPhysics.initialized()) {
        m_handPhysics.shutdown(::biofuel::engine::runtime::Runtime::physics().world3D());
    }
#ifdef BIOFUEL_ENABLE_HAND_TRACKING
    unloadPreviewTexture();
#endif
}

void DevHandLabScreen::onEnter() {
    setTransitionDuration(0.08f);
    m_cameraState = dev_hand_lab::HandLabCameraState{
        .target = Vector3{0.0f, 0.02f, 0.0f},
        .yaw = 0.0f,
        .pitch = 0.10f,
        .distance = 2.35f,
    };
    resetHands();
#ifdef BIOFUEL_ENABLE_HAND_TRACKING
    resetTrackingCalibration();
#endif
    applyCamera();
    m_loadingComplete = true;
}

void DevHandLabScreen::onExit() {
    m_handPhysics.shutdown(::biofuel::engine::runtime::Runtime::physics().world3D());
#ifdef BIOFUEL_ENABLE_HAND_TRACKING
    ::biofuel::engine::runtime::Runtime::handTracking().stop();
    unloadPreviewTexture();
    m_trackedLeft.valid = false;
    m_trackedRight.valid = false;
#endif
}

void DevHandLabScreen::onUpdate(const f32 dt) {
    updateHandTracking(dt);
    m_handEngine.solve(m_leftHand, m_rightHand, m_preset.ik);
    updateHandPhysics(dt);
    applyCamera();
}

void DevHandLabScreen::onInput() {
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (auto* sm = manager()) {
            sm->queueReplace<MainMenuScreen>();
        }
        return;
    }

#ifdef BIOFUEL_ENABLE_HAND_TRACKING
    if (IsKeyPressed(KEY_C)) { startHandTrackingWithPreview(); }
    if (IsKeyPressed(KEY_V)) {
        auto& tracking = ::biofuel::engine::runtime::Runtime::handTracking();
        tracking.setPreviewEnabled(!tracking.previewEnabled());
    }
    if (IsKeyPressed(KEY_X)) {
        ::biofuel::engine::runtime::Runtime::handTracking().stop();
        unloadPreviewTexture();
        m_handRetargeter.resetTracking();
        m_trackedLeft.valid = false;
        m_trackedRight.valid = false;
    }
    if (IsKeyPressed(KEY_K)) {
        resetTrackingCalibration();
    }
#endif

    updateCameraInput();
}

void DevHandLabScreen::onRender() {
    if (!m_loadingComplete) {
        ClearBackground(Color{5, 10, 13, 255});
        const char* loadingText = "Loading...";
        const i32 textW = ::biofuel::engine::graphics::Renderer::measureText(loadingText, 24);
        ::biofuel::engine::graphics::Renderer::drawText(
            loadingText,
            (::biofuel::engine::graphics::Renderer::screenWidth() - textW) / 2,
            ::biofuel::engine::graphics::Renderer::screenHeight() / 2 - 12,
            24,
            Color{216, 240, 228, 255});
        return;
    }

    drawStudio();

    BeginMode3D(m_camera);
    drawFloorGrid();
    drawPhysicsProps();

    const RobotHandRenderOptions renderOptions{
        .showBones = false,
        .showTargets = false,
        .selectedTargetOnly = false,
        .materials = m_preset.materials,
    };
#ifdef BIOFUEL_ENABLE_HAND_TRACKING
    const bool hasTrackedPose = m_trackedLeft.valid || m_trackedRight.valid;
    if (hasTrackedPose) {
        if (m_trackedLeft.valid) {
            m_handEngine.renderTracked(m_trackedLeft, renderOptions);
        }
        if (m_trackedRight.valid) {
            m_handEngine.renderTracked(m_trackedRight, renderOptions);
        }
    } else {
        m_handEngine.render(m_leftHand, renderOptions);
        m_handEngine.render(m_rightHand, renderOptions);
    }
#else
    m_handEngine.render(m_leftHand, renderOptions);
    m_handEngine.render(m_rightHand, renderOptions);
#endif
    EndMode3D();

#ifdef BIOFUEL_ENABLE_HAND_TRACKING
    drawLiveTrackingOverlay();
#endif
    drawStatusHud();
}

void DevHandLabScreen::applyCamera() noexcept {
    const f32 cp = std::cos(m_cameraState.pitch);
    const Vector3 offset{
        std::sin(m_cameraState.yaw) * cp * m_cameraState.distance,
        std::sin(m_cameraState.pitch) * m_cameraState.distance,
        std::cos(m_cameraState.yaw) * cp * m_cameraState.distance,
    };
    m_camera = Camera3D{
        .position = add(m_cameraState.target, offset),
        .target = m_cameraState.target,
        .up = Vector3{0.0f, 1.0f, 0.0f},
        .fovy = 34.0f,
        .projection = CAMERA_PERSPECTIVE,
    };
}

void DevHandLabScreen::updateCameraInput() noexcept {
    const Vector2 mouse = GetMousePosition();
#ifdef BIOFUEL_ENABLE_HAND_TRACKING
    const Rectangle panel = previewPanelBounds();
    if (CheckCollisionPointRec(mouse, panel)) {
        m_lastMouse = mouse;
        return;
    }
#endif

    const Vector2 delta{mouse.x - m_lastMouse.x, mouse.y - m_lastMouse.y};
    const bool pan = IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)
        || (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)));
    if (pan) {
        m_cameraState.target.x -= delta.x * 0.0025f;
        m_cameraState.target.y += delta.y * 0.0025f;
    } else if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        m_cameraState.yaw -= delta.x * 0.008f;
        m_cameraState.pitch = std::clamp(m_cameraState.pitch + delta.y * 0.006f, -0.85f, 0.95f);
    }

    const f32 wheel = GetMouseWheelMove();
    if (wheel != 0.0f) {
        m_cameraState.distance = std::clamp(m_cameraState.distance - wheel * 0.12f, 0.75f, 4.0f);
    }
    m_lastMouse = mouse;
}

void DevHandLabScreen::updateHandTracking(const f32 dt) noexcept {
#ifdef BIOFUEL_ENABLE_HAND_TRACKING
    auto& tracking = ::biofuel::engine::runtime::Runtime::handTracking();
    updatePreviewTexture();
    if (!tracking.running() || tracking.status().secondsSinceLastFrame > 0.35f) {
        m_trackedLeft.valid = false;
        m_trackedRight.valid = false;
        return;
    }
    if (const auto frame = tracking.latestFrame()) {
        m_handRetargeter.beginSession(
            frame->cameraWidth,
            frame->cameraHeight,
            MirrorPolicy::Selfie,
            StageLayoutPolicy::Adaptive);
        if (!m_handRetargeter.calibrationValid() && !m_handRetargeter.calibrationState().active) {
            m_handRetargeter.startCalibration();
        }
        applyTrackedFrame(*frame, dt);
    }
#else
    static_cast<void>(dt);
#endif
}

#ifdef BIOFUEL_ENABLE_HAND_TRACKING
void DevHandLabScreen::applyTrackedFrame(
    const ::biofuel::engine::vision::hand_tracking::HandTrackingFrame& frame,
    const f32 dt) noexcept
{
    m_trackingMapped = m_handRetargeter.map(frame, dt);
    m_trackedLeft = m_trackingMapped.leftPose;
    m_trackedRight = m_trackingMapped.rightPose;
}

void DevHandLabScreen::resetTrackingCalibration() noexcept {
    m_handRetargeter.resetCalibration();
    m_handRetargeter.resetTracking();
    m_handRetargeter.startCalibration();
    m_trackedLeft.valid = false;
    m_trackedRight.valid = false;
    m_trackingMapped = {};
}

void DevHandLabScreen::updatePreviewTexture() noexcept {
    auto& tracking = ::biofuel::engine::runtime::Runtime::handTracking();
    const auto status = tracking.status();
    if (!status.previewEnabled) {
        return;
    }
    const auto preview = tracking.latestPreviewFrameAfter(m_previewTextureSequence);
    if (!preview) {
        return;
    }
    const bool hasRgbaPreview = !(*preview)->rgbaBytes.empty() && (*preview)->width > 0U && (*preview)->height > 0U;
    const bool hasJpegPreview = !(*preview)->jpegBytes.empty();
    if (!hasRgbaPreview && !hasJpegPreview) {
        return;
    }

    if (hasRgbaPreview) {
        const i32 width = static_cast<i32>((*preview)->width);
        const i32 height = static_cast<i32>((*preview)->height);
        const usize requiredBytes = static_cast<usize>(width) * static_cast<usize>(height) * 4U;
        if ((*preview)->rgbaBytes.size() < requiredBytes) {
            return;
        }
        if (m_previewTexture.id != 0U && m_previewTexture.width == width && m_previewTexture.height == height) {
            UpdateTexture(m_previewTexture, (*preview)->rgbaBytes.data());
            m_previewTextureSequence = (*preview)->sequence;
            return;
        }

        Image image{
            .data = const_cast<u8*>((*preview)->rgbaBytes.data()),
            .width = width,
            .height = height,
            .mipmaps = 1,
            .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
        };
        Texture2D texture = LoadTextureFromImage(image);
        if (texture.id == 0U) {
            return;
        }
        unloadPreviewTexture();
        m_previewTexture = texture;
        m_previewTextureSequence = (*preview)->sequence;
        return;
    }

    if ((*preview)->jpegBytes.size() > static_cast<usize>(std::numeric_limits<i32>::max())) {
        return;
    }
    Image image = LoadImageFromMemory(".jpg", (*preview)->jpegBytes.data(), static_cast<i32>((*preview)->jpegBytes.size()));
    if (image.data == nullptr) {
        return;
    }
    ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    if (m_previewTexture.id != 0U && m_previewTexture.width == image.width && m_previewTexture.height == image.height) {
        UpdateTexture(m_previewTexture, image.data);
        UnloadImage(image);
        m_previewTextureSequence = (*preview)->sequence;
        return;
    }
    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    if (texture.id == 0U) {
        return;
    }
    unloadPreviewTexture();
    m_previewTexture = texture;
    m_previewTextureSequence = (*preview)->sequence;
}

void DevHandLabScreen::unloadPreviewTexture() noexcept {
    if (m_previewTexture.id != 0U) {
        UnloadTexture(m_previewTexture);
        m_previewTexture = Texture2D{};
        m_previewTextureSequence = std::numeric_limits<u64>::max();
    }
}

void DevHandLabScreen::startHandTrackingWithPreview() noexcept {
    auto& tracking = ::biofuel::engine::runtime::Runtime::handTracking();
    tracking.approveCameraAccess();
    tracking.setPreviewEnabled(true);
    (void)tracking.start();
}
#endif

void DevHandLabScreen::resetHands() noexcept {
    m_wrist = {};
    m_handEngine.resetHand(m_leftHand, m_wrist.leftOrigin);
    m_handEngine.resetHand(m_rightHand, m_wrist.rightOrigin);
    applyWristPose();
}

void DevHandLabScreen::applyWristPose() noexcept {
    constexpr f32 relaxedCurl = 0.10f;
    constexpr f32 relaxedSpread = 0.0f;
    m_leftHand.setWristPose(m_wrist.leftOrigin, m_wrist.pitch, m_wrist.yaw, m_wrist.roll);
    m_rightHand.setWristPose(m_wrist.rightOrigin, m_wrist.pitch, -m_wrist.yaw, -m_wrist.roll);
    m_leftHand.setCurl(relaxedCurl);
    m_rightHand.setCurl(relaxedCurl);
    m_leftHand.setSpread(relaxedSpread);
    m_rightHand.setSpread(relaxedSpread);
}

void DevHandLabScreen::reloadPreset() noexcept {
    m_preset = m_handEngine.presets().load<DefaultRobotHandPreset>();
    m_handEngine.applyPreset(m_preset);
}

void DevHandLabScreen::updateHandPhysics(const f32 dt) noexcept {
#ifdef BIOFUEL_ENABLE_HAND_TRACKING
    const TrackedHandPose* left = m_trackedLeft.valid ? &m_trackedLeft : nullptr;
    const TrackedHandPose* right = m_trackedRight.valid ? &m_trackedRight : nullptr;
    m_handPhysics.update(::biofuel::engine::runtime::Runtime::physics().world3D(), left, right, dt);
#else
    m_handPhysics.update(::biofuel::engine::runtime::Runtime::physics().world3D(), nullptr, nullptr, dt);
#endif
}

void DevHandLabScreen::drawStudio() const noexcept {
    DrawRectangleGradientV(0, 0, Renderer::screenWidth(), Renderer::screenHeight(), Color{5, 10, 13, 255}, Color{10, 27, 24, 255});
    DrawCircleGradient(
        Renderer::screenWidth() / 2 + 160,
        Renderer::screenHeight() / 2 - 48,
        static_cast<f32>(Renderer::screenHeight()) * 0.58f,
        Color{22, 104, 66, 72},
        Color{5, 10, 13, 0});
    DrawRectangle(0, Renderer::screenHeight() - 108, Renderer::screenWidth(), 108, Color{2, 6, 8, 132});
}

void DevHandLabScreen::drawPhysicsProps() const noexcept {
    const auto& state = m_handPhysics.state();
    if (state.touchShelfValid) {
        const Vector3 shelfSize = Vector3Scale(state.touchShelfHalfExtents, 2.0f);
        DrawCubeV(state.touchShelfCenter, shelfSize, Color{22, 43, 40, 210});
        DrawCubeWiresV(state.touchShelfCenter, shelfSize, Color{86, 188, 139, 135});
    }

    if (!state.cubeValid) {
        return;
    }

    const Vector3 size = Vector3Scale(state.cubeHalfExtents, 2.0f);
    const Color cubeColor = state.grabbed ? Color{255, 198, 86, 255} : Color{118, 212, 164, 255};
    const Color wireColor = state.grabbed ? Color{255, 238, 174, 255} : Color{202, 246, 221, 255};
    DrawCubeV(state.cubeCenter, size, cubeColor);
    DrawCubeWiresV(state.cubeCenter, size, wireColor);

    if (state.grabbed) {
        DrawSphere(state.grabPoint, 0.022f, Color{255, 242, 176, 210});
    }
}

void DevHandLabScreen::drawFloorGrid() const noexcept {
    constexpr f32 floorY = -0.37f;
    constexpr i32 lineCount = 12;
    constexpr f32 spacing = 0.16f;
    for (i32 index = -lineCount; index <= lineCount; ++index) {
        const f32 p = static_cast<f32>(index) * spacing;
        const Color line = index == 0 ? Color{76, 192, 130, 115} : Color{88, 126, 120, 58};
        DrawLine3D(Vector3{p, floorY, -1.9f}, Vector3{p, floorY, 1.9f}, line);
        DrawLine3D(Vector3{-1.9f, floorY, p}, Vector3{1.9f, floorY, p}, line);
    }
}

void DevHandLabScreen::drawStatusHud() const noexcept {
    DrawRectangle(18, Renderer::screenHeight() - 56, 590, 36, Color{3, 8, 10, 176});
    DrawRectangleLines(18, Renderer::screenHeight() - 56, 590, 36, Color{64, 190, 118, 155});

    std::array<char, 320> line{};
#ifdef BIOFUEL_ENABLE_HAND_TRACKING
    const auto& tracking = ::biofuel::engine::runtime::Runtime::handTracking();
    const auto status = tracking.status();
    const auto frame = tracking.latestFrame();
    std::string_view gesture = "No hand";
    if (frame && frame->valid) {
        for (const auto& hand : frame->hands) {
            if (hand.valid) {
                gesture = ::biofuel::engine::vision::hand_tracking::toString(hand.gesture);
                break;
            }
        }
    }
    const bool calibrating = m_handRetargeter.calibrationState().active;
    const auto& physicsState = m_handPhysics.state();
    const std::string_view grabState = physicsState.grabbed
        ? (physicsState.grabbedBy == HandSide::Left ? "grab left" : "grab right")
        : "cube idle";
    std::snprintf(line.data(), line.size(), "ESC | C start | V preview | X stop | K calibrate | %.*s | %.1f pps | %s | %.*s | %.*s",
        static_cast<int>(::biofuel::engine::vision::hand_tracking::toString(status.state).size()),
        ::biofuel::engine::vision::hand_tracking::toString(status.state).data(),
        static_cast<double>(status.packetsPerSecond),
        calibrating ? "Calibrating" : "Mapped",
        static_cast<int>(gesture.size()),
        gesture.data(),
        static_cast<int>(grabState.size()),
        grabState.data());
#else
    std::snprintf(line.data(), line.size(), "ESC menu | hand tracking build option OFF | RMB orbit | wheel zoom");
#endif
    Renderer::drawText(line.data(), 30, Renderer::screenHeight() - 43, 13, Color{216, 240, 228, 255});
}

#ifdef BIOFUEL_ENABLE_HAND_TRACKING
void DevHandLabScreen::drawLiveTrackingOverlay() const noexcept {
    const auto& tracking = ::biofuel::engine::runtime::Runtime::handTracking();
    const auto status = tracking.status();
    const auto wizard = m_handRetargeter.calibrationState();

    const Rectangle panel = previewPanelBounds();
    const Rectangle preview = previewImageBounds(panel, m_previewTexture);

    DrawRectangleRec(Rectangle{panel.x - 2.0f, panel.y - 2.0f, panel.width + 4.0f, panel.height + 4.0f}, Color{0, 0, 0, 116});
    DrawRectangleRec(panel, Color{4, 10, 13, 224});
    DrawRectangleLinesEx(panel, 1.0f, status.workerRunning ? Color{84, 236, 148, 210} : Color{92, 118, 122, 180});

    std::array<char, 192> line{};
    std::snprintf(line.data(), line.size(), "LIVE CAMERA | %.*s | age %.2fs | %s",
        static_cast<int>(::biofuel::engine::vision::hand_tracking::toString(status.state).size()),
        ::biofuel::engine::vision::hand_tracking::toString(status.state).data(),
        static_cast<double>(status.secondsSinceLastFrame),
        wizard.active ? "quick calibration running" : "selfie mirror mapped");
    Renderer::drawText(line.data(), static_cast<i32>(panel.x + 12.0f), static_cast<i32>(panel.y + 12.0f), 13, Color{226, 244, 236, 255});

    DrawRectangleRec(preview, Color{9, 16, 19, 255});
    if (m_previewTexture.id != 0U) {
        const Rectangle src{
            static_cast<f32>(m_previewTexture.width),
            0.0f,
            -static_cast<f32>(m_previewTexture.width),
            static_cast<f32>(m_previewTexture.height)};
        DrawTexturePro(m_previewTexture, src, preview, Vector2{0.0f, 0.0f}, 0.0f, WHITE);
        if (const auto frame = tracking.latestFrame(); frame && frame->valid) {
            drawHandLandmarkOverlay(preview, *frame);
        }
        if (wizard.active) {
            drawCalibrationGuide(preview);
        }
    } else {
        using ::biofuel::engine::vision::hand_tracking::HandTrackingConnectionState;
        if (status.state == HandTrackingConnectionState::Error) {
            drawPreviewMessage(preview, "Camera preview failed", status.message, Color{244, 159, 140, 255});
        } else if (status.previewEnabled) {
            drawPreviewMessage(preview, "Camera preview warming up...", status.message, Color{191, 210, 204, 255});
        } else {
            drawPreviewMessage(preview, "Press C to start camera preview", {}, Color{191, 210, 204, 255});
        }
    }
}

void DevHandLabScreen::drawHandLandmarkOverlay(
    const Rectangle previewBounds,
    const ::biofuel::engine::vision::hand_tracking::HandTrackingFrame& frame) const noexcept
{
    using ::biofuel::engine::vision::hand_tracking::HandTrackingHand;
    using ::biofuel::engine::vision::hand_tracking::HandTrackingHandedness;
    using ::biofuel::engine::vision::hand_tracking::HandTrackingLandmark;

    constexpr std::array<std::array<usize, 2U>, 23U> connections{{
        {{0U, 1U}}, {{1U, 2U}}, {{2U, 3U}}, {{3U, 4U}},
        {{0U, 5U}}, {{5U, 6U}}, {{6U, 7U}}, {{7U, 8U}},
        {{0U, 9U}}, {{9U, 10U}}, {{10U, 11U}}, {{11U, 12U}},
        {{0U, 13U}}, {{13U, 14U}}, {{14U, 15U}}, {{15U, 16U}},
        {{0U, 17U}}, {{17U, 18U}}, {{18U, 19U}}, {{19U, 20U}},
        {{5U, 9U}}, {{9U, 13U}}, {{13U, 17U}},
    }};
    constexpr std::array<usize, 5U> tipLandmarks{{4U, 8U, 12U, 16U, 20U}};

    const auto point = [this, previewBounds](const HandTrackingLandmark landmark) noexcept {
        const HandTrackingLandmark display = m_handRetargeter.displayLandmark(landmark);
        return Vector2{
            previewBounds.x + std::clamp(display.x, 0.0f, 1.0f) * previewBounds.width,
            previewBounds.y + std::clamp(display.y, 0.0f, 1.0f) * previewBounds.height,
        };
    };

    const usize handLimit = std::min(static_cast<usize>(frame.handCount), frame.hands.size());
    for (usize handIndexValue = 0U; handIndexValue < handLimit; ++handIndexValue) {
        const HandTrackingHand& hand = frame.hands[handIndexValue];
        if (!hand.valid) {
            continue;
        }
        const Color lineColor = hand.handedness == HandTrackingHandedness::Left
            ? Color{80, 238, 146, 230}
            : Color{82, 166, 255, 230};
        for (const auto& connection : connections) {
            DrawLineEx(point(hand.imageLandmarks[connection[0]]), point(hand.imageLandmarks[connection[1]]), 2.0f, lineColor);
        }
        for (usize landmarkIndex = 0U; landmarkIndex < HandTrackingHand::LANDMARK_COUNT; ++landmarkIndex) {
            const bool tip = std::find(tipLandmarks.begin(), tipLandmarks.end(), landmarkIndex) != tipLandmarks.end();
            DrawCircleV(point(hand.imageLandmarks[landmarkIndex]), tip ? 4.0f : 2.5f, tip ? Color{255, 194, 72, 245} : Color{235, 250, 242, 235});
        }
    }
}

void DevHandLabScreen::drawCalibrationGuide(const Rectangle previewBounds) const noexcept {
    const auto wizard = m_handRetargeter.calibrationState();
    if (!wizard.active) {
        return;
    }

    const Vector2 target =
        ::biofuel::engine::custom::procedural::pose::calibrationTarget(wizard.step);
    const Vector2 targetPoint{
        previewBounds.x + target.x * previewBounds.width,
        previewBounds.y + target.y * previewBounds.height,
    };
    const f32 targetRadius =
        ::biofuel::engine::custom::procedural::pose::calibrationTargetRadius(wizard.step)
        * std::min(previewBounds.width, previewBounds.height);
    const Color targetColor = wizard.targetAcquired
        ? Color{84, 236, 148, 235}
        : Color{255, 203, 84, 230};
    const Color progressColor = wizard.targetAcquired
        ? Color{84, 236, 148, 255}
        : Color{255, 203, 84, 180};

    DrawCircleLinesV(targetPoint, targetRadius, targetColor);
    DrawCircleLinesV(targetPoint, targetRadius + 10.0f, wizard.targetAcquired ? Color{84, 236, 148, 140} : Color{255, 203, 84, 92});
    DrawCircleV(targetPoint, 4.0f, targetColor);

    const Rectangle card{
        previewBounds.x + 16.0f,
        previewBounds.y + previewBounds.height - 82.0f,
        previewBounds.width - 32.0f,
        64.0f,
    };
    DrawRectangleRec(card, Color{3, 8, 10, 216});
    DrawRectangleLinesEx(card, 1.0f, Color{84, 236, 148, 165});

    const std::string_view prompt =
        ::biofuel::engine::custom::procedural::pose::calibrationPrompt(wizard.activeHand, wizard.step);
    Renderer::drawText(
        prompt.data(),
        static_cast<i32>(card.x + 10.0f),
        static_cast<i32>(card.y + 7.0f),
        12,
        Color{233, 246, 238, 255});

    const std::string_view phaseName =
        ::biofuel::engine::custom::procedural::pose::calibrationHandPhaseName(wizard.activeHand);
    std::array<char, 96> stepText{};
    std::snprintf(stepText.data(), stepText.size(), "%.*s phase | step %u/%u",
        static_cast<int>(phaseName.size()),
        phaseName.data(),
        static_cast<unsigned>(::biofuel::engine::custom::procedural::pose::calibrationStepOrdinal(wizard.step)),
        static_cast<unsigned>(::biofuel::engine::custom::procedural::pose::calibrationStepCount()));
    Renderer::drawText(
        stepText.data(),
        static_cast<i32>(card.x + 10.0f),
        static_cast<i32>(card.y + 24.0f),
        10,
        Color{177, 212, 198, 255});

    const auto drawHandProgress = [&progressColor, card](std::string_view label, const auto progress, const f32 y) noexcept {
        const f32 ratio = std::clamp(
            progress.requiredHoldSeconds > 0.0f ? progress.holdSeconds / progress.requiredHoldSeconds : 0.0f,
            0.0f,
            1.0f);
        const std::string_view status =
            ::biofuel::engine::custom::procedural::pose::calibrationCaptureStatusName(progress.status);
        std::array<char, 96> text{};
        std::snprintf(text.data(), text.size(), "%.*s  %.*s",
            static_cast<int>(label.size()),
            label.data(),
            static_cast<int>(status.size()),
            status.data());
        Renderer::drawText(text.data(), static_cast<i32>(card.x + 10.0f), static_cast<i32>(y - 2.0f), 10, Color{214, 234, 226, 255});
        DrawRectangle(
            static_cast<i32>(card.x + 92.0f),
            static_cast<i32>(y),
            static_cast<i32>(card.width - 104.0f),
            8,
            Color{18, 28, 30, 255});
        DrawRectangle(
            static_cast<i32>(card.x + 92.0f),
            static_cast<i32>(y),
            static_cast<i32>((card.width - 104.0f) * ratio),
            8,
            progress.sampleCaptured ? Color{84, 236, 148, 255} : progressColor);
    };
    const auto activeProgress = wizard.activeHand == CalibrationHandPhase::Right ? wizard.right : wizard.left;
    drawHandProgress(phaseName, activeProgress, card.y + 46.0f);
}
#endif

} // namespace biofuel::game::screens

#endif // BIOFUEL_ENABLE_DEV_SCREENS
