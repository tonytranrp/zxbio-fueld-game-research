#include "ScreenManager.hpp"
#include "Screen.hpp"
#include "Utils/render/Render.hpp"
#include "AnimationController/animation/Easing.hpp"
#include <spdlog/spdlog.h>

namespace biofuel::ui {

// ------------------------------------------------------------------------------
// Singleton
// ------------------------------------------------------------------------------

ScreenManager& ScreenManager::instance() {
    static ScreenManager instance;
    return instance;
}

// ------------------------------------------------------------------------------
// Lifecycle
// ------------------------------------------------------------------------------

void ScreenManager::init() {
    // Screen stack starts empty
}

void ScreenManager::shutdown() {
    clear();
}

// ------------------------------------------------------------------------------
// Stack Operations
// ------------------------------------------------------------------------------

void ScreenManager::push(std::unique_ptr<Screen> screen) {
    if (isTransitioning()) {
        spdlog::warn("ScreenManager::push() ignored — transition in progress");
        return;
    }

    if (!m_screens.empty()) {
        m_screens.back()->onPause();
    }

    screen->setManager(this);
    screen->onEnter();
    screen->m_transitionState = Screen::TransitionState::TransitionIn;
    screen->m_transitionProgress = 0.0f;

    m_screens.push_back(std::move(screen));
}

void ScreenManager::pop() {
    if (isTransitioning()) {
        spdlog::warn("ScreenManager::pop() ignored — transition in progress");
        return;
    }

    if (m_screens.empty()) {
        spdlog::warn("ScreenManager::pop() called on empty stack");
        return;
    }

    m_screens.back()->m_transitionState = Screen::TransitionState::TransitionOut;
    m_screens.back()->m_transitionProgress = 0.0f;
}

void ScreenManager::replace(std::unique_ptr<Screen> screen) {
    if (isTransitioning()) {
        spdlog::warn("ScreenManager::replace() ignored — transition in progress");
        return;
    }

    if (!m_screens.empty()) {
        m_screens.back()->onPause();
        m_screens.back()->m_transitionState = Screen::TransitionState::TransitionOut;
        m_screens.back()->m_transitionProgress = 0.0f;
    }

    screen->setManager(this);
    screen->onEnter();
    screen->m_transitionState = Screen::TransitionState::TransitionIn;
    screen->m_transitionProgress = 0.0f;

    m_screens.push_back(std::move(screen));
}

void ScreenManager::clear() {
    while (!m_screens.empty()) {
        m_screens.back()->onExit();
        m_screens.pop_back();
    }
}

// ------------------------------------------------------------------------------
// Per-Frame Delegation
// ------------------------------------------------------------------------------

void ScreenManager::update(f32 dt) {
    // Update screens top-to-bottom
    for (auto it = m_screens.rbegin(); it != m_screens.rend(); ++it) {
        bool isTop = (it == m_screens.rbegin());
        if (isTop || (*it)->m_passthroughUpdate) {
            (*it)->onUpdate(dt);
        }
    }

    // Advance transitions for all screens with easing support
    for (auto& screen : m_screens) {
        if (screen->isTransitioning()) {
            screen->m_transitionProgress += dt / screen->m_transitionDuration;

            if (screen->m_transitionProgress >= 1.0f) {
                screen->m_transitionProgress = 1.0f;

                if (screen->m_transitionState == Screen::TransitionState::TransitionIn) {
                    screen->m_transitionState = Screen::TransitionState::None;
                }
                // TransitionOut: stay at TransitionOut + progress=1.0 so needsRemoval() returns true
            }
        }
    }

    // Remove screens that completed transition-out
    for (auto it = m_screens.begin(); it != m_screens.end(); ) {
        if ((*it)->needsRemoval()) {
            bool wasTop = (it->get() == m_screens.back().get());
            (*it)->onExit();
            it = m_screens.erase(it);
            if (wasTop && !m_screens.empty()) {
                m_screens.back()->onResume();
            }
        } else {
            ++it;
        }
    }
}

void ScreenManager::render() {
    // Render screens bottom-to-top
    for (auto& screen : m_screens) {
        bool isTop = (&screen == &m_screens.back());
        if (isTop || screen->m_passthroughRender) {
            screen->onRender();
        }
    }

    // Draw fade overlay if top screen is transitioning
    if (!m_screens.empty() && m_screens.back()->isTransitioning()) {
        Screen* top = m_screens.back().get();
        f32 overlayAlpha = 0.0f;

        if (top->m_transitionState == Screen::TransitionState::TransitionIn) {
            // Apply easing to the fade overlay for smooth transition
            // TransitionIn: fade from black (alpha=255) to transparent (alpha=0)
            // Use easeOutQuad for a smooth deceleration
            f32 easedProgress = animation::Easing::easeOutQuad(top->m_transitionProgress);
            overlayAlpha = (1.0f - easedProgress) * 255.0f;
        } else if (top->m_transitionState == Screen::TransitionState::TransitionOut) {
            // TransitionOut: fade from transparent to black
            // Use easeInQuad for a smooth acceleration
            f32 easedProgress = animation::Easing::easeInQuad(top->m_transitionProgress);
            overlayAlpha = easedProgress * 255.0f;
        }

        Color fadeColor = {0, 0, 0, static_cast<u8>(overlayAlpha)};
        utils::render::Renderer::drawRect(
            0, 0,
            utils::render::Renderer::screenWidth(),
            utils::render::Renderer::screenHeight(),
            fadeColor);
    }
}

void ScreenManager::handleInput() {
    if (m_screens.empty()) {
        return;
    }

    m_screens.back()->onInput();

    if (m_screens.back()->m_passthroughInput && m_screens.size() > 1) {
        auto it = m_screens.rbegin();
        ++it;
        (*it)->onInput();
    }
}

// ------------------------------------------------------------------------------
// Accessors
// ------------------------------------------------------------------------------

Screen* ScreenManager::currentScreen() const noexcept {
    if (m_screens.empty()) {
        return nullptr;
    }
    return m_screens.back().get();
}

bool ScreenManager::isEmpty() const noexcept {
    return m_screens.empty();
}

size_t ScreenManager::stackSize() const noexcept {
    return m_screens.size();
}

bool ScreenManager::isTransitioning() const noexcept {
    for (const auto& screen : m_screens) {
        if (screen->isTransitioning()) {
            return true;
        }
    }
    return false;
}

} // namespace biofuel::ui