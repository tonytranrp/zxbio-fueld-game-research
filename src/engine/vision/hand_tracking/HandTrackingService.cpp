#include "engine/vision/hand_tracking/HandTrackingService.hpp"

#include "engine/runtime/typed/Events.hpp"
#include "engine/events/hand_tracking/HandTrackingEventModule.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <limits>
#include <mutex>
#include <optional>
#include <spdlog/spdlog.h>
#include <thread>
#include <utility>
#include <raylib.h>

#ifdef BIOFUEL_ENABLE_HAND_TRACKING
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define CloseWindow Win32CloseWindow
#define ShowCursor Win32ShowCursor
#endif
#include <asio.hpp>
#include <atomic>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#undef CloseWindow
#undef ShowCursor
#else
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
#endif

namespace biofuel::engine::vision::hand_tracking {

namespace {

constexpr u16 PROTOCOL_VERSION = 1U;
constexpr u16 UDP_PORT = 40241U;
constexpr u16 CONTROL_PORT = 40242U;
constexpr u16 PREVIEW_PORT = 40243U;
constexpr f32 FRAME_STALE_SECONDS = 0.75f;
constexpr f32 STARTUP_GRACE_SECONDS = 12.0f;
constexpr f32 GESTURE_DEBOUNCE_SECONDS = 0.15f;
constexpr f32 HAND_IDENTITY_LOCK_CONFIDENCE = 0.70f;
constexpr f32 HAND_IDENTITY_RECOVERY_CONFIDENCE = 0.44f;
constexpr f32 HAND_IDENTITY_MAX_DISTANCE_SQUARED = 0.055f;
constexpr f32 LANDMARK_SMOOTH_MIN_RESPONSE = 9.0f;
constexpr f32 LANDMARK_SMOOTH_BASE_RESPONSE = 16.0f;
constexpr f32 LANDMARK_SMOOTH_FAST_RESPONSE = 34.0f;
constexpr usize PREVIEW_MAX_PIXELS = 1920U * 1080U;

[[nodiscard]] u64 nowEpochMs() noexcept {
    using Clock = std::chrono::system_clock;
    return static_cast<u64>(std::chrono::duration_cast<std::chrono::milliseconds>(
        Clock::now().time_since_epoch()).count());
}

[[nodiscard]] f32 secondsSince(const std::chrono::steady_clock::time_point timePoint) noexcept {
    using Clock = std::chrono::steady_clock;
    return std::chrono::duration<f32>(Clock::now() - timePoint).count();
}

[[nodiscard]] HandTrackingLandmark lerpLandmark(
    const HandTrackingLandmark a,
    const HandTrackingLandmark b,
    const f32 alpha) noexcept
{
    return HandTrackingLandmark{
        .x = a.x + (b.x - a.x) * alpha,
        .y = a.y + (b.y - a.y) * alpha,
        .z = a.z + (b.z - a.z) * alpha,
    };
}

[[nodiscard]] f32 sanitizeUnitScore(const f32 value) noexcept {
    return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.0f;
}

[[nodiscard]] f32 sanitizeCoordinate(const f32 value) noexcept {
    return std::isfinite(value) ? value : 0.0f;
}

[[nodiscard]] HandTrackingLandmark sanitizedLandmark(
    const f32 x,
    const f32 y,
    const f32 z) noexcept
{
    return HandTrackingLandmark{
        .x = sanitizeCoordinate(x),
        .y = sanitizeCoordinate(y),
        .z = sanitizeCoordinate(z),
    };
}

[[nodiscard]] bool validHandednessValue(const u8 value) noexcept {
    return value <= static_cast<u8>(HandTrackingHandedness::Right);
}

[[nodiscard]] bool validGestureValue(const u8 value) noexcept {
    return value <= static_cast<u8>(HandTrackingGesture::ILoveYou);
}

[[nodiscard]] Vector2 handPalmCenter2D(const HandTrackingHand& hand) noexcept {
    constexpr std::array<usize, 5U> palm{{0U, 5U, 9U, 13U, 17U}};
    Vector2 center{0.0f, 0.0f};
    for (const usize index : palm) {
        center.x += hand.imageLandmarks[index].x;
        center.y += hand.imageLandmarks[index].y;
    }
    return Vector2{center.x / static_cast<f32>(palm.size()), center.y / static_cast<f32>(palm.size())};
}

[[nodiscard]] f32 distanceSquared2D(const Vector2 a, const Vector2 b) noexcept {
    const f32 dx = a.x - b.x;
    const f32 dy = a.y - b.y;
    return dx * dx + dy * dy;
}

[[nodiscard]] std::filesystem::path appDirectory() {
    return std::filesystem::path{GetApplicationDirectory()};
}

[[nodiscard]] std::filesystem::path workerScriptPath() {
    return appDirectory() / "python" / "hand_tracking" / "worker.py";
}

[[nodiscard]] std::filesystem::path workerConfigPath() {
    return appDirectory() / "python" / "hand_tracking" / "default_config.json";
}

[[nodiscard]] std::filesystem::path workerModelPath() {
    return appDirectory() / "assets" / "vision" / "hand_tracking" / "gesture_recognizer.task";
}

[[nodiscard]] std::filesystem::path pythonExecutablePath() {
#ifdef BIOFUEL_HAND_TRACKING_PYTHON_EXECUTABLE
    return std::filesystem::path{BIOFUEL_HAND_TRACKING_PYTHON_EXECUTABLE};
#else
    return {};
#endif
}

[[nodiscard]] std::string controlJson(const std::string_view command, const bool previewEnabled) {
    std::ostringstream out;
    out << "{\"command\":\"" << command << "\","
        << "\"preview\":" << (previewEnabled ? "true" : "false") << ","
        << "\"udp_port\":" << UDP_PORT << ","
        << "\"preview_port\":" << PREVIEW_PORT << "}\n";
    return out.str();
}

#ifdef _WIN32
[[nodiscard]] std::wstring quotedWide(const std::filesystem::path& value) {
    const std::wstring path = value.wstring();
    std::wstring escaped;
    escaped.reserve(path.size() + 2U);
    escaped.push_back(L'"');
    for (const wchar_t ch : path) {
        if (ch == L'"') {
            escaped.push_back(L'\\');
        }
        escaped.push_back(ch);
    }
    escaped.push_back(L'"');
    return escaped;
}
#endif

} // namespace

class HandTrackingService::Impl final {
public:
    Impl() = default;
    ~Impl() noexcept { shutdown(); }

