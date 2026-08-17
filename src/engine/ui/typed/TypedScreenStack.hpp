#pragma once

#include "engine/ui/typed/ScreenRegistry.hpp"
#include "engine/ui/typed/ScreenSlot.hpp"
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace biofuel::engine::ui::typed {

template<typename TRegistry>
class TypedScreenStack {
public:
    using Container = std::vector<ScreenSlot>;
    using iterator = Container::iterator;
    using const_iterator = Container::const_iterator;

    [[nodiscard]] ScreenSlot makeBridgeSlot(std::unique_ptr<::biofuel::engine::ui::Screen> screen, TransitionPolicyData policy) const {
        return ScreenSlot{std::move(screen), policy};
    }

    void push(ScreenSlot slot) {
        m_slots.push_back(std::move(slot));
    }

    void popBack() {
        m_slots.pop_back();
    }

    [[nodiscard]] ScreenSlot& back() noexcept {
        return m_slots.back();
    }

    [[nodiscard]] const ScreenSlot& back() const noexcept {
        return m_slots.back();
    }

    [[nodiscard]] bool empty() const noexcept {
        return m_slots.empty();
    }

    [[nodiscard]] size_t size() const noexcept {
        return m_slots.size();
    }

    void clear() noexcept {
        m_slots.clear();
    }

    [[nodiscard]] ScreenSlot& operator[](size_t index) noexcept {
        return m_slots[index];
    }

    [[nodiscard]] const ScreenSlot& operator[](size_t index) const noexcept {
        return m_slots[index];
    }

    [[nodiscard]] iterator begin() noexcept { return m_slots.begin(); }
    [[nodiscard]] iterator end() noexcept { return m_slots.end(); }
    [[nodiscard]] const_iterator begin() const noexcept { return m_slots.begin(); }
    [[nodiscard]] const_iterator end() const noexcept { return m_slots.end(); }

    [[nodiscard]] auto rbegin() noexcept { return m_slots.rbegin(); }
    [[nodiscard]] auto rend() noexcept { return m_slots.rend(); }
    [[nodiscard]] auto rbegin() const noexcept { return m_slots.rbegin(); }
    [[nodiscard]] auto rend() const noexcept { return m_slots.rend(); }

    iterator erase(iterator it) {
        return m_slots.erase(it);
    }

private:
    Container m_slots;
};

} // namespace biofuel::engine::ui::typed
