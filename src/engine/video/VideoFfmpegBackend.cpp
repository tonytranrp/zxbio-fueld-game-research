#include "VideoManager.hpp"
#include "VideoBackend.hpp"
#include "VideoBufferPolicy.hpp"
#include "engine/debug/MemoryTelemetry.hpp"
#include "engine/runtime/typed/Events.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <mutex>
#include <spdlog/spdlog.h>
#include <thread>
#include <vector>

#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #define CloseWindow Win32CloseWindow
    #define ShowCursor Win32ShowCursor
    #include <Windows.h>
    #undef CloseWindow
    #undef ShowCursor
#else
    #include <cerrno>
    #include <csignal>
    #include <sys/types.h>
    #include <sys/wait.h>
    #include <unistd.h>
#endif

namespace biofuel::engine::video {

namespace {

constexpr i32 VIDEO_WIDTH = 1280;
constexpr i32 VIDEO_HEIGHT = 720;
constexpr i32 VIDEO_FPS = 30;
constexpr i32 AUDIO_SAMPLE_RATE = 44100;
constexpr i32 AUDIO_CHANNELS = 2;
constexpr i32 AUDIO_BITS = 16;
constexpr i32 AUDIO_FRAMES_PER_CHUNK = 4096;
constexpr auto IDLE_VIDEO_BUFFER_POLICY =
    VideoBufferPolicy<::biofuel::engine::runtime::typed::video::IdleAmbient>::value;
constexpr std::size_t MAX_VIDEO_FRAMES = IDLE_VIDEO_BUFFER_POLICY.maxVideoFrames;
constexpr std::size_t MAX_AUDIO_CHUNKS = IDLE_VIDEO_BUFFER_POLICY.maxAudioChunks;
constexpr std::size_t MIN_VIDEO_PREFILL_FRAMES = IDLE_VIDEO_BUFFER_POLICY.minVideoPrefillFrames;
constexpr std::size_t MIN_AUDIO_PREFILL_CHUNKS = IDLE_VIDEO_BUFFER_POLICY.minAudioPrefillChunks;
constexpr i32 MAX_AUDIO_PUMPS_PER_UPDATE = 4;
constexpr f64 PREFILL_TIMEOUT_SECONDS = 1.0;


#ifdef _WIN32

std::wstring utf8ToWide(const std::string_view text) {
    if (text.empty()) {
        return {};
    }

    const int wideLen = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        text.data(),
        static_cast<int>(text.size()),
        nullptr,
        0);
    if (wideLen > 0) {
        std::wstring wide(static_cast<std::size_t>(wideLen), L'\0');
        MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            text.data(),
            static_cast<int>(text.size()),
            wide.data(),
            wideLen);
        return wide;
    }

    const int fallbackLen = MultiByteToWideChar(
        CP_ACP,
        0,
        text.data(),
        static_cast<int>(text.size()),
        nullptr,
        0);
    if (fallbackLen <= 0) {
        return {};
    }

    std::wstring wide(static_cast<std::size_t>(fallbackLen), L'\0');
    MultiByteToWideChar(
        CP_ACP,
        0,
        text.data(),
        static_cast<int>(text.size()),
        wide.data(),
        fallbackLen);
    return wide;
}

std::wstring quoteArg(const std::wstring& arg) {
    std::wstring quoted = L"\"";
    for (const wchar_t ch : arg) {
        if (ch == L'"') {
            quoted += L"\\\"";
        } else {
            quoted += ch;
        }
    }
    quoted += L"\"";
    return quoted;
}

std::wstring findFfmpegExecutable() {
#ifdef BIOFUEL_FFMPEG_EXECUTABLE
    std::wstring configured = utf8ToWide(BIOFUEL_FFMPEG_EXECUTABLE);
    if (!configured.empty() && GetFileAttributesW(configured.c_str()) != INVALID_FILE_ATTRIBUTES) {
        return configured;
    }
#endif

    std::array<wchar_t, MAX_PATH> found{};
    if (SearchPathW(nullptr, L"ffmpeg.exe", nullptr, MAX_PATH, found.data(), nullptr) > 0) {
        return found.data();
    }
    return {};
}

class PipeProcess final {
public:
    PipeProcess() = default;
    ~PipeProcess() noexcept {
        stop();
    }

