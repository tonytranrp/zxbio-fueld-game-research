#pragma once

#include "engine/ui/typed/ScreenSlot.hpp"

namespace biofuel::engine::ui::typed {

class ScreenCommandQueue {
public:
    enum class Action : u8 {
        None,
        Push,
        Replace,
    };

    void push(ScreenSlot slot) {
        m_action = Action::Push;
        m_slot = std::move(slot);
    }

    void replace(ScreenSlot slot) {
        m_action = Action::Replace;
        m_slot = std::move(slot);
    }

    void pop() noexcept {
        m_pop = true;
    }

    [[nodiscard]] bool popRequested() const noexcept {
        return m_pop;
    }

    void consumePop() noexcept {
        m_pop = false;
    }

    [[nodiscard]] Action action() const noexcept {
        return m_action;
    }

    [[nodiscard]] bool hasSlot() const noexcept {
        return m_slot.get() != nullptr;
    }

    [[nodiscard]] ScreenSlot consumeSlot() noexcept {
        m_action = Action::None;
        return std::move(m_slot);
    }

    void clear() noexcept {
        m_action = Action::None;
        m_slot = {};
        m_pop = false;
    }

private:
    Action m_action = Action::None;
    ScreenSlot m_slot{};
    bool m_pop = false;
};

} // namespace biofuel::engine::ui::typed
