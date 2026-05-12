#include "engine/vision/hand_tracking/HandTrackingService.hpp"

#include "engine/runtime/typed/Events.hpp"
#include "engine/events/hand_tracking/HandTrackingEventModule.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <mutex>
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
constexpr f32 GESTURE_DEBOUNCE_SECONDS = 0.15f;

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

[[nodiscard]] std::string quotedUtf8(const std::filesystem::path& value) {
    std::string path = value.generic_string();
    std::string escaped;
    escaped.reserve(path.size() + 2U);
    escaped.push_back('"');
    for (const char ch : path) {
        if (ch == '"') {
            escaped.push_back('\\');
        }
        escaped.push_back(ch);
    }
    escaped.push_back('"');
    return escaped;
}

[[nodiscard]] std::string narrowPath(const std::filesystem::path& path) {
    return path.generic_string();
}

[[nodiscard]] std::string controlJson(const std::string_view command, const bool previewEnabled) {
    std::ostringstream out;
    out << "{\"command\":\"" << command << "\","
        << "\"preview\":" << (previewEnabled ? "true" : "false") << ","
        << "\"camera_index\":0,"
        << "\"udp_port\":" << UDP_PORT << ","
        << "\"preview_port\":" << PREVIEW_PORT << "}\n";
    return out.str();
}

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
        {
            std::scoped_lock lock(m_mutex);
            if (m_pendingFrame && (!m_latestFrame || m_pendingFrame->sequence != m_latestFrame->sequence)) {
                pending = m_pendingFrame;
            }
        }

        if (pending) {
            HandTrackingFrame smoothed = smoothFrame(*pending);
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

        std::scoped_lock lock(m_mutex);
        if (m_latestFrame) {
            m_status.secondsSinceLastFrame = secondsSince(m_lastFrameSteady);
            if (m_status.secondsSinceLastFrame > FRAME_STALE_SECONDS && m_status.state == HandTrackingConnectionState::Online) {
                m_status.state = HandTrackingConnectionState::Offline;
                m_status.message = "Tracking packets are stale";
                publishLostHands();
            }
        }
#else
        std::scoped_lock lock(m_mutex);
        m_status.featureEnabled = false;
        m_status.state = HandTrackingConnectionState::Disabled;
        m_status.message = "Reconfigure with BIOFUEL_ENABLE_HAND_TRACKING=ON";
#endif
    }

    void requestCameraAccess() {
        std::scoped_lock lock(m_mutex);
        m_status.cameraConsentRequested = true;
        m_status.state = HandTrackingConnectionState::WaitingForConsent;
        m_status.message = "Camera access requested";
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
            std::scoped_lock lock(m_mutex);
            if (!m_status.cameraConsentGranted) {
                m_status.cameraConsentRequested = true;
                m_status.state = HandTrackingConnectionState::WaitingForConsent;
                m_status.message = "Camera access required before tracking can start";
                ::biofuel::engine::runtime::typed::Events::publish<::biofuel::engine::runtime::typed::hand_tracking::CameraAccessRequested>();
                return false;
            }
        }

        startReceiver();
        if (!startWorkerProcess()) {
            return false;
        }

        const bool preview = previewEnabled();
        if (!sendControlWithRetry(controlJson("start", preview))) {
            setError("Could not connect to Python hand-tracking worker control port");
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
        }
        ::biofuel::engine::runtime::typed::Events::publish<::biofuel::engine::runtime::typed::hand_tracking::WorkerStarted>();
        return true;
