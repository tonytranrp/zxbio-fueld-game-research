if(NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "SOURCE_DIR is required")
endif()

function(read_required relative_path out_var)
    set(full_path "${SOURCE_DIR}/${relative_path}")
    if(NOT EXISTS "${full_path}")
        message(FATAL_ERROR "Runtime safety guard missing required file: ${relative_path}")
    endif()
    file(READ "${full_path}" contents)
    set(${out_var} "${contents}" PARENT_SCOPE)
endfunction()

function(require_contains label contents needle)
    string(FIND "${contents}" "${needle}" found_index)
    if(found_index EQUAL -1)
        message(FATAL_ERROR "Runtime safety guard failed: ${label}")
    endif()
endfunction()

set(RAYLIB_LIFETIME_PATTERN
    "(Load(Texture|TextureFromImage|Image|ImageFromMemory|RenderTexture|Shader|ShaderFromMemory|Sound|MusicStream|Model|ModelFromMesh|ModelAnimations|Font|FontEx)|Unload(Texture|Image|RenderTexture|Shader|Sound|MusicStream|Model|ModelAnimations|Font)|InitAudioDevice|CloseAudioDevice|InitWindow|CloseWindow)")

set(ALLOWED_RAYLIB_LIFETIME_FILES
    "src/engine/app/App.cpp"
    "src/engine/app/AppLifecycle.cpp"
    "src/engine/audio/AudioManager.cpp"
    "src/engine/fonts/FontUtils.cpp"
    "src/engine/graphics/RenderSurface.hpp"
    "src/engine/graphics/ShaderManager.cpp"
    "src/engine/models/ModelSystem.cpp"
    "src/engine/video/VideoFfmpegBackend.cpp"
    "src/engine/video/VideoManager.cpp")

file(GLOB_RECURSE RAYLIB_LIFETIME_SCAN_FILES
    "${SOURCE_DIR}/src/engine/*.cpp"
    "${SOURCE_DIR}/src/engine/*.hpp"
    "${SOURCE_DIR}/src/engine/*.h"
    "${SOURCE_DIR}/src/game/*.cpp"
    "${SOURCE_DIR}/src/game/*.hpp"
    "${SOURCE_DIR}/src/game/*.h")
foreach(full_path IN LISTS RAYLIB_LIFETIME_SCAN_FILES)
    file(RELATIVE_PATH relative_path "${SOURCE_DIR}" "${full_path}")
    string(REPLACE "\\" "/" relative_path "${relative_path}")
    file(READ "${full_path}" raylib_lifetime_contents)
    string(REGEX REPLACE "(^|\n)[ \t]*//[^\n]*" "\\1" raylib_lifetime_contents "${raylib_lifetime_contents}")
    if(raylib_lifetime_contents MATCHES "${RAYLIB_LIFETIME_PATTERN}")
        if(relative_path MATCHES "^src/game/")
            message(FATAL_ERROR "Runtime safety guard failed: game code must not own raw Raylib resource lifetime calls (${relative_path})")
        endif()
        list(FIND ALLOWED_RAYLIB_LIFETIME_FILES "${relative_path}" allowed_lifetime_file)
        if(allowed_lifetime_file EQUAL -1)
            message(FATAL_ERROR "Runtime safety guard failed: raw Raylib lifetime calls must stay inside approved engine managers/caches/RAII helpers (${relative_path})")
        endif()
    endif()
endforeach()

read_required("src/engine/audio/AudioManager.hpp" AUDIO_MANAGER_HPP)
read_required("src/engine/audio/AudioManager.cpp" AUDIO_MANAGER)
require_contains(
    "AudioManager must centralize stale current-music recovery in the header contract"
    "${AUDIO_MANAGER_HPP}"
    "recoverStaleCurrentMusic(std::string_view caller) noexcept")
require_contains(
    "AudioManager must warn when recovering stale current music"
    "${AUDIO_MANAGER}"
    "AudioManager::{} recovered stale current music")
require_contains(
    "AudioManager::update must use centralized stale current-music recovery"
    "${AUDIO_MANAGER}"
    "recoverStaleCurrentMusic(\"update\")")
require_contains(
    "AudioManager::playMusic must use centralized stale current-music recovery"
    "${AUDIO_MANAGER}"
    "recoverStaleCurrentMusic(\"playMusic\")")
require_contains(
    "AudioManager::pauseMusic must use centralized stale current-music recovery"
    "${AUDIO_MANAGER}"
    "recoverStaleCurrentMusic(\"pauseMusic\")")
require_contains(
    "AudioManager::resumeMusic must use centralized stale current-music recovery"
    "${AUDIO_MANAGER}"
    "recoverStaleCurrentMusic(\"resumeMusic\")")
