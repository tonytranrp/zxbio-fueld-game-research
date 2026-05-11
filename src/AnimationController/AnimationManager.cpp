#include "AnimationManager.hpp"
#include <spdlog/spdlog.h>

namespace biofuel::animation {

AnimationManager& AnimationManager::instance() noexcept {
    static AnimationManager instance;
    return instance;
}

void AnimationManager::init() {
    m_animations.clear();
    spdlog::debug("AnimationManager initialized");
}

void AnimationManager::shutdown() {
    cancelAll();
    m_animations.clear();
    spdlog::debug("AnimationManager shutdown");
}

void AnimationManager::update(const f32 dt) {
    for (auto& anim : m_animations) {
        anim->update(dt);
    }
    prune();
}

void AnimationManager::prune() {
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
        spdlog::trace("AnimationManager pruned {} animation(s)", removed);
    }
}

void AnimationManager::cancelAll() {
    for (auto& anim : m_animations) {
        anim->cancel();
    }
    prune();
}

void AnimationManager::cancelAll(const std::string& name) {
    for (auto& anim : m_animations) {
        if (anim->name() == name) {
            anim->cancel();
        }
    }
    prune();
}

// ---- Explicit template instantiations ----
void AnimationManager::add(std::unique_ptr<Animation<f32>> anim) {
    m_animations.emplace_back(std::make_unique<AnimationWrapper<f32>>(std::move(anim)));
}
void AnimationManager::add(std::unique_ptr<Animation<Color>> anim) {
    m_animations.emplace_back(std::make_unique<AnimationWrapper<Color>>(std::move(anim)));
}
void AnimationManager::add(std::unique_ptr<Animation<Vector2>> anim) {
    m_animations.emplace_back(std::make_unique<AnimationWrapper<Vector2>>(std::move(anim)));
}
void AnimationManager::add(std::unique_ptr<Animation<Rectangle>> anim) {
    m_animations.emplace_back(std::make_unique<AnimationWrapper<Rectangle>>(std::move(anim)));
}

} // namespace biofuel::animation