#endif
    }

    void stop() noexcept {
#ifdef BIOFUEL_ENABLE_HAND_TRACKING
        m_receiverRunning.store(false);
        m_previewRunning.store(false);
        try {
            (void)sendControl(controlJson("shutdown", false));
        } catch (...) {
        }
        if (m_receiverThread.joinable()) {
            m_receiverThread.join();
        }
        if (m_previewThread.joinable()) {
            m_previewThread.join();
        }
        stopWorkerProcess();
#endif
        std::scoped_lock lock(m_mutex);
        m_status.workerRunning = false;
        m_status.previewEnabled = false;
        m_status.state = featureEnabled() ? HandTrackingConnectionState::Idle : HandTrackingConnectionState::Disabled;
        m_status.message = featureEnabled() ? "Tracking stopped" : "Hand tracking disabled";
        m_pendingFrame.reset();
        m_latestFrame.reset();
    }

    void setPreviewEnabled(const bool enabled) {
        {
            std::scoped_lock lock(m_mutex);
            m_status.previewEnabled = enabled;
        }
#ifdef BIOFUEL_ENABLE_HAND_TRACKING
        if (enabled) {
            startPreview();
        } else {
            m_previewRunning.store(false);
        }
        if (running()) {
            (void)sendControl(controlJson("preview", enabled));
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

    [[nodiscard]] std::optional<HandTrackingPreviewFrame> latestPreviewFrame() const {
        std::scoped_lock lock(m_mutex);
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
        char magic[4];
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
        char magic[4];
        u64 sequence;
        u32 byteCount;
    };
#pragma pack(pop)

    static_assert(sizeof(RawPacket) == 1064U);
    static_assert(sizeof(PreviewHeader) == 16U);

    void startReceiver() {
        if (m_receiverRunning.load()) {
            return;
        }
        m_receiverRunning.store(true);
        m_receiverThread = std::thread([this] { receiverLoop(); });
    }

    void startPreview() {
        if (m_previewRunning.load()) {
            return;
        }
        m_previewRunning.store(true);
        m_previewThread = std::thread([this] { previewLoop(); });
    }

    void receiverLoop() {
        try {
            asio::io_context io;
            asio::ip::udp::socket socket(io, asio::ip::udp::endpoint(asio::ip::udp::v4(), UDP_PORT));
            socket.non_blocking(true);
            std::array<u8, sizeof(RawPacket)> buffer{};
            asio::ip::udp::endpoint sender;
            while (m_receiverRunning.load()) {
                asio::error_code ec;
                const std::size_t received = socket.receive_from(asio::buffer(buffer), sender, 0, ec);
                if (!ec && received == sizeof(RawPacket)) {
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
    }

    void previewLoop() {
        while (m_previewRunning.load()) {
            try {
                asio::io_context io;
                asio::ip::tcp::socket socket(io);
                socket.connect(asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), PREVIEW_PORT));
                while (m_previewRunning.load()) {
                    PreviewHeader header{};
                    asio::read(socket, asio::buffer(&header, sizeof(header)));
                    if (std::memcmp(header.magic, "MJPG", 4U) != 0 || header.byteCount == 0U || header.byteCount > 2'500'000U) {
                        break;
                    }
                    HandTrackingPreviewFrame frame{};
                    frame.sequence = header.sequence;
                    frame.jpegBytes.resize(header.byteCount);
                    asio::read(socket, asio::buffer(frame.jpegBytes.data(), frame.jpegBytes.size()));
                    {
                        std::scoped_lock lock(m_mutex);
                        m_latestPreview = std::move(frame);
                    }
                }
            } catch (const std::exception&) {
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }
        }
    }

    [[nodiscard]] static std::optional<HandTrackingFrame> parsePacket(const std::array<u8, sizeof(RawPacket)>& buffer) {
        RawPacket raw{};
        std::memcpy(&raw, buffer.data(), sizeof(raw));
        if (std::memcmp(raw.magic, "BHTK", 4U) != 0 || raw.version != PROTOCOL_VERSION || raw.headerSize != 32U) {
            return std::nullopt;
        }

        HandTrackingFrame frame{};
        frame.valid = true;
        frame.sequence = raw.sequence;
        frame.timestampMs = raw.timestampMs;
        frame.cameraWidth = raw.cameraWidth;
        frame.cameraHeight = raw.cameraHeight;
        frame.handCount = std::min<u8>(raw.handCount, static_cast<u8>(HandTrackingFrame::MAX_HANDS));
        const u64 nowMs = nowEpochMs();
        frame.latencyMs = raw.timestampMs > nowMs ? 0.0f : static_cast<f32>(nowMs - raw.timestampMs);

        for (usize handIndexValue = 0U; handIndexValue < HandTrackingFrame::MAX_HANDS; ++handIndexValue) {
            const RawHand& rawHand = raw.hands[handIndexValue];
            HandTrackingHand& hand = frame.hands[handIndexValue];
            hand.valid = rawHand.valid != 0U;
            hand.handedness = static_cast<HandTrackingHandedness>(rawHand.handedness);
            hand.gesture = static_cast<HandTrackingGesture>(rawHand.gesture);
            hand.handednessScore = rawHand.handednessScore;
            hand.gestureScore = rawHand.gestureScore;
            for (usize landmark = 0U; landmark < HandTrackingHand::LANDMARK_COUNT; ++landmark) {
                hand.imageLandmarks[landmark] = HandTrackingLandmark{
                    .x = rawHand.image[landmark].x,
                    .y = rawHand.image[landmark].y,
                    .z = rawHand.image[landmark].z,
                };
                hand.worldLandmarks[landmark] = HandTrackingLandmark{
                    .x = rawHand.world[landmark].x,
                    .y = rawHand.world[landmark].y,
                    .z = rawHand.world[landmark].z,
                };
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

        const std::string args =
            quotedUtf8(python) + " " +
            quotedUtf8(script) +
            " --model " + quotedUtf8(model) +
            " --config " + quotedUtf8(config) +
            " --udp-host 127.0.0.1" +
            " --udp-port " + std::to_string(UDP_PORT) +
            " --control-port " + std::to_string(CONTROL_PORT) +
            " --preview-port " + std::to_string(PREVIEW_PORT) +
            " --parent-pid " + std::to_string(currentProcessId());

#ifdef _WIN32
        std::wstring command(args.begin(), args.end());
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

    [[nodiscard]] HandTrackingFrame smoothFrame(const HandTrackingFrame& raw) const {
        std::optional<HandTrackingFrame> previous;
        {
            std::scoped_lock lock(m_mutex);
            previous = m_latestFrame;
        }
        if (!previous) {
            return raw;
        }

        constexpr f32 alpha = 0.45f;
        HandTrackingFrame result = raw;
        for (auto& hand : result.hands) {
            if (!hand.valid) {
                continue;
            }
            const auto previousIt = std::find_if(previous->hands.begin(), previous->hands.end(),
                [&hand](const HandTrackingHand& candidate) {
                    return candidate.valid && candidate.handedness == hand.handedness;
                });
            if (previousIt == previous->hands.end()) {
                continue;
            }
            for (usize index = 0U; index < HandTrackingHand::LANDMARK_COUNT; ++index) {
                hand.imageLandmarks[index] = lerpLandmark(previousIt->imageLandmarks[index], hand.imageLandmarks[index], alpha);
                hand.worldLandmarks[index] = lerpLandmark(previousIt->worldLandmarks[index], hand.worldLandmarks[index], alpha);
            }
        }
        return result;
    }

    void updateGestures(const HandTrackingFrame& frame) {
        const auto now = std::chrono::steady_clock::now();
        for (const auto& hand : frame.hands) {
            if (!hand.valid) {
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
    std::optional<HandTrackingPreviewFrame> m_latestPreview{};
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

std::optional<HandTrackingPreviewFrame> HandTrackingService::latestPreviewFrame() const {
    return m_impl->latestPreviewFrame();
}

} // namespace biofuel::engine::vision::hand_tracking