require_contains(
    "AudioManager::mute must reapply SFX volumes so existing sounds become silent while muted"
    "${AUDIO_MANAGER}"
    "m_muted = true;
    applyMasterVolume();
    applyAllSfxVolumes();")
require_contains(
    "AudioManager SFX volume application must use the muted effective volume"
    "${AUDIO_MANAGER}"
    "SetSoundVolume(it->second, m_muted ? 0.0f : m_sfxVolume);")
require_contains(
    "AudioManager all-SFX volume application must use the muted effective volume"
    "${AUDIO_MANAGER}"
    "SetSoundVolume(s, m_muted ? 0.0f : m_sfxVolume);")
require_contains(
    "AudioManager unloading current music must clear stale paused state"
    "${AUDIO_MANAGER}"
    "m_currentMusic.clear();
            m_musicPaused = false;")

read_required("src/engine/tasks/LoadingTask.hpp" LOADING_TASK)
require_contains(
    "LoadingTaskQueue must expose a failed state"
    "${LOADING_TASK}"
    "bool m_failed = false;")
require_contains(
    "LoadingTaskQueue must expose failureMessage()"
    "${LOADING_TASK}"
    "failureMessage() const noexcept")
require_contains(
    "LoadingTaskQueue::processNext must catch std::exception"
    "${LOADING_TASK}"
    "catch (const std::exception& ex)")
require_contains(
    "LoadingTaskQueue::processNext must catch unknown exceptions"
    "${LOADING_TASK}"
    "catch (...)")

read_required("src/game/screens/loading/LoadingScreen.cpp" LOADING_SCREEN)
read_required("src/engine/app/AppLifecycle.cpp" APP_LIFECYCLE)
read_required("src/engine/app/App.cpp" APP_CPP)
require_contains(
    "AppLifecycle must own startup task staging"
    "${APP_LIFECYCLE}"
    "void AppLifecycle::addStartupTasks")
require_contains(
    "AppLifecycle must own the loading-visible shader stage"
    "${APP_LIFECYCLE}"
    "void AppLifecycle::prepareLoadingPrelude()")
require_contains(
    "Application startup must delegate loading-visible shader staging to AppLifecycle"
    "${APP_CPP}"
    "AppLifecycle::prepareLoadingPrelude();")
require_contains(
    "Application must keep video playback pumping even when overlay screens freeze gameplay updates"
    "${APP_CPP}"
    "AudioService>().update();
    // Video overlays need per-frame pumping for decoded frames and Raylib audio
    // streams even when the top screen freezes gameplay updates below it.
    services.get<::biofuel::engine::runtime::typed::VideoService>().update();")
if(APP_CPP MATCHES "if \\(!freezeUnderlying\\)[^{]*\\{[^}]*VideoService>\\(\\)\\.update\\(\\)")
    message(FATAL_ERROR "Runtime safety guard failed: VideoService update must not be gated by freezeUnderlying")
endif()
require_contains(
    "AppLifecycle must own core service shutdown ordering"
    "${APP_LIFECYCLE}"
    "void AppLifecycle::shutdownCoreServices() noexcept")
require_contains(
    "Application shutdown must delegate service ordering to AppLifecycle"
    "${APP_CPP}"
    "AppLifecycle::shutdownCoreServices();")
require_contains(
    "LoadingScreen must delegate startup policy to AppLifecycle instead of owning core service order"
    "${LOADING_SCREEN}"
    "AppLifecycle::addStartupTasks")
if(LOADING_SCREEN MATCHES "Runtime::(events|animation|physics|model|audio|video)\\(\\)\\.init\\(")
    message(FATAL_ERROR "Runtime safety guard failed: LoadingScreen must not initialize core engine services directly")
endif()
if(APP_CPP MATCHES "Runtime::shader\\(\\)\\.init\\(|ShaderService|Shaders::load")
    message(FATAL_ERROR "Runtime safety guard failed: Application must not hardcode shader service bootstrapping outside AppLifecycle")
endif()
require_contains(
    "LoadingScreen status text must render loading task failure messages"
    "${LOADING_SCREEN}"
    "status = screen.m_tasks.failureMessage();")
require_contains(
    "LoadingScreen update must stop transition flow when a loading task fails"
    "${LOADING_SCREEN}"
    "if (m_tasks.isFailed()) {
            m_tasksDone = false;
            m_allowSkip = false;
            return;
        }")

read_required("src/engine/ui/ScreenManager.hpp" SCREEN_MANAGER_HPP)
require_contains(
    "ScreenManager::ensureTransitionTextures must return success/failure"
    "${SCREEN_MANAGER_HPP}"
    "[[nodiscard]] bool ensureTransitionTextures(i32 width, i32 height);")
require_contains(
    "ScreenManager must have a direct backbuffer render fallback helper"
    "${SCREEN_MANAGER_HPP}"
    "void renderSlotToBackbuffer(typed::ScreenSlot& slot, i32 width, i32 height);")

# Crossfade transition rendering lives in ScreenManagerRendering.cpp (split out
# of ScreenManager.cpp for readability); the safety invariants below still apply.
read_required("src/engine/ui/ScreenManagerRendering.cpp" SCREEN_MANAGER_RENDERING_CPP)
require_contains(
    "ScreenManager::ensureTransitionTextures must require both render surfaces to be valid"
    "${SCREEN_MANAGER_RENDERING_CPP}"
    "return m_transitionOut.valid() && m_transitionIn.valid();")
require_contains(
    "Crossfade must fall back to direct render when render texture allocation fails"
    "${SCREEN_MANAGER_RENDERING_CPP}"
    "if (!ensureTransitionTextures(sw, sh)) {
        renderSlotToBackbuffer(incoming, sw, sh);
        return;
    }")
require_contains(
    "Crossfade shader fallback must not draw an invalid transition texture"
    "${SCREEN_MANAGER_RENDERING_CPP}"
    "if (m_transitionIn.valid()) {
            Renderer::drawRenderTexture(m_transitionIn.texture());
        } else {
            renderSlotToBackbuffer(incoming, sw, sh);
        }")

message(STATUS "Runtime safety guard passed: audio, loading task, video pumping, and crossfade fallbacks are hardened.")