    void init() {
        std::scoped_lock lock(m_mutex);
        m_status.featureEnabled = featureEnabled();
        m_status.state = featureEnabled() ? HandTrackingConnectionState::Idle : HandTrackingConnectionState::Disabled;
        m_status.message = featureEnabled() ? "Hand tracking ready" : "Hand tracking feature is disabled";
    }

    void shutdown() noexcept {
        stop();
    }

    void update(const f32 dt) {
        (void)dt;
#ifdef BIOFUEL_ENABLE_HAND_TRACKING
        std::optional<HandTrackingFrame> pending;
        std::optional<HandTrackingFrame> previous;
        {
            std::scoped_lock lock(m_mutex);
            if (m_pendingFrame && (!m_latestFrame || m_pendingFrame->sequence != m_latestFrame->sequence)) {
                pending = m_pendingFrame;
            }
            previous = m_latestFrame;
        }

        if (pending) {
            HandTrackingFrame smoothed = smoothFrame(*pending, previous);
            {
                std::scoped_lock lock(m_mutex);
                m_latestFrame = smoothed;
                m_status.state = HandTrackingConnectionState::Online;
                m_status.secondsSinceLastFrame = 0.0f;
                m_status.message = "Tracking online";
                ++m_packetCountThisSecond;
            }
            publishFrame(smoothed);
            updateGestures(smoothed);
        }

        const f32 elapsed = secondsSince(m_packetRateWindowStart);
        if (elapsed >= 1.0f) {
            std::scoped_lock lock(m_mutex);
            m_status.packetsPerSecond = static_cast<f32>(m_packetCountThisSecond) / elapsed;
            m_packetCountThisSecond = 0U;
            m_packetRateWindowStart = std::chrono::steady_clock::now();
        }

        bool shouldPublishLostHands = false;
        bool startupTimedOut = false;
        std::string startupErrorMessage;
        {
            std::scoped_lock lock(m_mutex);
            if (m_latestFrame) {
                m_status.secondsSinceLastFrame = secondsSince(m_lastFrameSteady);
                if (m_status.secondsSinceLastFrame > FRAME_STALE_SECONDS && m_status.state == HandTrackingConnectionState::Online) {
                    m_status.state = HandTrackingConnectionState::Offline;
                    m_status.message = "Tracking packets are stale";
                    shouldPublishLostHands = true;
                }
                if (m_status.workerRunning && m_status.secondsSinceLastFrame > STARTUP_GRACE_SECONDS) {
                    startupTimedOut = true;
                    startupErrorMessage = "Tracking stream stalled. Press C to retry.";
                    m_status.state = HandTrackingConnectionState::Error;
                    m_status.message = startupErrorMessage;
                    m_status.workerRunning = false;
                }
            } else if (m_status.workerRunning && m_status.state == HandTrackingConnectionState::Starting) {
                m_status.secondsSinceLastFrame = secondsSince(m_lastFrameSteady);
                if (m_status.secondsSinceLastFrame > 1.0f) {
                    m_status.message = "Camera and MediaPipe model are warming up";
                }
                if (m_status.secondsSinceLastFrame > STARTUP_GRACE_SECONDS) {
                    startupTimedOut = true;
                    startupErrorMessage = "Camera/model startup timed out. Press C to retry.";
                    m_status.state = HandTrackingConnectionState::Error;
                    m_status.message = startupErrorMessage;
                    m_status.workerRunning = false;
                }
            }
        }
        if (shouldPublishLostHands) {
            publishLostHands();
        }
        if (startupTimedOut) {
            cleanupRuntimeResources();
            ::biofuel::engine::runtime::typed::Events::publish<::biofuel::engine::runtime::typed::hand_tracking::WorkerError>({
                startupErrorMessage,
            });
        }
#else
        std::scoped_lock lock(m_mutex);
        m_status.featureEnabled = false;
        m_status.state = HandTrackingConnectionState::Disabled;
        m_status.message = "Reconfigure with BIOFUEL_ENABLE_HAND_TRACKING=ON";
#endif
    }