    PipeProcess(const PipeProcess&) = delete;
    PipeProcess& operator=(const PipeProcess&) = delete;
    PipeProcess(PipeProcess&&) = delete;
    PipeProcess& operator=(PipeProcess&&) = delete;

    bool start(const std::wstring& commandLine, std::string& error) noexcept {
        stop();

        SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;

        HANDLE readPipe = nullptr;
        HANDLE writePipe = nullptr;
        if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) {
            error = "CreatePipe failed";
            return false;
        }

        SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        startup.hStdOutput = writePipe;
        startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);

        std::wstring mutableCommand = commandLine;
        if (!CreateProcessW(
                nullptr,
                mutableCommand.data(),
                nullptr,
                nullptr,
                TRUE,
                CREATE_NO_WINDOW,
                nullptr,
                nullptr,
                &startup,
                &m_process)) {
            CloseHandle(readPipe);
            CloseHandle(writePipe);
            error = "CreateProcessW(ffmpeg) failed";
            return false;
        }

        CloseHandle(writePipe);
        m_stdout = readPipe;
        return true;
    }

    bool readExact(unsigned char* dst, const std::size_t bytes, std::atomic<bool>& stopFlag) noexcept {
        std::size_t total = 0;
        while (!stopFlag.load(std::memory_order_relaxed) && total < bytes) {
            DWORD read = 0;
            const DWORD want = static_cast<DWORD>(std::min<std::size_t>(bytes - total, 64U * 1024U));
            if (!ReadFile(m_stdout, dst + total, want, &read, nullptr) || read == 0) {
                return false;
            }
            total += read;
        }
        return total == bytes;
    }

    void stop() noexcept {
        if (m_process.hProcess) {
            TerminateProcess(m_process.hProcess, 0);
            WaitForSingleObject(m_process.hProcess, 1000);
        }
        if (m_stdout) {
            CloseHandle(m_stdout);
            m_stdout = nullptr;
        }
        if (m_process.hThread) {
            CloseHandle(m_process.hThread);
            m_process.hThread = nullptr;
        }
        if (m_process.hProcess) {
            CloseHandle(m_process.hProcess);
            m_process.hProcess = nullptr;
        }
        m_process.dwProcessId = 0;
        m_process.dwThreadId = 0;
    }

private:
    PROCESS_INFORMATION m_process{};
    HANDLE m_stdout = nullptr;
};

#else

std::string quoteArg(const std::string& arg) {
    std::string quoted = "'";
    for (const char ch : arg) {
        if (ch == '\'') {
            quoted += "'\\''";
        } else {
            quoted.push_back(ch);
        }
    }
    quoted.push_back('\'');
    return quoted;
}

[[nodiscard]] bool executableExists(const std::string& path) noexcept {
    return !path.empty() && access(path.c_str(), X_OK) == 0;
}

