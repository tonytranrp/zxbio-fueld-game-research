#include "ModelSystem.hpp"
#include <algorithm>
#include <cmath>
#include <span>

// ------------------------------------------------------------------------------
// ModelAnimator — per-instance animation state machine.
//
// Split out of ModelSystem.cpp. Owns the active/blend/return state tracking and
// advances Raylib skeletal animation each frame. Holds no GPU resources of its
// own; ModelInstance owns the Model and feeds clips in through update().
// ------------------------------------------------------------------------------

namespace biofuel::engine::models {

void ModelAnimator::configure(
    const std::span<const ModelAnimationStateSpec> states,
    const std::string_view defaultIdleState,
    const i32 clipCount) noexcept
{
    m_states.clear();
    m_states.reserve(states.size());
    for (const auto& state : states) {
        if (state.name.empty()) {
            continue;
        }

        if (state.clipIndex >= clipCount && state.clipIndex >= 0) {
            continue;
        }

        m_states.push_back(StateConfig{
            .name = std::string{state.name},
            .clipIndex = state.clipIndex,
            .loop = state.loop,
            .returnState = std::string{state.returnState},
            .durationSeconds = state.durationSeconds,
        });
    }

    m_defaultState = std::string{defaultIdleState};
    reset();
    if (!m_defaultState.empty()) {
        setState(m_defaultState);
    }
}

void ModelAnimator::reset() noexcept {
    m_currentState.clear();
    m_pendingReturnState.clear();
    m_stateElapsed = 0.0f;
    m_stateProgress = 0.0f;
    m_transitionDuration = 0.0f;
    m_transitionElapsed = 0.0f;
    m_lastFrame = 0;
}

void ModelAnimator::setState(const std::string_view stateName, const f32 transitionSeconds) noexcept {
    const StateConfig* state = findState(stateName);
    if (state == nullptr) {
        return;
    }

    beginState(*state, transitionSeconds);
}

void ModelAnimator::playAction(const std::string_view stateName, const f32 transitionSeconds) noexcept {
    const StateConfig* state = findState(stateName);
    if (state == nullptr) {
        return;
    }

    beginState(*state, transitionSeconds);
    scheduleReturn(*state);
}

void ModelAnimator::update(Model& model, const ModelAnimation* clips, const i32 clipCount, const f32 dt) noexcept {
    if (m_currentState.empty()) {
        return;
    }

    const StateConfig* state = findState(m_currentState);
    if (state == nullptr) {
        return;
    }

    m_stateElapsed += dt;
    m_transitionElapsed = std::min(m_transitionElapsed + dt, m_transitionDuration);
    const f32 duration = std::max(resolveDurationSeconds(*state, clipCount, clips), 0.0f);
    if (duration > 0.0f) {
        if (state->loop) {
            const f32 normalized = std::fmod(std::max(m_stateElapsed, 0.0f), duration) / duration;
            m_stateProgress = std::clamp(normalized, 0.0f, 1.0f);
        } else {
            m_stateProgress = std::clamp(m_stateElapsed / duration, 0.0f, 1.0f);
        }
    } else {
        m_stateProgress = state->loop ? 0.0f : 1.0f;
    }

    if (state->clipIndex >= 0 && state->clipIndex < clipCount && clips != nullptr) {
        const ModelAnimation& clip = clips[state->clipIndex];
        if (clip.frameCount > 0) {
            const f32 clipDuration = std::max(duration, 0.001f);
            f32 clipTime = m_stateElapsed;
            if (state->loop) {
                clipTime = std::fmod(clipTime, clipDuration);
            } else {
                clipTime = std::min(clipTime, clipDuration);
            }

            const f32 normalized = std::clamp(clipTime / clipDuration, 0.0f, 1.0f);
            const i32 frame = std::clamp(
                static_cast<i32>(normalized * static_cast<f32>(std::max(clip.frameCount - 1, 0))),
                0,
                std::max(clip.frameCount - 1, 0)
            );
            if (frame != m_lastFrame || m_stateElapsed <= dt) {
                UpdateModelAnimation(model, clip, frame);
                m_lastFrame = frame;
            }
        }
    }

    if (!state->loop) {
        if (duration <= 0.0f || m_stateElapsed >= duration) {
            if (!m_pendingReturnState.empty()) {
                setState(m_pendingReturnState, 0.14f);
                m_pendingReturnState.clear();
            } else if (!state->returnState.empty()) {
                setState(state->returnState, 0.14f);
            } else if (!m_defaultState.empty() && m_currentState != m_defaultState) {
                setState(m_defaultState, 0.14f);
            }
        }
    }
}

bool ModelAnimator::hasState(const std::string_view stateName) const noexcept {
    return findState(stateName) != nullptr;
}

f32 ModelAnimator::transitionProgress() const noexcept {
    if (m_transitionDuration <= 0.0f) {
        return 1.0f;
    }
    return std::clamp(m_transitionElapsed / m_transitionDuration, 0.0f, 1.0f);
}

f32 ModelAnimator::stateProgress() const noexcept {
    return std::clamp(m_stateProgress, 0.0f, 1.0f);
}

const ModelAnimator::StateConfig* ModelAnimator::findState(const std::string_view stateName) const noexcept {
    const auto it = std::find_if(m_states.begin(), m_states.end(),
        [stateName](const StateConfig& state) { return state.name == stateName; });
    if (it == m_states.end()) {
        return nullptr;
    }
    return &(*it);
}

f32 ModelAnimator::resolveDurationSeconds(
    const StateConfig& state,
    const i32 clipCount,
    const ModelAnimation* clips) const noexcept
{
    if (state.durationSeconds > 0.0f) {
        return state.durationSeconds;
    }

    if (state.clipIndex >= 0 && state.clipIndex < clipCount && clips != nullptr) {
        const i32 frameCount = clips[state.clipIndex].frameCount;
        if (frameCount > 0) {
            return static_cast<f32>(frameCount) / 24.0f;
        }
    }

    return 0.0f;
}

void ModelAnimator::beginState(const StateConfig& state, const f32 transitionSeconds) noexcept {
    m_currentState = state.name;
    m_stateElapsed = 0.0f;
    m_stateProgress = 0.0f;
    m_transitionDuration = std::max(transitionSeconds, 0.0f);
    m_transitionElapsed = 0.0f;
    m_lastFrame = 0;
}

void ModelAnimator::scheduleReturn(const StateConfig& state) noexcept {
    if (!state.returnState.empty()) {
        m_pendingReturnState = state.returnState;
        return;
    }

    if (!m_defaultState.empty() && state.name != m_defaultState) {
        m_pendingReturnState = m_defaultState;
        return;
    }

    m_pendingReturnState.clear();
}

} // namespace biofuel::engine::models
