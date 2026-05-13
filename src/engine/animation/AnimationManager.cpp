#include "AnimationManager.hpp"
#include "engine/debug/MemoryTelemetry.hpp"
#include <spdlog/spdlog.h>

namespace biofuel::engine::animation {

AnimationManager& AnimationManager::instance() noexcept {
    static AnimationManager instance;
    return instance;
}

void AnimationManager::init() {
    m_animations.clear();
    m_pendingAnimations.clear();
    m_dispatchDepth = 0U;
    m_pruneRequested = false;
    spdlog::debug("AnimationManager initialized");
}

void AnimationManager::shutdown() {
    cancelAll();
    m_animations.clear();
    m_pendingAnimations.clear();
    m_dispatchDepth = 0U;
    m_pruneRequested = false;
    spdlog::debug("AnimationManager shutdown");
}

void AnimationManager::update(const f32 dt) {
    beginDispatch();
    for (auto& anim : m_animations) {
        anim->update(dt);
    }
    m_pruneRequested = true;
    endDispatch();
}

void AnimationManager::prune() {
    if (m_dispatchDepth > 0U) {
        m_pruneRequested = true;
        return;
    }
    if (m_animations.empty()) {
        return;
    }
    const auto before = m_animations.size();
    m_animations.erase(
        std::remove_if(m_animations.begin(), m_animations.end(),
            [](const std::unique_ptr<IAnimation>& a) { return a->shouldRemove(); }),
        m_animations.end()
    );
    const auto removed = before - m_animations.size();
    if (removed > 0) {
        ::biofuel::engine::debug::MemoryTelemetry::remove(
            ::biofuel::engine::debug::ResourceKind::Animation,
            static_cast<i64>(removed),
            0);
        spdlog::trace("AnimationManager pruned {} animation(s)", removed);
    }
}

void AnimationManager::cancelAll() {
    beginDispatch();
    for (auto& anim : m_animations) {
        anim->cancel();
    }
    m_pruneRequested = true;
    endDispatch();
}

void AnimationManager::cancelAll(const std::string& name) {
    beginDispatch();
    for (auto& anim : m_animations) {
        if (anim->name() == name) {
            anim->cancel();
        }
    }
    m_pruneRequested = true;
    endDispatch();
}

// ---- Explicit template instantiations ----
void AnimationManager::add(std::unique_ptr<Animation<f32>> anim) {
    appendTypedAnimation(std::move(anim));
}
void AnimationManager::add(std::unique_ptr<Animation<Color>> anim) {
    appendTypedAnimation(std::move(anim));
}
void AnimationManager::add(std::unique_ptr<Animation<Vector2>> anim) {
    appendTypedAnimation(std::move(anim));
}
void AnimationManager::add(std::unique_ptr<Animation<Rectangle>> anim) {
    appendTypedAnimation(std::move(anim));
}

void AnimationManager::beginDispatch() noexcept {
    ++m_dispatchDepth;
}

void AnimationManager::endDispatch() {
    if (m_dispatchDepth == 0U) {
        return;
    }
    --m_dispatchDepth;
    if (m_dispatchDepth == 0U) {
        flushDeferredChanges();
    }
}

void AnimationManager::flushDeferredChanges() {
    if (m_pruneRequested) {
        m_pruneRequested = false;
        prune();
    }

    if (m_pendingAnimations.empty()) {
        return;
    }

    m_animations.reserve(m_animations.size() + m_pendingAnimations.size());
    for (auto& anim : m_pendingAnimations) {
        m_animations.emplace_back(std::move(anim));
    }
    m_pendingAnimations.clear();
}

void AnimationManager::appendAnimation(std::unique_ptr<IAnimation> anim) {
    if (!anim) {
        return;
    }
    if (m_dispatchDepth > 0U) {
        m_pendingAnimations.emplace_back(std::move(anim));
        return;
    }
    m_animations.emplace_back(std::move(anim));
}

} // namespace biofuel::engine::animation