    void requestCameraAccess() {
        {
            std::scoped_lock lock(m_mutex);
            m_status.cameraConsentRequested = true;
            m_status.state = HandTrackingConnectionState::WaitingForConsent;
            m_status.message = "Camera access requested";
        }
        ::biofuel::engine::runtime::typed::Events::publish<::biofuel::engine::runtime::typed::hand_tracking::CameraAccessRequested>();
    }

    void approveCameraAccess() {
        {
            std::scoped_lock lock(m_mutex);
            m_status.cameraConsentRequested = true;
            m_status.cameraConsentGranted = true;
            m_status.message = "Camera access granted";
        }
        ::biofuel::engine::runtime::typed::Events::publish<::biofuel::engine::runtime::typed::hand_tracking::CameraAccessChanged>({true});
    }

    void denyCameraAccess() {
        stop();
        {
            std::scoped_lock lock(m_mutex);
            m_status.cameraConsentRequested = true;
            m_status.cameraConsentGranted = false;
            m_status.state = HandTrackingConnectionState::Idle;
            m_status.message = "Camera access denied";
        }
        ::biofuel::engine::runtime::typed::Events::publish<::biofuel::engine::runtime::typed::hand_tracking::CameraAccessChanged>({false});
    }

    bool start() {
#ifndef BIOFUEL_ENABLE_HAND_TRACKING
        std::scoped_lock lock(m_mutex);
        m_status.state = HandTrackingConnectionState::Disabled;
        m_status.message = "Hand tracking was not compiled into this build";
        return false;
#else
        {
            bool publishCameraRequest = false;
            {
                std::scoped_lock lock(m_mutex);
                if (!m_status.cameraConsentGranted) {
                    m_status.cameraConsentRequested = true;
                    m_status.state = HandTrackingConnectionState::WaitingForConsent;
                    m_status.message = "Camera access required before tracking can start";
                    publishCameraRequest = true;
                }
            }
            if (publishCameraRequest) {
                ::biofuel::engine::runtime::typed::Events::publish<::biofuel::engine::runtime::typed::hand_tracking::CameraAccessRequested>();
                return false;
            }
        }

        startReceiver();
        if (!startWorkerProcess()) {
            cleanupRuntimeResources();
            return false;
        }

        const bool preview = previewEnabled();
        if (!sendControlWithRetry(controlJson("start", preview))) {
            setError("Could not connect to Python hand-tracking worker control port");
            cleanupRuntimeResources();
            return false;
        }

        if (preview) {
            startPreview();
        }

        {
            std::scoped_lock lock(m_mutex);
            m_status.state = HandTrackingConnectionState::Starting;
            m_status.workerRunning = true;
            m_status.message = "Tracking worker started";
            m_status.secondsSinceLastFrame = 0.0f;
            m_lastFrameSteady = std::chrono::steady_clock::now();
        }
        ::biofuel::engine::runtime::typed::Events::publish<::biofuel::engine::runtime::typed::hand_tracking::WorkerStarted>();
        return true;
#endif
    }

    void stop() noexcept {
#ifdef BIOFUEL_ENABLE_HAND_TRACKING
        cleanupRuntimeResources();
#endif
        std::scoped_lock lock(m_mutex);
        m_status.workerRunning = false;
        m_status.previewEnabled = false;
        m_status.state = featureEnabled() ? HandTrackingConnectionState::Idle : HandTrackingConnectionState::Disabled;
        m_status.message = featureEnabled() ? "Tracking stopped" : "Hand tracking disabled";
        m_pendingFrame.reset();
        m_latestFrame.reset();
        m_latestPreview.reset();
        m_lastPreviewSequence.reset();
    }

    void setPreviewEnabled(const bool enabled) {
        {
            std::scoped_lock lock(m_mutex);
            m_status.previewEnabled = enabled;
        }
#ifdef BIOFUEL_ENABLE_HAND_TRACKING
        if (running()) {
            (void)sendControl(controlJson("preview", enabled));
        }
        if (enabled) {
            startPreview();
        } else {
            stopPreviewThread();
        }
#endif
    }

    [[nodiscard]] bool featureEnabled() const noexcept {
#ifdef BIOFUEL_ENABLE_HAND_TRACKING
        return true;
#else
        return false;
#endif
    }

    [[nodiscard]] bool cameraConsentGranted() const noexcept {
        std::scoped_lock lock(m_mutex);
        return m_status.cameraConsentGranted;
    }

    [[nodiscard]] bool cameraConsentRequested() const noexcept {
        std::scoped_lock lock(m_mutex);
        return m_status.cameraConsentRequested;
    }

    [[nodiscard]] bool running() const noexcept {
        std::scoped_lock lock(m_mutex);
        return m_status.workerRunning;
    }

    [[nodiscard]] bool previewEnabled() const noexcept {
        std::scoped_lock lock(m_mutex);
        return m_status.previewEnabled;
    }

    [[nodiscard]] HandTrackingStatus status() const {
        std::scoped_lock lock(m_mutex);
        return m_status;
    }

