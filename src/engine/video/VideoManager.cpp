#include "VideoManager.hpp"
#include "engine/runtime/typed/Events.hpp"
#include <algorithm>
#include <atomic>
#include <cctype>
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
constexpr std::size_t MAX_VIDEO_FRAMES = 8;
constexpr std::size_t MAX_AUDIO_CHUNKS = 32;
constexpr std::size_t MIN_VIDEO_PREFILL_FRAMES = 4;
constexpr std::size_t MIN_AUDIO_PREFILL_CHUNKS = 4;
constexpr i32 MAX_AUDIO_PUMPS_PER_UPDATE = 4;
constexpr f64 PREFILL_TIMEOUT_SECONDS = 1.0;

struct VideoUpdate {
    bool completed = false;
    bool error = false;
    std::string errorMessage;
};

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

    wchar_t found[MAX_PATH]{};
    if (SearchPathW(nullptr, L"ffmpeg.exe", nullptr, MAX_PATH, found, nullptr) > 0) {
        return found;
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

#endif

} // namespace

struct VideoManager::Backend {
    virtual ~Backend() = default;
    virtual bool load(std::string_view path, std::string& error) = 0;
    virtual void unload() noexcept = 0;
    virtual bool play(bool looping, f32 volume, std::string& error) = 0;
    virtual void stop() noexcept = 0;
    virtual void pause() noexcept = 0;
    virtual void resume() noexcept = 0;
    virtual VideoUpdate update() = 0;
    virtual void setLooping(bool looping) noexcept = 0;
    virtual void setVolume(f32 volume) noexcept = 0;
    [[nodiscard]] virtual Texture2D texture() const noexcept = 0;
};

#ifdef _WIN32

class FfmpegProcessBackend final : public VideoManager::Backend {
public:
    ~FfmpegProcessBackend() override {
        unload();
    }