std::string findOnPath(const char* executable) {
    const char* pathEnv = std::getenv("PATH");
    if (pathEnv == nullptr) {
        return {};
    }

    std::string path{pathEnv};
    std::size_t start = 0U;
    while (start <= path.size()) {
        const std::size_t end = path.find(':', start);
        const std::string dir = path.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (!dir.empty()) {
            const std::string candidate = dir + "/" + executable;
            if (executableExists(candidate)) {
                return candidate;
            }
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1U;
    }
    return {};
}

std::string findFfmpegExecutable() {
#ifdef BIOFUEL_FFMPEG_EXECUTABLE
    const std::string configured = BIOFUEL_FFMPEG_EXECUTABLE;
    if (executableExists(configured)) {
        return configured;
    }
#endif
    return findOnPath("ffmpeg");
}

class PipeProcess final {
public:
    PipeProcess() = default;
    ~PipeProcess() noexcept {
        stop();
    }

    PipeProcess(const PipeProcess&) = delete;
    PipeProcess& operator=(const PipeProcess&) = delete;
    PipeProcess(PipeProcess&&) = delete;
    PipeProcess& operator=(PipeProcess&&) = delete;

    bool start(const std::string& commandLine, std::string& error) noexcept {
        stop();

        int pipefd[2]{-1, -1};
        if (pipe(pipefd) != 0) {
            error = "pipe failed while starting ffmpeg";
            return false;
        }

        const pid_t pid = fork();
        if (pid < 0) {
            close(pipefd[0]);
            close(pipefd[1]);
            error = "fork failed while starting ffmpeg";
            return false;
        }

        if (pid == 0) {
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[1]);
            execl("/bin/sh", "sh", "-c", commandLine.c_str(), static_cast<char*>(nullptr));
            _exit(127);
        }

        close(pipefd[1]);
        m_stdout = pipefd[0];
        m_pid = pid;
        return true;
    }

    bool readExact(unsigned char* dst, const std::size_t bytes, std::atomic<bool>& stopFlag) noexcept {
        std::size_t total = 0U;
        while (!stopFlag.load(std::memory_order_relaxed) && total < bytes) {
            const std::size_t want = std::min<std::size_t>(bytes - total, 64U * 1024U);
            const ssize_t readBytes = read(m_stdout, dst + total, want);
            if (readBytes > 0) {
                total += static_cast<std::size_t>(readBytes);
                continue;
            }
            if (readBytes < 0 && errno == EINTR) {
                continue;
            }
            return false;
        }
        return total == bytes;
    }

    void stop() noexcept {
        if (m_pid > 0) {
            int status = 0;
            const pid_t result = waitpid(m_pid, &status, WNOHANG);
            if (result == 0) {
                kill(m_pid, SIGTERM);
                for (int attempt = 0; attempt < 10; ++attempt) {
                    if (waitpid(m_pid, &status, WNOHANG) == m_pid) {
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(25));
                }
                if (waitpid(m_pid, &status, WNOHANG) == 0) {
                    kill(m_pid, SIGKILL);
                    (void)waitpid(m_pid, &status, 0);
                }
            }
            m_pid = -1;
        }
        if (m_stdout >= 0) {
            close(m_stdout);
            m_stdout = -1;
        }
    }

private:
    pid_t m_pid = -1;
    int m_stdout = -1;
};

#endif

#ifdef _WIN32
using NativePath = std::wstring;
#else
using NativePath = std::string;
#endif

NativePath utf8ToNativePath(const std::string_view value) {
#ifdef _WIN32
    return utf8ToWide(value);
#else
    return std::string{value};
#endif
}

bool fileExists(const NativePath& path) noexcept {
#ifdef _WIN32
    return !path.empty() && GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
#else
    return !path.empty() && access(path.c_str(), R_OK) == 0;
#endif
}

void sleepForMilliseconds(const int milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

} // namespace


class FfmpegProcessBackend final : public VideoManager::Backend {
public:
    ~FfmpegProcessBackend() override {
        unload();
    }

    bool load(std::string_view path, std::string& error) override {
        m_ffmpegPath = findFfmpegExecutable();
        if (m_ffmpegPath.empty()) {
            error = "ffmpeg was not found on PATH";
            return false;
        }

        m_path = utf8ToNativePath(path);
        if (m_path.empty()) {
            error = "Video path is empty or could not be converted for the native platform";
            return false;
        }

        if (!fileExists(m_path)) {
            error = "Video file was not found";
            return false;
        }

        Image blank = GenImageColor(VIDEO_WIDTH, VIDEO_HEIGHT, BLACK);
        m_texture = LoadTextureFromImage(blank);
        UnloadImage(blank);
        if (m_texture.id == 0) {
            error = "Failed to allocate video texture";
            return false;
        }
        SetTextureFilter(m_texture, TEXTURE_FILTER_BILINEAR);

        if (!IsAudioDeviceReady()) {
            error = "Raylib audio device is not ready";
            unload();
            return false;
        }
        SetAudioStreamBufferSizeDefault(AUDIO_FRAMES_PER_CHUNK);
        m_audio = LoadAudioStream(AUDIO_SAMPLE_RATE, AUDIO_BITS, AUDIO_CHANNELS);
        if (!m_audio.buffer) {
            error = "Failed to create video audio stream";
            unload();
            return false;
        }
        SetAudioStreamVolume(m_audio, m_volume);
        m_audioLoaded = true;
        m_audioScratch.resize(static_cast<std::size_t>(AUDIO_FRAMES_PER_CHUNK) * AUDIO_CHANNELS * sizeof(i16));
        return true;
    }

