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

read_required("src/engine/audio/AudioManager.cpp" AUDIO_MANAGER)
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

read_required("src/engine/core/LoadingTask.hpp" LOADING_TASK)
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

read_required("src/engine/ui/ScreenManager.cpp" SCREEN_MANAGER_CPP)
require_contains(
    "ScreenManager::ensureTransitionTextures must require both render surfaces to be valid"
    "${SCREEN_MANAGER_CPP}"
    "return m_transitionOut.valid() && m_transitionIn.valid();")
require_contains(
    "Crossfade must fall back to direct render when render texture allocation fails"
    "${SCREEN_MANAGER_CPP}"
    "if (!ensureTransitionTextures(sw, sh)) {
        renderSlotToBackbuffer(incoming, sw, sh);
        return;
    }")
require_contains(
    "Crossfade shader fallback must not draw an invalid transition texture"
    "${SCREEN_MANAGER_CPP}"
    "if (m_transitionIn.valid()) {
            Renderer::drawRenderTexture(m_transitionIn.texture());
        } else {
            renderSlotToBackbuffer(incoming, sw, sh);
        }")

message(STATUS "Runtime safety guard passed: audio, loading task, and crossfade fallbacks are hardened.")