    [[nodiscard]] std::optional<HandTrackingFrame> latestFrame() const {
        std::scoped_lock lock(m_mutex);
        return m_latestFrame;
    }

    [[nodiscard]] std::optional<std::shared_ptr<const HandTrackingPreviewFrame>> latestPreviewFrame() const {
        std::scoped_lock lock(m_mutex);
        if (!m_latestPreview) {
            return std::nullopt;
        }
        return m_latestPreview;
    }

    [[nodiscard]] std::optional<std::shared_ptr<const HandTrackingPreviewFrame>> latestPreviewFrameAfter(const u64 sequence) const {
        std::scoped_lock lock(m_mutex);
        if (!m_latestPreview || (*m_latestPreview)->sequence == sequence) {
            return std::nullopt;
        }
        return m_latestPreview;
    }

private:
#ifdef BIOFUEL_ENABLE_HAND_TRACKING
#pragma pack(push, 1)
    struct RawLandmark {
        f32 x;
        f32 y;
        f32 z;
    };

    struct RawHand {
        u8 valid;
        u8 handedness;
        u8 gesture;
        u8 reserved;
        f32 handednessScore;
        f32 gestureScore;
        RawLandmark image[HandTrackingHand::LANDMARK_COUNT];
        RawLandmark world[HandTrackingHand::LANDMARK_COUNT];
    };

    struct RawPacket {
        std::array<char, 4> magic{};
        u16 version;
        u16 headerSize;
        u64 sequence;
        u64 timestampMs;
        u16 cameraWidth;
        u16 cameraHeight;
        u8 handCount;
        u8 flags;
        u16 reserved;
        RawHand hands[HandTrackingFrame::MAX_HANDS];
    };

    struct PreviewHeader {
        std::array<char, 4> magic{};
        u64 sequence;
        u32 byteCount;
    };
#pragma pack(pop)

    static_assert(sizeof(RawPacket) == 1064U);
    static_assert(sizeof(PreviewHeader) == 16U);

    void cleanupRuntimeResources() noexcept {
        try {
            (void)sendControl(controlJson("shutdown", false));
        } catch (...) {
        }
        stopReceiverThread();
        stopPreviewThread();
        stopWorkerProcess();
    }

    void startReceiver() {
        if (m_receiverRunning.load()) {
            return;
        }
        if (m_receiverThread.joinable()) {
            m_receiverThread.join();
        }
        m_receiverRunning.store(true);
        m_receiverThread = std::thread([this] { receiverLoop(); });
    }

    void startPreview() {
        if (m_previewRunning.load()) {
            return;
        }
        if (m_previewThread.joinable()) {
            m_previewThread.join();
        }
        m_previewRunning.store(true);
        m_previewThread = std::thread([this] { previewLoop(); });
    }

    void stopReceiverThread() noexcept {
        m_receiverRunning.store(false);
        if (m_receiverThread.joinable()) {
            m_receiverThread.join();
        }
    }

    void stopPreviewThread() noexcept {
        m_previewRunning.store(false);
        if (m_previewThread.joinable()) {
            m_previewThread.join();
        }
    }

    void receiverLoop() {
        try {
            asio::io_context io;
            asio::ip::udp::socket socket(io, asio::ip::udp::endpoint(asio::ip::address_v4::loopback(), UDP_PORT));
            socket.non_blocking(true);
            std::array<u8, sizeof(RawPacket)> buffer{};
            asio::ip::udp::endpoint sender;
            while (m_receiverRunning.load()) {
                asio::error_code ec;
                const std::size_t received = socket.receive_from(asio::buffer(buffer), sender, 0, ec);
                if (!ec && received == sizeof(RawPacket) && sender.address().is_loopback()) {
                    if (auto frame = parsePacket(buffer)) {
                        std::scoped_lock lock(m_mutex);
                        m_pendingFrame = *frame;
                        m_lastFrameSteady = std::chrono::steady_clock::now();
                    }
                } else if (ec == asio::error::would_block || ec == asio::error::try_again) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                } else if (ec) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }
        } catch (const std::exception& ex) {
            setError(std::string{"UDP receiver failed: "} + ex.what());
        }
        m_receiverRunning.store(false);
    }