    bool load(std::string_view path, std::string& error) override {
        m_ffmpegPath = findFfmpegExecutable();
        if (m_ffmpegPath.empty()) {
            error = "ffmpeg.exe was not found on PATH";
            return false;
        }

        m_path = utf8ToWide(path);
        if (m_path.empty()) {
            error = "Video path is empty or could not be converted to UTF-16";
            return false;
        }

        if (GetFileAttributesW(m_path.c_str()) == INVALID_FILE_ATTRIBUTES) {
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
            return false;
        }
        SetAudioStreamBufferSizeDefault(AUDIO_FRAMES_PER_CHUNK);
        m_audio = LoadAudioStream(AUDIO_SAMPLE_RATE, AUDIO_BITS, AUDIO_CHANNELS);
        if (!m_audio.buffer) {
            error = "Failed to create video audio stream";
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
        m_looping = looping;
        setVolume(volume);
        m_stop.store(false, std::memory_order_relaxed);
        m_completed.store(false, std::memory_order_relaxed);
        m_playing.store(true, std::memory_order_relaxed);
        m_paused.store(false, std::memory_order_relaxed);

        if (!m_videoProcess.start(videoCommand(looping), error)) {
            m_playing.store(false, std::memory_order_relaxed);
            return false;
        }
        if (!m_audioProcess.start(audioCommand(looping), error)) {
            m_playing.store(false, std::memory_order_relaxed);
            m_videoProcess.stop();
            return false;
        }

        m_videoThread = std::thread([this] { videoReaderLoop(); });
        m_audioThread = std::thread([this] { audioReaderLoop(); });
        if (!waitForPrebuffer()) {
            error = "Timed out waiting for decoded video/audio data";
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
        PlayAudioStream(m_audio);
        return true;
    }

    void stop() noexcept override {
        m_stop.store(true, std::memory_order_relaxed);
        m_playing.store(false, std::memory_order_relaxed);
        m_paused.store(false, std::memory_order_relaxed);
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
        m_looping = looping;
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
    [[nodiscard]] std::wstring loopArg(const bool looping) const {
        return looping ? L" -stream_loop -1" : L"";
    }

    [[nodiscard]] std::wstring videoCommand(const bool looping) const {
        return quoteArg(m_ffmpegPath) + L" -nostdin -hide_banner -loglevel quiet" +
            loopArg(looping) +
            L" -i " + quoteArg(m_path) +
            L" -an -vf \"scale=1280:720:force_original_aspect_ratio=increase,crop=1280:720,fps=30\""
            L" -f rawvideo -pix_fmt rgba pipe:1";
    }

    [[nodiscard]] std::wstring audioCommand(const bool looping) const {
        return quoteArg(m_ffmpegPath) + L" -nostdin -hide_banner -loglevel quiet" +
            loopArg(looping) +
            L" -i " + quoteArg(m_path) +
            L" -vn -f s16le -acodec pcm_s16le -ar 44100 -ac 2 pipe:1";
    }

    void videoReaderLoop() {
        const std::size_t frameBytes =
            static_cast<std::size_t>(VIDEO_WIDTH) * static_cast<std::size_t>(VIDEO_HEIGHT) * 4U;

        while (!m_stop.load(std::memory_order_relaxed)) {
            while (!m_stop.load(std::memory_order_relaxed)) {
                {
                    std::scoped_lock lock{m_mutex};
                    if (m_videoFrames.size() < MAX_VIDEO_FRAMES) {
                        break;
                    }
                }
                Sleep(1);
            }
            std::vector<unsigned char> frame(frameBytes);
            if (!m_videoProcess.readExact(frame.data(), frameBytes, m_stop)) {
                break;
            }
            if (m_paused.load(std::memory_order_relaxed)) {
                continue;
            }
            std::scoped_lock lock{m_mutex};
            m_videoFrames.push_back(std::move(frame));
        }

        if (!m_looping && !m_stop.load(std::memory_order_relaxed)) {
            m_completed.store(true, std::memory_order_relaxed);
        }
    }

    void audioReaderLoop() {
        while (!m_stop.load(std::memory_order_relaxed)) {
            while (!m_stop.load(std::memory_order_relaxed)) {
                {
                    std::scoped_lock lock{m_mutex};
                    if (m_audioChunks.size() < MAX_AUDIO_CHUNKS) {
                        break;
                    }
                }
                Sleep(1);
            }
            std::vector<unsigned char> chunk(m_audioScratch.size());
            if (!m_audioProcess.readExact(chunk.data(), chunk.size(), m_stop)) {
                break;
            }
            std::scoped_lock lock{m_mutex};
            m_audioChunks.push_back(std::move(chunk));
        }
    }

    [[nodiscard]] bool popVideoFrame(std::vector<unsigned char>& frame) {
        std::scoped_lock lock{m_mutex};
        if (m_videoFrames.empty()) {
            return false;
        }

        frame = std::move(m_videoFrames.front());
        m_videoFrames.pop_front();
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
        return true;
    }

    [[nodiscard]] bool waitForPrebuffer() const {
        const f64 deadline = GetTime() + PREFILL_TIMEOUT_SECONDS;
        while (!m_stop.load(std::memory_order_relaxed) && GetTime() < deadline) {
            {
                std::scoped_lock lock{m_mutex};
                if (m_videoFrames.size() >= MIN_VIDEO_PREFILL_FRAMES &&
                    m_audioChunks.size() >= MIN_AUDIO_PREFILL_CHUNKS) {
                    return true;
                }
            }
            Sleep(1);
        }
        return false;
    }

    std::wstring m_path;
    std::wstring m_ffmpegPath;
    Texture2D m_texture{};
    AudioStream m_audio{};
    bool m_audioLoaded = false;
    f32 m_volume = 1.0f;
    bool m_looping = false;

    PipeProcess m_videoProcess;
    PipeProcess m_audioProcess;
    std::thread m_videoThread;
    std::thread m_audioThread;
    std::atomic<bool> m_stop{false};
    std::atomic<bool> m_playing{false};
    std::atomic<bool> m_paused{false};
    std::atomic<bool> m_completed{false};

    mutable std::mutex m_mutex;
    std::deque<std::vector<unsigned char>> m_videoFrames;
    std::vector<unsigned char> m_audioScratch;
    std::deque<std::vector<unsigned char>> m_audioChunks;
    f64 m_nextFrameTime = 0.0;
};

#else

class NullVideoBackend final : public VideoManager::Backend {
public:
    bool load(std::string_view, std::string& error) override {
        error = "FFmpeg video playback is currently implemented only on Windows";
        return false;
    }
    void unload() noexcept override {}
    bool play(bool, f32, std::string& error) override {
        error = "No native video backend is available on this platform";
        return false;
    }
    void stop() noexcept override {}
    void pause() noexcept override {}
    void resume() noexcept override {}
    VideoUpdate update() override { return {}; }
    void setLooping(bool) noexcept override {}
    void setVolume(f32) noexcept override {}
    [[nodiscard]] Texture2D texture() const noexcept override { return {}; }
};

#endif

namespace {

std::unique_ptr<VideoManager::Backend> makeBackend() {
#ifdef _WIN32
    return std::make_unique<FfmpegProcessBackend>();
#else
    return std::make_unique<NullVideoBackend>();
#endif
}

} // namespace

VideoManager& VideoManager::instance() noexcept {
    static VideoManager mgr;
    return mgr;
}

VideoManager::~VideoManager() noexcept {
    shutdown();
}

void VideoManager::init() {
    m_initialized = true;
}

void VideoManager::shutdown() noexcept {
    unloadAllVideos();
    m_initialized = false;
}

void VideoManager::update() {
    if (!m_initialized) {
        return;
    }

    for (auto& [_, inst] : m_videos) {
        if (!inst.loaded || inst.error || !inst.backend) {
            continue;
        }

        const VideoUpdate result = inst.backend->update();
        if (result.error) {
            setError(inst, result.errorMessage);
            continue;
        }
        if (result.completed) {
            ::biofuel::engine::runtime::typed::Events::publish<::biofuel::engine::runtime::typed::video::Completed>({
                .videoName = inst.name
            });
            if (!inst.looping) {
                inst.playing = false;
                inst.ended = true;
            }
        }
    }
}

void VideoManager::loadVideo(std::string_view name, std::string_view path) {
    if (!m_initialized) {
        init();
    }

    const std::string key{name};
    unloadVideo(key);

    VideoInstance inst;
    inst.name = key;
    inst.path = std::string{path};
    inst.backend = makeBackend();

    std::string error;
    if (!inst.backend || !inst.backend->load(inst.path, error)) {
        inst.loaded = false;
        inst.error = true;
        inst.errorMessage = error.empty() ? "Video backend failed to load the file" : std::move(error);
        spdlog::error("VideoManager: failed to load '{}': {}", inst.path, inst.errorMessage);
        m_videos.emplace(key, std::move(inst));
        ::biofuel::engine::runtime::typed::Events::publish<::biofuel::engine::runtime::typed::video::Error>({
            .videoName = m_videos.at(key).name,
            .errorMessage = m_videos.at(key).errorMessage
        });
        return;
    }

    inst.loaded = true;
    spdlog::info("VideoManager: loaded '{}'", inst.path);
    m_videos.emplace(key, std::move(inst));
}

void VideoManager::unloadVideo(std::string_view name) {
    const auto it = m_videos.find(name);
    if (it == m_videos.end()) {
        return;
    }
    if (it->second.backend) {
        it->second.backend->unload();
    }
    m_videos.erase(it);
}

void VideoManager::unloadAllVideos() noexcept {
    for (auto& [_, inst] : m_videos) {
        if (inst.backend) {
            inst.backend->unload();
        }
    }
    m_videos.clear();
}

void VideoManager::play(std::string_view name) {
    auto* inst = findVideo(name);
    if (!inst || !inst->loaded || inst->error || !inst->backend) {
        return;
    }

    std::string error;
    if (!inst->backend->play(inst->looping, inst->volume, error)) {
        setError(*inst, error);
        return;
    }

    inst->playing = true;
    inst->paused = false;
    inst->ended = false;
    ::biofuel::engine::runtime::typed::Events::publish<::biofuel::engine::runtime::typed::video::Started>({
        .videoName = inst->name
    });
}

void VideoManager::stop(std::string_view name) noexcept {
    auto* inst = findVideo(name);
    if (!inst || !inst->backend) {
        return;
    }
    inst->backend->stop();
    inst->playing = false;
    inst->paused = false;
}

void VideoManager::pause(std::string_view name) noexcept {
    auto* inst = findVideo(name);
    if (!inst || !inst->backend || !inst->playing) {
        return;
    }
    inst->backend->pause();
    inst->paused = true;
}

void VideoManager::resume(std::string_view name) noexcept {
    auto* inst = findVideo(name);
    if (!inst || !inst->backend || !inst->playing) {
        return;
    }
    inst->backend->resume();
    inst->paused = false;
}

bool VideoManager::hasVideo(std::string_view name) const noexcept {
    const auto* inst = findVideo(name);
    return inst && inst->loaded && !inst->error;
}

bool VideoManager::isPlaying(std::string_view name) const noexcept {
    const auto* inst = findVideo(name);
    return inst && inst->playing && !inst->paused;
}

bool VideoManager::isPaused(std::string_view name) const noexcept {
    const auto* inst = findVideo(name);
    return inst && inst->paused;
}

bool VideoManager::hasEnded(std::string_view name) const noexcept {
    const auto* inst = findVideo(name);
    return inst && inst->ended;
}

bool VideoManager::hasError(std::string_view name) const noexcept {
    const auto* inst = findVideo(name);
    return inst && inst->error;
}

std::string_view VideoManager::errorMessage(std::string_view name) const noexcept {
    const auto* inst = findVideo(name);
    return inst ? std::string_view{inst->errorMessage} : std::string_view{};
}

Texture2D VideoManager::getFrameTexture(std::string_view name) const noexcept {
    const auto* inst = findVideo(name);
    if (!inst || !inst->loaded || inst->error || !inst->backend) {
        return {};
    }
    return inst->backend->texture();
}

void VideoManager::setLooping(std::string_view name, const bool loop) {
    auto* inst = findVideo(name);
    if (!inst) {
        return;
    }
    inst->looping = loop;
    if (inst->backend) {
        inst->backend->setLooping(loop);
    }
}

void VideoManager::setVolume(std::string_view name, const f32 volume) {
    auto* inst = findVideo(name);
    if (!inst) {
        return;
    }
    inst->volume = std::clamp(volume, 0.0f, 1.0f);
    if (inst->backend) {
        inst->backend->setVolume(inst->volume);
    }
}

void VideoManager::mute(std::string_view name) noexcept {
    auto* inst = findVideo(name);
    if (inst && inst->backend) {
        inst->volume = 0.0f;
        inst->backend->setVolume(0.0f);
    }
}

void VideoManager::unmute(std::string_view name) noexcept {
    auto* inst = findVideo(name);
    if (inst && inst->backend) {
        inst->volume = 1.0f;
        inst->backend->setVolume(1.0f);
    }
}

void VideoManager::setError(VideoInstance& inst, std::string_view message) {
    inst.playing = false;
    inst.paused = false;
    inst.error = true;
    inst.errorMessage = message.empty() ? "Unknown video error" : std::string{message};
    if (inst.backend) {
        inst.backend->stop();
    }
    spdlog::error("VideoManager: '{}': {}", inst.name, inst.errorMessage);
    ::biofuel::engine::runtime::typed::Events::publish<::biofuel::engine::runtime::typed::video::Error>({
        .videoName = inst.name,
        .errorMessage = inst.errorMessage
    });
}

VideoManager::VideoInstance* VideoManager::findVideo(std::string_view name) noexcept {
    const auto it = m_videos.find(name);
    return it == m_videos.end() ? nullptr : &it->second;
}

const VideoManager::VideoInstance* VideoManager::findVideo(std::string_view name) const noexcept {
    const auto it = m_videos.find(name);
    return it == m_videos.end() ? nullptr : &it->second;
}

} // namespace biofuel::engine::video