    void unload() noexcept override {
        stop();
        if (m_audioLoaded) {
            UnloadAudioStream(m_audio);
            m_audio = {};
            m_audioLoaded = false;
        }
        if (m_texture.id != 0) {
            UnloadTexture(m_texture);
            m_texture = {};
        }
    }

    bool play(const bool looping, const f32 volume, std::string& error) override {
        stop();
        m_looping.store(looping, std::memory_order_relaxed);
        setVolume(volume);
        m_stop.store(false, std::memory_order_relaxed);
        m_completed.store(false, std::memory_order_relaxed);
        m_playing.store(true, std::memory_order_relaxed);
        m_paused.store(false, std::memory_order_relaxed);
        m_audioDecodeEnded.store(false, std::memory_order_relaxed);
        m_audioHadData.store(false, std::memory_order_relaxed);

        if (!m_videoProcess.start(videoCommand(looping), error)) {
            m_playing.store(false, std::memory_order_relaxed);
            return false;
        }
        if (!m_audioProcess.start(audioCommand(looping), error)) {
            spdlog::warn("VideoManager: audio decode unavailable, playing video silently: {}", error);
            m_audioDecodeEnded.store(true, std::memory_order_relaxed);
        }

        m_videoThread = std::thread([this] { videoReaderLoop(); });
        if (!m_audioDecodeEnded.load(std::memory_order_relaxed)) {
            m_audioThread = std::thread([this] { audioReaderLoop(); });
        }
        if (!waitForPrebuffer()) {
            error = "Timed out waiting for decoded video data";
            stop();
            return false;
        }

        if (!uploadNextVideoFrame()) {
            error = "Decoded video prebuffer was empty";
            stop();
            return false;
        }
        m_nextFrameTime = GetTime() + 1.0 / static_cast<f64>(VIDEO_FPS);
        pumpAudio();
        if (m_audioHadData.load(std::memory_order_relaxed)) {
            PlayAudioStream(m_audio);
        }
        return true;
    }

    void stop() noexcept override {
        m_stop.store(true, std::memory_order_relaxed);
        m_playing.store(false, std::memory_order_relaxed);
        m_paused.store(false, std::memory_order_relaxed);
        m_audioDecodeEnded.store(true, std::memory_order_relaxed);
        m_videoProcess.stop();
        m_audioProcess.stop();
        if (m_videoThread.joinable()) {
            m_videoThread.join();
        }
        if (m_audioThread.joinable()) {
            m_audioThread.join();
        }
        if (m_audioLoaded) {
            StopAudioStream(m_audio);
        }
        {
            std::scoped_lock lock{m_mutex};
            m_videoFrames.clear();
            m_audioChunks.clear();
            updateTelemetryLocked();
        }
    }

    void pause() noexcept override {
        m_paused.store(true, std::memory_order_relaxed);
        if (m_audioLoaded) {
            PauseAudioStream(m_audio);
        }
    }

    void resume() noexcept override {
        m_paused.store(false, std::memory_order_relaxed);
        m_nextFrameTime = GetTime() + 1.0 / static_cast<f64>(VIDEO_FPS);
        if (m_audioLoaded) {
            ResumeAudioStream(m_audio);
        }
    }

    VideoUpdate update() override {
        VideoUpdate result{};
        if (!m_playing.load(std::memory_order_relaxed)) {
            return result;
        }

        if (!m_paused.load(std::memory_order_relaxed)) {
            const f64 now = GetTime();
            std::vector<unsigned char> frame;
            while (now + 0.001 >= m_nextFrameTime) {
                std::vector<unsigned char> nextFrame;
                if (!popVideoFrame(nextFrame)) {
                    m_nextFrameTime = now + 1.0 / static_cast<f64>(VIDEO_FPS);
                    break;
                }
                frame = std::move(nextFrame);
                m_nextFrameTime += 1.0 / static_cast<f64>(VIDEO_FPS);
            }
            if (!frame.empty()) {
                UpdateTexture(m_texture, frame.data());
            }

            pumpAudio();
        }

        if (m_completed.exchange(false, std::memory_order_relaxed)) {
            result.completed = true;
            m_playing.store(false, std::memory_order_relaxed);
        }
        return result;
    }