    [[nodiscard]] static bool readPreviewBytes(
        asio::ip::tcp::socket& socket,
        void* data,
        const std::size_t byteCount,
        const std::atomic<bool>& running)
    {
        auto* cursor = static_cast<u8*>(data);
        std::size_t total = 0U;
        while (running.load() && total < byteCount) {
            asio::error_code ec;
            const std::size_t received = socket.read_some(asio::buffer(cursor + total, byteCount - total), ec);
            if (!ec) {
                if (received == 0U) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    continue;
                }
                total += received;
                continue;
            }
            if (ec == asio::error::would_block || ec == asio::error::try_again) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            return false;
        }
        return total == byteCount;
    }

    void previewLoop() {
        while (m_previewRunning.load()) {
            try {
                asio::io_context io;
                asio::ip::tcp::socket socket(io);
                socket.connect(asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), PREVIEW_PORT));
                socket.non_blocking(true);
                while (m_previewRunning.load()) {
                    PreviewHeader header{};
                    if (!readPreviewBytes(socket, &header, sizeof(header), m_previewRunning)) {
                        break;
                    }
                    if (std::memcmp(header.magic.data(), "MJPG", 4U) != 0 || header.byteCount == 0U || header.byteCount > 2'500'000U) {
                        break;
                    }
                    if (m_lastPreviewSequence.has_value() && header.sequence <= *m_lastPreviewSequence) {
                        std::vector<u8> discard(header.byteCount);
                        if (!readPreviewBytes(socket, discard.data(), discard.size(), m_previewRunning)) {
                            break;
                        }
                        continue;
                    }
                    auto frame = std::make_shared<HandTrackingPreviewFrame>();
                    frame->sequence = header.sequence;
                    frame->jpegBytes.resize(header.byteCount);
                    if (!readPreviewBytes(socket, frame->jpegBytes.data(), frame->jpegBytes.size(), m_previewRunning)) {
                        break;
                    }
                    decodePreviewFrame(*frame);
                    {
                        std::scoped_lock lock(m_mutex);
                        m_lastPreviewSequence = frame->sequence;
                        m_latestPreview = frame;
                    }
                }
            } catch (const std::exception&) {
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }
        }
        m_previewRunning.store(false);
    }

    static void decodePreviewFrame(HandTrackingPreviewFrame& frame) noexcept {
        if (frame.jpegBytes.size() > static_cast<usize>(std::numeric_limits<i32>::max())) {
            return;
        }

        Image image = LoadImageFromMemory(".jpg", frame.jpegBytes.data(), static_cast<i32>(frame.jpegBytes.size()));
        if (image.data == nullptr) {
            return;
        }
        ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        const i32 width = image.width;
        const i32 height = image.height;
        if (width > 0 && height > 0 && width <= static_cast<i32>(std::numeric_limits<u16>::max())
            && height <= static_cast<i32>(std::numeric_limits<u16>::max())) {
            const usize byteCount = static_cast<usize>(width) * static_cast<usize>(height) * 4U;
            if (byteCount / 4U > PREVIEW_MAX_PIXELS) {
                UnloadImage(image);
                return;
            }
            frame.rgbaBytes.resize(byteCount);
            std::memcpy(frame.rgbaBytes.data(), image.data, byteCount);
            frame.width = static_cast<u16>(width);
            frame.height = static_cast<u16>(height);
            frame.jpegBytes.clear();
        }
        UnloadImage(image);
    }

    [[nodiscard]] static std::optional<HandTrackingFrame> parsePacket(const std::array<u8, sizeof(RawPacket)>& buffer) {
        RawPacket raw{};
        std::memcpy(&raw, buffer.data(), sizeof(raw));
        if (std::memcmp(raw.magic.data(), "BHTK", 4U) != 0 || raw.version != PROTOCOL_VERSION || raw.headerSize != 32U) {
            return std::nullopt;
        }

        HandTrackingFrame frame{};
        frame.valid = true;
        frame.sequence = raw.sequence;
        frame.timestampMs = raw.timestampMs;
        frame.cameraWidth = raw.cameraWidth;
        frame.cameraHeight = raw.cameraHeight;
        const u8 packetHandCount = std::min<u8>(raw.handCount, static_cast<u8>(HandTrackingFrame::MAX_HANDS));
        const u64 nowMs = nowEpochMs();
        frame.latencyMs = raw.timestampMs > nowMs ? 0.0f : static_cast<f32>(nowMs - raw.timestampMs);

        for (usize handIndexValue = 0U; handIndexValue < HandTrackingFrame::MAX_HANDS; ++handIndexValue) {
            const RawHand& rawHand = raw.hands[handIndexValue];
            HandTrackingHand& hand = frame.hands[handIndexValue];
            hand.valid = handIndexValue < packetHandCount && rawHand.valid != 0U;
            hand.handedness = validHandednessValue(rawHand.handedness)
                ? static_cast<HandTrackingHandedness>(rawHand.handedness)
                : HandTrackingHandedness::Unknown;
            hand.gesture = validGestureValue(rawHand.gesture)
                ? static_cast<HandTrackingGesture>(rawHand.gesture)
                : HandTrackingGesture::Unknown;
            hand.handednessScore = sanitizeUnitScore(rawHand.handednessScore);
            hand.gestureScore = sanitizeUnitScore(rawHand.gestureScore);
            for (usize landmark = 0U; landmark < HandTrackingHand::LANDMARK_COUNT; ++landmark) {
                hand.imageLandmarks[landmark] = sanitizedLandmark(
                    rawHand.image[landmark].x,
                    rawHand.image[landmark].y,
                    rawHand.image[landmark].z);
                hand.worldLandmarks[landmark] = sanitizedLandmark(
                    rawHand.world[landmark].x,
                    rawHand.world[landmark].y,
                    rawHand.world[landmark].z);
            }
            if (hand.valid) {
                ++frame.handCount;
            }
        }
        return frame;
    }

    [[nodiscard]] bool startWorkerProcess() {
        {
            std::scoped_lock lock(m_mutex);
            if (m_status.workerRunning) {
                return true;
            }
            m_status.state = HandTrackingConnectionState::Starting;
            m_status.message = "Starting Python hand-tracking worker";
        }

        const std::filesystem::path python = pythonExecutablePath();
        const std::filesystem::path script = workerScriptPath();
        const std::filesystem::path model = workerModelPath();
        const std::filesystem::path config = workerConfigPath();
        if (python.empty() || !std::filesystem::exists(python)) {
            setError("Python 3.12 worker runtime is missing. Build the SetupHandTrackingPython target.");
            return false;
        }
        if (!std::filesystem::exists(script)) {
            setError("Python hand-tracking worker script is missing from runtime output.");
            return false;
        }
        if (!std::filesystem::exists(model)) {
            setError("MediaPipe gesture_recognizer.task asset is missing.");
            return false;
        }

#ifdef _WIN32
        std::wstring command =
            quotedWide(python) + L" " +
            quotedWide(script) +
            L" --model " + quotedWide(model) +
            L" --config " + quotedWide(config) +
            L" --udp-host 127.0.0.1" +
            L" --udp-port " + std::to_wstring(UDP_PORT) +
            L" --control-port " + std::to_wstring(CONTROL_PORT) +
            L" --preview-port " + std::to_wstring(PREVIEW_PORT) +
            L" --parent-pid " + std::to_wstring(currentProcessId());
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) {
            setError("CreateProcessW failed while starting Python hand-tracking worker");
            return false;
        }
        m_processHandle = process.hProcess;
        CloseHandle(process.hThread);
