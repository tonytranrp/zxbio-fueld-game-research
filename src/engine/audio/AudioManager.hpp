#pragma once

#include "engine/core/Types.hpp"
#include <raylib.h>
#include <string>
#include <string_view>
#include <unordered_map>

namespace biofuel::engine::audio {

// -----------------------------------------------------------------------------
// AudioManager - Sound effect and music playback wrapped around raylib/miniaudio.
//
// Sounds (SFX) are loaded fully into memory for instant playback.
// Music is streamed for long-running background tracks.
//
// Lifecycle:
//   init()           — Opens the audio device. Call once at startup.
//   update()         — Call every frame to pump streaming music buffers.
//   shutdown()       — Unloads everything and closes the device.
// -----------------------------------------------------------------------------
class AudioManager {
public:
    [[nodiscard]] static AudioManager& instance() noexcept;

    // ---------- lifecycle ----------
    void init();
    void shutdown() noexcept;
    void update() noexcept;

    // ---------- sound effects ----------
    void loadSound(std::string_view name, std::string_view path);
    void unloadSound(std::string_view name);
    void unloadAllSounds() noexcept;

    void playSound(std::string_view name);
    void playSoundPitched(std::string_view name, f32 pitch); // 1.0 = normal
    void playSoundAtVolume(std::string_view name, f32 volume); // 0.0–1.0

    [[nodiscard]] bool hasSound(std::string_view name) const noexcept;

    // ---------- music (streaming) ----------
    void loadMusic(std::string_view name, std::string_view path);
    void unloadMusic(std::string_view name);
    void unloadAllMusic() noexcept;

    void playMusic(std::string_view name);
    void stopMusic() noexcept;
    void pauseMusic() noexcept;
    void resumeMusic() noexcept;
    [[nodiscard]] bool isMusicPlaying() const noexcept;

    [[nodiscard]] bool hasMusic(std::string_view name) const noexcept;

    // ---------- volume ----------
    void setMasterVolume(f32 volume);       // 0.0–1.0, affects everything
    void setSfxVolume(f32 volume);          // 0.0–1.0, affects sound effects
    void setMusicVolume(f32 volume);        // 0.0–1.0, affects music only

    [[nodiscard]] f32 masterVolume() const noexcept { return m_masterVolume; }
    [[nodiscard]] f32 sfxVolume() const noexcept { return m_sfxVolume; }
    [[nodiscard]] f32 musicVolume() const noexcept { return m_musicVolume; }

    // ---------- helpers ----------
    void mute() noexcept;
    void unmute() noexcept;
    [[nodiscard]] bool isMuted() const noexcept { return m_muted; }

    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;
    AudioManager(AudioManager&&) = delete;
    AudioManager& operator=(AudioManager&&) = delete;

private:
    AudioManager() = default;
    ~AudioManager() noexcept;

    void applySfxVolume(std::string_view name);
    void applyAllSfxVolumes() noexcept;

    std::unordered_map<std::string, Sound, TransparentHash, std::equal_to<>> m_sounds;
    std::unordered_map<std::string, Music, TransparentHash, std::equal_to<>> m_musicTracks;

    std::string m_currentMusic;
    bool m_musicPaused = false;

    f32 m_masterVolume = 1.0f;
    f32 m_sfxVolume = 1.0f;
    f32 m_musicVolume = 1.0f;
    f32 m_mutedVolume = 1.0f;
    bool m_muted = false;
    bool m_initialized = false;
};

} // namespace biofuel::engine::audio