    void setLooping(const bool looping) noexcept override {
        m_looping.store(looping, std::memory_order_relaxed);
    }

    void setVolume(const f32 volume) noexcept override {
        m_volume = std::clamp(volume, 0.0f, 1.0f);
        if (m_audioLoaded) {
            SetAudioStreamVolume(m_audio, m_volume);
        }
    }

    [[nodiscard]] Texture2D texture() const noexcept override {
        return m_texture;
    }

private:
    [[nodiscard]] NativePath loopArg(const bool looping) const {
#ifdef _WIN32
        return looping ? L" -stream_loop -1" : L"";
#else
        return looping ? " -stream_loop -1" : "";
#endif
    }

    [[nodiscard]] NativePath videoCommand(const bool looping) const {
#ifdef _WIN32
        return quoteArg(m_ffmpegPath) + L" -nostdin -hide_banner -loglevel quiet" +
            loopArg(looping) +
            L" -i " + quoteArg(m_path) +
            L" -an -vf \"scale=1280:720:force_original_aspect_ratio=increase,crop=1280:720,fps=30\""
            L" -f rawvideo -pix_fmt rgba pipe:1";
#else
        return quoteArg(m_ffmpegPath) + " -nostdin -hide_banner -loglevel quiet" +
            loopArg(looping) +
            " -i " + quoteArg(m_path) +
            " -an -vf \"scale=1280:720:force_original_aspect_ratio=increase,crop=1280:720,fps=30\""
            " -f rawvideo -pix_fmt rgba pipe:1";
#endif
    }

    [[nodiscard]] NativePath audioCommand(const bool looping) const {
#ifdef _WIN32
        return quoteArg(m_ffmpegPath) + L" -nostdin -hide_banner -loglevel quiet" +
            loopArg(looping) +
            L" -i " + quoteArg(m_path) +
            L" -vn -f s16le -acodec pcm_s16le -ar 44100 -ac 2 pipe:1";
#else
        return quoteArg(m_ffmpegPath) + " -nostdin -hide_banner -loglevel quiet" +
            loopArg(looping) +
            " -i " + quoteArg(m_path) +
            " -vn -f s16le -acodec pcm_s16le -ar 44100 -ac 2 pipe:1";
#endif
    }

    void videoReaderLoop() {
        while (!m_stop.load(std::memory_order_relaxed)) {
            while (m_paused.load(std::memory_order_relaxed) && !m_stop.load(std::memory_order_relaxed)) {
                sleepForMilliseconds(4);
            }
            while (!m_stop.load(std::memory_order_relaxed)) {
                {
                    std::scoped_lock lock{m_mutex};
                    if (m_videoFrames.size() < MAX_VIDEO_FRAMES) {
                        break;
                    }
                }
                sleepForMilliseconds(1);
            }
            std::vector<unsigned char> frame(FRAME_BYTES);
            if (!m_videoProcess.readExact(frame.data(), FRAME_BYTES, m_stop)) {
                break;
            }
            std::scoped_lock lock{m_mutex};
            m_videoFrames.push_back(std::move(frame));
            updateTelemetryLocked();
        }

        if (!m_looping.load(std::memory_order_relaxed) && !m_stop.load(std::memory_order_relaxed)) {
            m_completed.store(true, std::memory_order_relaxed);
        }
    }

    void audioReaderLoop() {
        while (!m_stop.load(std::memory_order_relaxed)) {
            while (m_paused.load(std::memory_order_relaxed) && !m_stop.load(std::memory_order_relaxed)) {
                sleepForMilliseconds(4);
            }
            while (!m_stop.load(std::memory_order_relaxed)) {
                {
                    std::scoped_lock lock{m_mutex};
                    if (m_audioChunks.size() < MAX_AUDIO_CHUNKS) {
                        break;
                    }
                }
                sleepForMilliseconds(1);
            }
            std::vector<unsigned char> chunk(m_audioScratch.size());
            if (!m_audioProcess.readExact(chunk.data(), chunk.size(), m_stop)) {
                break;
            }
            m_audioHadData.store(true, std::memory_order_relaxed);
            std::scoped_lock lock{m_mutex};
            m_audioChunks.push_back(std::move(chunk));
            updateTelemetryLocked();
        }
        m_audioDecodeEnded.store(true, std::memory_order_relaxed);
    }