#else
        const pid_t pid = fork();
        if (pid == 0) {
            execl(python.c_str(), python.c_str(), script.c_str(),
                "--model", model.c_str(),
                "--config", config.c_str(),
                "--udp-host", "127.0.0.1",
                "--udp-port", std::to_string(UDP_PORT).c_str(),
                "--control-port", std::to_string(CONTROL_PORT).c_str(),
                "--preview-port", std::to_string(PREVIEW_PORT).c_str(),
                "--parent-pid", std::to_string(currentProcessId()).c_str(),
                static_cast<char*>(nullptr));
            std::_Exit(127);
        }
        if (pid < 0) {
            setError("fork failed while starting Python hand-tracking worker");
            return false;
        }
        m_processId = pid;
#endif
        return true;
    }

    void stopWorkerProcess() noexcept {
#ifdef _WIN32
        if (m_processHandle != nullptr) {
            WaitForSingleObject(m_processHandle, 500);
            DWORD exitCode = 0U;
            if (GetExitCodeProcess(m_processHandle, &exitCode) && exitCode == STILL_ACTIVE) {
                TerminateProcess(m_processHandle, 0U);
            }
            CloseHandle(m_processHandle);
            m_processHandle = nullptr;
        }
#else
        if (m_processId > 0) {
            int status = 0;
            const pid_t result = waitpid(m_processId, &status, WNOHANG);
            if (result == 0) {
                kill(m_processId, SIGTERM);
                (void)waitpid(m_processId, &status, 0);
            }
            m_processId = -1;
        }
#endif
        ::biofuel::engine::runtime::typed::Events::publish<::biofuel::engine::runtime::typed::hand_tracking::WorkerStopped>();
    }

    [[nodiscard]] static u32 currentProcessId() noexcept {
#ifdef _WIN32
        return static_cast<u32>(GetCurrentProcessId());
#else
        return static_cast<u32>(getpid());
#endif
    }

    [[nodiscard]] bool sendControlWithRetry(const std::string& command) {
        for (i32 attempt = 0; attempt < 50; ++attempt) {
            if (sendControl(command)) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return false;
    }

    [[nodiscard]] bool sendControl(const std::string& command) {
        try {
            asio::io_context io;
            asio::ip::tcp::socket socket(io);
            socket.connect(asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), CONTROL_PORT));
            asio::write(socket, asio::buffer(command.data(), command.size()));
            return true;
        } catch (const std::exception&) {
            return false;
        }
    }
