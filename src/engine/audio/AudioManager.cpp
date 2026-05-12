#include "AudioManager.hpp"
#include "engine/debug/MemoryTelemetry.hpp"
#include <algorithm>

namespace biofuel::engine::audio {

AudioManager& AudioManager::instance() noexcept {
    static AudioManager mgr;
    return mgr;
}

AudioManager::~AudioManager() noexcept {
    if (m_initialized) {
        shutdown();
    }
}

// -----------------------------------------------------------------------------
// Lifecycle
// -----------------------------------------------------------------------------

void AudioManager::init() {
    if (m_initialized) return;
    InitAudioDevice();
    m_initialized = true;
}

void AudioManager::shutdown() noexcept {
    unloadAllSounds();
    unloadAllMusic();
    if (m_initialized) {
        CloseAudioDevice();
        m_initialized = false;
    }
}

void AudioManager::update() noexcept {
    if (!m_initialized) return;
    if (!m_currentMusic.empty()) {
        UpdateMusicStream(m_musicTracks.at(m_currentMusic));
    }
}

// -----------------------------------------------------------------------------
// Sound effects
// -----------------------------------------------------------------------------

void AudioManager::loadSound(std::string_view name, std::string_view path) {
    const std::string key(name);
    unloadSound(key);
    const std::string filePath(path);
    Sound s = LoadSound(filePath.c_str());
    m_sounds.emplace(key, s);
    ::biofuel::engine::debug::MemoryTelemetry::add(
        ::biofuel::engine::debug::ResourceKind::AudioAsset,
        1,
        static_cast<i64>(s.frameCount) * static_cast<i64>(s.stream.channels) * static_cast<i64>(s.stream.sampleSize / 8));
    applySfxVolume(key);
}

void AudioManager::unloadSound(std::string_view name) {
    const std::string key(name);
    if (auto it = m_sounds.find(key); it != m_sounds.end()) {
        const Sound sound = it->second;
        ::biofuel::engine::debug::MemoryTelemetry::remove(
            ::biofuel::engine::debug::ResourceKind::AudioAsset,
            1,
            static_cast<i64>(sound.frameCount) * static_cast<i64>(sound.stream.channels) * static_cast<i64>(sound.stream.sampleSize / 8));
        UnloadSound(it->second);
        m_sounds.erase(it);
    }
}

void AudioManager::unloadAllSounds() noexcept {
    for (auto& [_, s] : m_sounds) {
        ::biofuel::engine::debug::MemoryTelemetry::remove(
            ::biofuel::engine::debug::ResourceKind::AudioAsset,
            1,
            static_cast<i64>(s.frameCount) * static_cast<i64>(s.stream.channels) * static_cast<i64>(s.stream.sampleSize / 8));
        UnloadSound(s);
    }
    m_sounds.clear();
}

void AudioManager::playSound(std::string_view name) {
    const std::string key(name);
    if (auto it = m_sounds.find(key); it != m_sounds.end()) {
        PlaySound(it->second);
    }
}

void AudioManager::playSoundPitched(std::string_view name, f32 pitch) {
    const std::string key(name);
    if (auto it = m_sounds.find(key); it != m_sounds.end()) {
        SetSoundPitch(it->second, pitch);
        PlaySound(it->second);
    }
}

void AudioManager::playSoundAtVolume(std::string_view name, f32 volume) {
    const std::string key(name);
    if (auto it = m_sounds.find(key); it != m_sounds.end()) {
        SetSoundVolume(it->second, volume);
        PlaySound(it->second);
    }
}

bool AudioManager::hasSound(std::string_view name) const noexcept {
    return m_sounds.find(name) != m_sounds.end();
}

// -----------------------------------------------------------------------------
// Music (streaming)
// -----------------------------------------------------------------------------

void AudioManager::loadMusic(std::string_view name, std::string_view path) {
    const std::string key(name);
    unloadMusic(key);
    const std::string filePath(path);
    Music m = LoadMusicStream(filePath.c_str());
    m.looping = true;
    m_musicTracks.emplace(key, m);
    ::biofuel::engine::debug::MemoryTelemetry::add(
        ::biofuel::engine::debug::ResourceKind::AudioAsset,
        1,
        0);
}

void AudioManager::unloadMusic(std::string_view name) {
    const std::string key(name);
    if (auto it = m_musicTracks.find(key); it != m_musicTracks.end()) {
        if (m_currentMusic == key) {
            StopMusicStream(it->second);
            m_currentMusic.clear();
        }
        ::biofuel::engine::debug::MemoryTelemetry::remove(
            ::biofuel::engine::debug::ResourceKind::AudioAsset,
            1,
            0);
        UnloadMusicStream(it->second);
        m_musicTracks.erase(it);
    }
}

void AudioManager::unloadAllMusic() noexcept {
    if (!m_currentMusic.empty()) {
        StopMusicStream(m_musicTracks.at(m_currentMusic));
        m_currentMusic.clear();
    }
    for (auto& [_, m] : m_musicTracks) {
        ::biofuel::engine::debug::MemoryTelemetry::remove(
            ::biofuel::engine::debug::ResourceKind::AudioAsset,
            1,
            0);
        UnloadMusicStream(m);
    }
    m_musicTracks.clear();
}

void AudioManager::playMusic(std::string_view name) {
    const std::string key(name);
    auto it = m_musicTracks.find(key);
    if (it == m_musicTracks.end()) return;

    if (m_currentMusic == key) {
        if (m_musicPaused) {
            ResumeMusicStream(it->second);
            m_musicPaused = false;
        }
        return;
    }

    if (!m_currentMusic.empty()) {
        StopMusicStream(m_musicTracks.at(m_currentMusic));
    }

    m_currentMusic = key;
    m_musicPaused = false;
    PlayMusicStream(it->second);
}

void AudioManager::stopMusic() noexcept {
    if (m_currentMusic.empty()) return;
    StopMusicStream(m_musicTracks.at(m_currentMusic));
    m_currentMusic.clear();
    m_musicPaused = false;
}

void AudioManager::pauseMusic() noexcept {
    if (m_currentMusic.empty()) return;
    PauseMusicStream(m_musicTracks.at(m_currentMusic));
    m_musicPaused = true;
}

void AudioManager::resumeMusic() noexcept {
    if (m_currentMusic.empty() || !m_musicPaused) return;
    ResumeMusicStream(m_musicTracks.at(m_currentMusic));
    m_musicPaused = false;
}

bool AudioManager::isMusicPlaying() const noexcept {
    if (m_currentMusic.empty() || m_musicPaused) return false;
    return IsMusicStreamPlaying(m_musicTracks.at(m_currentMusic));
}

bool AudioManager::hasMusic(std::string_view name) const noexcept {
    return m_musicTracks.find(name) != m_musicTracks.end();
}

// -----------------------------------------------------------------------------
// Volume
// -----------------------------------------------------------------------------

void AudioManager::setMasterVolume(f32 volume) {
    m_masterVolume = std::clamp(volume, 0.0f, 1.0f);
    if (!m_muted) {
        SetMasterVolume(m_masterVolume * m_mutedVolume);
    }
}

void AudioManager::setSfxVolume(f32 volume) {
    m_sfxVolume = std::clamp(volume, 0.0f, 1.0f);
    if (!m_muted) {
        applyAllSfxVolumes();
    }
}

void AudioManager::setMusicVolume(f32 volume) {
    m_musicVolume = std::clamp(volume, 0.0f, 1.0f);
    if (!m_currentMusic.empty() && !m_muted) {
        SetMusicVolume(m_musicTracks.at(m_currentMusic), m_musicVolume);
    }
}

// -----------------------------------------------------------------------------
// Mute
// -----------------------------------------------------------------------------

void AudioManager::mute() noexcept {
    if (m_muted) return;
    m_muted = true;
    m_mutedVolume = m_masterVolume;
    SetMasterVolume(0.0f);
}

void AudioManager::unmute() noexcept {
    if (!m_muted) return;
    m_muted = false;
    SetMasterVolume(m_masterVolume * m_mutedVolume);
}

// -----------------------------------------------------------------------------
// Internal helpers
// -----------------------------------------------------------------------------

void AudioManager::applySfxVolume(std::string_view name) {
    if (auto it = m_sounds.find(std::string(name)); it != m_sounds.end()) {
        SetSoundVolume(it->second, m_sfxVolume);
    }
}

void AudioManager::applyAllSfxVolumes() noexcept {
    for (auto& [_, s] : m_sounds) {
        SetSoundVolume(s, m_sfxVolume);
    }
}

} // namespace biofuel::engine::audio