    [[nodiscard]] bool popVideoFrame(std::vector<unsigned char>& frame) {
        std::scoped_lock lock{m_mutex};
        if (m_videoFrames.empty()) {
            return false;
        }

        frame = std::move(m_videoFrames.front());
        m_videoFrames.pop_front();
        updateTelemetryLocked();
        return true;
    }

    [[nodiscard]] bool uploadNextVideoFrame() {
        std::vector<unsigned char> frame;
        if (!popVideoFrame(frame)) {
            return false;
        }

        UpdateTexture(m_texture, frame.data());
        return true;
    }

    void pumpAudio() {
        if (!m_audioLoaded) {
            return;
        }

        for (i32 pumped = 0; pumped < MAX_AUDIO_PUMPS_PER_UPDATE && IsAudioStreamProcessed(m_audio); ++pumped) {
            if (!copyNextAudioChunk()) {
                break;
            }
            UpdateAudioStream(m_audio, m_audioScratch.data(), AUDIO_FRAMES_PER_CHUNK);
        }
    }

    [[nodiscard]] bool copyNextAudioChunk() {
        std::scoped_lock lock{m_mutex};
        if (m_audioChunks.empty()) {
            return false;
        }

        const auto& chunk = m_audioChunks.front();
        std::memcpy(m_audioScratch.data(), chunk.data(), m_audioScratch.size());
        m_audioChunks.pop_front();
        updateTelemetryLocked();
        return true;
    }

    [[nodiscard]] bool waitForPrebuffer() const {
        const f64 deadline = GetTime() + PREFILL_TIMEOUT_SECONDS;
        while (!m_stop.load(std::memory_order_relaxed) && GetTime() < deadline) {
            {
                std::scoped_lock lock{m_mutex};
                if (m_videoFrames.size() >= MIN_VIDEO_PREFILL_FRAMES &&
                    (m_audioChunks.size() >= MIN_AUDIO_PREFILL_CHUNKS ||
                        m_audioDecodeEnded.load(std::memory_order_relaxed))) {
                    return true;
                }
            }
            sleepForMilliseconds(1);
        }
        return false;
    }

    NativePath m_path;
    NativePath m_ffmpegPath;
    Texture2D m_texture{};
    AudioStream m_audio{};
    bool m_audioLoaded = false;
    f32 m_volume = 1.0f;
    std::atomic_bool m_looping = false;

    PipeProcess m_videoProcess;
    PipeProcess m_audioProcess;
    std::thread m_videoThread;
    std::thread m_audioThread;
    std::atomic<bool> m_stop{false};
    std::atomic<bool> m_playing{false};
    std::atomic<bool> m_paused{false};
    std::atomic<bool> m_completed{false};
    std::atomic<bool> m_audioDecodeEnded{false};
    std::atomic<bool> m_audioHadData{false};

    mutable std::mutex m_mutex;
    static constexpr std::size_t FRAME_BYTES =
        static_cast<std::size_t>(VIDEO_WIDTH) * static_cast<std::size_t>(VIDEO_HEIGHT) * 4U;
    std::deque<std::vector<unsigned char>> m_videoFrames;
    std::vector<unsigned char> m_audioScratch;
    std::deque<std::vector<unsigned char>> m_audioChunks;
    f64 m_nextFrameTime = 0.0;

    void updateTelemetryLocked() const noexcept {
        ::biofuel::engine::debug::MemoryTelemetry::set(
            ::biofuel::engine::debug::ResourceKind::VideoFrameQueue,
            static_cast<i64>(m_videoFrames.size()),
            static_cast<i64>(m_videoFrames.size() * FRAME_BYTES));
        ::biofuel::engine::debug::MemoryTelemetry::set(
            ::biofuel::engine::debug::ResourceKind::AudioChunkQueue,
            static_cast<i64>(m_audioChunks.size()),
            static_cast<i64>(m_audioChunks.size() * m_audioScratch.size()));
    }
};

std::unique_ptr<VideoManager::Backend> makeBackend() {
    return std::make_unique<FfmpegProcessBackend>();
}

} // namespace biofuel::engine::video