#endif

    struct PreviousHandMatch {
        const HandTrackingHand* hand = nullptr;
        usize index = 0U;
        f32 distanceSquared = std::numeric_limits<f32>::max();
    };

    [[nodiscard]] static bool confidentHandedness(const HandTrackingHand& hand) noexcept {
        return hand.handedness != HandTrackingHandedness::Unknown
            && hand.handednessScore >= HAND_IDENTITY_LOCK_CONFIDENCE;
    }

    [[nodiscard]] static std::optional<PreviousHandMatch> matchingPreviousHand(
        const HandTrackingHand& current,
        const HandTrackingFrame& previous,
        const std::array<bool, HandTrackingFrame::MAX_HANDS>& usedPrevious) noexcept
    {
        const Vector2 currentPalm = handPalmCenter2D(current);
        std::optional<PreviousHandMatch> best;
        f32 bestScore = std::numeric_limits<f32>::max();
        for (usize index = 0U; index < previous.hands.size(); ++index) {
            if (usedPrevious[index]) {
                continue;
            }
            const HandTrackingHand& candidate = previous.hands[index];
            if (!candidate.valid) {
                continue;
            }
            const f32 distance = distanceSquared2D(currentPalm, handPalmCenter2D(candidate));
            if (distance > HAND_IDENTITY_MAX_DISTANCE_SQUARED) {
                continue;
            }

            f32 score = distance;
            if (current.handedness != HandTrackingHandedness::Unknown
                && candidate.handedness != HandTrackingHandedness::Unknown
                && current.handedness != candidate.handedness) {
                const bool strongConflict = confidentHandedness(current) && confidentHandedness(candidate);
                score += strongConflict ? 0.08f : 0.012f;
            } else if (current.handedness == candidate.handedness
                && current.handedness != HandTrackingHandedness::Unknown) {
                score *= 0.55f;
            }

            if (score < bestScore) {
                bestScore = score;
                best = PreviousHandMatch{
                    .hand = &candidate,
                    .index = index,
                    .distanceSquared = distance,
                };
            }
        }
        return best;
    }

    static void stabilizeHandIdentity(
        HandTrackingHand& current,
        const HandTrackingHand& previous,
        const f32 distanceSquared) noexcept
    {
        if (previous.handedness == HandTrackingHandedness::Unknown) {
            return;
        }

        const bool currentAmbiguous =
            current.handedness == HandTrackingHandedness::Unknown
            || current.handednessScore < HAND_IDENTITY_LOCK_CONFIDENCE;
        const bool closeContinuation = distanceSquared <= HAND_IDENTITY_MAX_DISTANCE_SQUARED * 0.65f;
        const bool recoverableConflict =
            current.handedness != previous.handedness
            && current.handednessScore < HAND_IDENTITY_LOCK_CONFIDENCE + 0.10f
            && previous.handednessScore >= HAND_IDENTITY_RECOVERY_CONFIDENCE
            && closeContinuation;

        if (!currentAmbiguous && !recoverableConflict) {
            return;
        }

        current.handedness = previous.handedness;
        current.handednessScore = std::max(
            current.handednessScore,
            std::clamp(previous.handednessScore * 0.88f, HAND_IDENTITY_RECOVERY_CONFIDENCE, 1.0f));
    }

    [[nodiscard]] static f32 frameDeltaSeconds(
        const HandTrackingFrame& current,
        const HandTrackingFrame& previous) noexcept
    {
        if (current.timestampMs <= previous.timestampMs) {
            return 1.0f / 30.0f;
        }
        const f32 seconds = static_cast<f32>(current.timestampMs - previous.timestampMs) / 1000.0f;
        return std::clamp(seconds, 1.0f / 120.0f, 0.12f);
    }

    [[nodiscard]] static f32 landmarkSmoothingAlpha(
        const HandTrackingHand& current,
        const HandTrackingHand& previous,
        const f32 distanceSquared,
        const f32 dt) noexcept
    {
        const f32 palmMotion = std::sqrt(std::max(distanceSquared, 0.0f));
        const f32 motionT = std::clamp((palmMotion - 0.010f) / 0.070f, 0.0f, 1.0f);
        f32 response = LANDMARK_SMOOTH_BASE_RESPONSE
            + (LANDMARK_SMOOTH_FAST_RESPONSE - LANDMARK_SMOOTH_BASE_RESPONSE) * motionT;
        if (current.handedness == HandTrackingHandedness::Unknown
            || current.handednessScore < HAND_IDENTITY_RECOVERY_CONFIDENCE
            || previous.handednessScore < HAND_IDENTITY_RECOVERY_CONFIDENCE) {
            response = LANDMARK_SMOOTH_MIN_RESPONSE;
        }
        const f32 alpha = 1.0f - std::exp(-response * dt);
        return std::clamp(alpha, 0.08f, 0.92f);
    }

    [[nodiscard]] HandTrackingFrame smoothFrame(const HandTrackingFrame& raw, const std::optional<HandTrackingFrame>& previous) const {
        if (!previous) {
            return raw;
        }

        const f32 frameDt = frameDeltaSeconds(raw, *previous);
        HandTrackingFrame result = raw;
        std::array<bool, HandTrackingFrame::MAX_HANDS> usedPrevious{};
        for (auto& hand : result.hands) {
            if (!hand.valid) {
                continue;
            }
            const auto match = matchingPreviousHand(hand, *previous, usedPrevious);
            if (!match || match->hand == nullptr) {
                continue;
            }
            usedPrevious[match->index] = true;
            stabilizeHandIdentity(hand, *match->hand, match->distanceSquared);
            const f32 alpha = landmarkSmoothingAlpha(hand, *match->hand, match->distanceSquared, frameDt);
            for (usize index = 0U; index < HandTrackingHand::LANDMARK_COUNT; ++index) {
                hand.imageLandmarks[index] = lerpLandmark(match->hand->imageLandmarks[index], hand.imageLandmarks[index], alpha);
                hand.worldLandmarks[index] = lerpLandmark(match->hand->worldLandmarks[index], hand.worldLandmarks[index], alpha);
            }
        }
        return result;
    }

    void updateGestures(const HandTrackingFrame& frame) {
        const auto now = std::chrono::steady_clock::now();
        for (const auto& hand : frame.hands) {
            if (!hand.valid || hand.handedness == HandTrackingHandedness::Unknown) {
                continue;
            }
            GestureDebounce& state = m_gestures[handIndex(hand.handedness)];
            if (state.candidate != hand.gesture) {
                state.candidate = hand.gesture;
                state.candidateSince = now;
                continue;
            }
            const f32 heldSeconds = std::chrono::duration<f32>(now - state.candidateSince).count();
            if (heldSeconds >= GESTURE_DEBOUNCE_SECONDS && state.stable != hand.gesture) {
                state.stable = hand.gesture;
                ::biofuel::engine::runtime::typed::Events::publish<::biofuel::engine::runtime::typed::hand_tracking::GestureChanged>({
                    hand.handedness,
                    hand.gesture,
                    hand.gestureScore,
                });
            }
        }
    }

    void publishFrame(const HandTrackingFrame& frame) {
        ::biofuel::engine::runtime::typed::Events::publish<::biofuel::engine::runtime::typed::hand_tracking::FrameReceived>({frame});
    }

    void publishLostHands() {
        ::biofuel::engine::runtime::typed::Events::publish<::biofuel::engine::runtime::typed::hand_tracking::HandLost>({
            HandTrackingHandedness::Left,
        });
        ::biofuel::engine::runtime::typed::Events::publish<::biofuel::engine::runtime::typed::hand_tracking::HandLost>({
            HandTrackingHandedness::Right,
        });
    }

    void setError(const std::string& message) {
        {
            std::scoped_lock lock(m_mutex);
            m_status.state = HandTrackingConnectionState::Error;
            m_status.message = message;
            m_status.workerRunning = false;
        }
        spdlog::warn("{}", message);
        ::biofuel::engine::runtime::typed::Events::publish<::biofuel::engine::runtime::typed::hand_tracking::WorkerError>({message});
    }

    struct GestureDebounce {
        HandTrackingGesture stable = HandTrackingGesture::Unknown;
        HandTrackingGesture candidate = HandTrackingGesture::Unknown;
        std::chrono::steady_clock::time_point candidateSince = std::chrono::steady_clock::now();
    };

    mutable std::mutex m_mutex;
    HandTrackingStatus m_status{};
    std::optional<HandTrackingFrame> m_pendingFrame{};
    std::optional<HandTrackingFrame> m_latestFrame{};
    std::optional<std::shared_ptr<HandTrackingPreviewFrame>> m_latestPreview{};
    std::optional<u64> m_lastPreviewSequence{};
    std::array<GestureDebounce, 2U> m_gestures{};
    std::chrono::steady_clock::time_point m_lastFrameSteady = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point m_packetRateWindowStart = std::chrono::steady_clock::now();
    u32 m_packetCountThisSecond = 0U;

#ifdef BIOFUEL_ENABLE_HAND_TRACKING
    std::atomic<bool> m_receiverRunning{false};
    std::atomic<bool> m_previewRunning{false};
    std::thread m_receiverThread{};
    std::thread m_previewThread{};
#ifdef _WIN32
    HANDLE m_processHandle = nullptr;
#else
    pid_t m_processId = -1;
#endif
#endif
};

HandTrackingService::HandTrackingService()
    : m_impl(std::make_unique<Impl>())
{
}

HandTrackingService::~HandTrackingService() noexcept = default;

void HandTrackingService::init() {
    m_impl->init();
}

void HandTrackingService::shutdown() noexcept {
    m_impl->shutdown();
}

void HandTrackingService::update(const f32 dt) {
    m_impl->update(dt);
}

void HandTrackingService::requestCameraAccess() {
    m_impl->requestCameraAccess();
}

void HandTrackingService::approveCameraAccess() {
    m_impl->approveCameraAccess();
}

void HandTrackingService::denyCameraAccess() {
    m_impl->denyCameraAccess();
}

bool HandTrackingService::start() {
    return m_impl->start();
}

void HandTrackingService::stop() noexcept {
    m_impl->stop();
}

void HandTrackingService::setPreviewEnabled(const bool enabled) {
    m_impl->setPreviewEnabled(enabled);
}

bool HandTrackingService::featureEnabled() const noexcept {
    return m_impl->featureEnabled();
}

bool HandTrackingService::cameraConsentGranted() const noexcept {
    return m_impl->cameraConsentGranted();
}

bool HandTrackingService::cameraConsentRequested() const noexcept {
    return m_impl->cameraConsentRequested();
}

bool HandTrackingService::running() const noexcept {
    return m_impl->running();
}

bool HandTrackingService::previewEnabled() const noexcept {
    return m_impl->previewEnabled();
}

HandTrackingStatus HandTrackingService::status() const {
    return m_impl->status();
}

std::optional<HandTrackingFrame> HandTrackingService::latestFrame() const {
    return m_impl->latestFrame();
}

std::optional<std::shared_ptr<const HandTrackingPreviewFrame>> HandTrackingService::latestPreviewFrame() const {
    return m_impl->latestPreviewFrame();
}

std::optional<std::shared_ptr<const HandTrackingPreviewFrame>> HandTrackingService::latestPreviewFrameAfter(const u64 sequence) const {
    return m_impl->latestPreviewFrameAfter(sequence);
}

} // namespace biofuel::engine::vision::hand_tracking
