#pragma once

#include "engine/core/Types.hpp"
#include "engine/core/typed/Meta.hpp"
#include <array>
#include <concepts>
#include <string_view>

namespace biofuel::engine::debug {

struct DebugOverlayContext {
    i32 screenWidth = 0;
    i32 screenHeight = 0;
    f32 frameTime = 0.0f;
};

struct DebugPanelTag {};

template<typename TPanel>
struct DebugPanelSpec;

struct FrameTimingDebugPanel : DebugPanelTag {};
struct MemoryTelemetryDebugPanel : DebugPanelTag {};
struct PhysicsDebugPanel : DebugPanelTag {};
struct AssetDebugPanel : DebugPanelTag {};

template<typename TPanel>
concept RegisteredDebugPanel = requires {
    typename DebugPanelSpec<TPanel>::Panel;
    { DebugPanelSpec<TPanel>::Name } -> std::convertible_to<std::string_view>;
    { DebugPanelSpec<TPanel>::Title } -> std::convertible_to<std::string_view>;
    { DebugPanelSpec<TPanel>::DefaultEnabled } -> std::convertible_to<bool>;
};

template<>
struct DebugPanelSpec<FrameTimingDebugPanel> {
    using Panel = FrameTimingDebugPanel;
    static constexpr std::string_view Name = "debug.frame_timing";
    static constexpr std::string_view Title = "Frame Timing";
    // Off by default; toggle live with F7.
    static constexpr bool DefaultEnabled = false;
};

template<>
struct DebugPanelSpec<MemoryTelemetryDebugPanel> {
    using Panel = MemoryTelemetryDebugPanel;
    static constexpr std::string_view Name = "debug.memory";
    static constexpr std::string_view Title = "Memory";
    // Off by default; toggle live with F4, or force-on by building with
    // -DBIOFUEL_DEBUG_MEMORY_STATS=ON.
#ifdef BIOFUEL_DEBUG_MEMORY_STATS
    static constexpr bool DefaultEnabled = true;
#else
    static constexpr bool DefaultEnabled = false;
#endif
};

template<>
struct DebugPanelSpec<PhysicsDebugPanel> {
    using Panel = PhysicsDebugPanel;
    static constexpr std::string_view Name = "debug.physics";
    static constexpr std::string_view Title = "Physics";
    static constexpr bool DefaultEnabled = false;
};

template<>
struct DebugPanelSpec<AssetDebugPanel> {
    using Panel = AssetDebugPanel;
    static constexpr std::string_view Name = "debug.assets";
    static constexpr std::string_view Title = "Assets";
    static constexpr bool DefaultEnabled = false;
};

using DebugPanelRegistry = ::biofuel::typed::Registry<
    FrameTimingDebugPanel,
    MemoryTelemetryDebugPanel,
    PhysicsDebugPanel,
    AssetDebugPanel>;

template<typename TRegistry>
struct DebugPanelRegistryValidator;

template<typename... TPanels>
struct DebugPanelRegistryValidator<::biofuel::typed::Registry<TPanels...>> {
    static consteval bool valid() {
        static_assert(::biofuel::typed::Registry<TPanels...>::valid(),
            "Debug panel registry entries must be unique.");
        static_assert((RegisteredDebugPanel<TPanels> && ...),
            "Every debug panel in DebugPanelRegistry needs DebugPanelSpec<T>.");
        static_assert(((DebugPanelSpec<TPanels>::Name.size() > 0U) && ...),
            "Every debug panel needs a non-empty DebugPanelSpec<T>::Name.");
        static_assert(((DebugPanelSpec<TPanels>::Title.size() > 0U) && ...),
            "Every debug panel needs a non-empty DebugPanelSpec<T>::Title.");
        return true;
    }
};

static_assert(DebugPanelRegistryValidator<DebugPanelRegistry>::valid());

namespace detail {

template<typename... TPanels>
consteval std::array<bool, sizeof...(TPanels)> defaultPanelEnabled(::biofuel::typed::Registry<TPanels...>) {
    return {DebugPanelSpec<TPanels>::DefaultEnabled...};
}

} // namespace detail

class DebugOverlayService final {
public:
    DebugOverlayService() noexcept = default;

    void init() noexcept {}
    void shutdown() noexcept { m_panelEnabled = detail::defaultPanelEnabled(DebugPanelRegistry{}); }

    void setEnabled(const bool enabled) noexcept { m_enabled = enabled; }
    void toggle() noexcept { m_enabled = !m_enabled; }
    [[nodiscard]] bool enabled() const noexcept { return m_enabled; }

    template<typename TPanel>
    void setPanelEnabled(const bool enabled) {
        static_assert(RegisteredDebugPanel<TPanel>, "Debug panel must have DebugPanelSpec<T>.");
        static_assert(DebugPanelRegistry::template contains<TPanel>,
            "Debug panel must be listed in DebugPanelRegistry before it can be toggled.");
        m_panelEnabled[DebugPanelRegistry::template index<TPanel>] = enabled;
    }

    template<typename TPanel>
    [[nodiscard]] bool panelEnabled() const {
        static_assert(RegisteredDebugPanel<TPanel>, "Debug panel must have DebugPanelSpec<T>.");
        static_assert(DebugPanelRegistry::template contains<TPanel>,
            "Debug panel must be listed in DebugPanelRegistry before it can be queried.");
        return m_panelEnabled[DebugPanelRegistry::template index<TPanel>];
    }

    void render(const DebugOverlayContext& context) const;

private:
#ifndef NDEBUG
    bool m_enabled = true;
#else
    bool m_enabled = false;
#endif
    std::array<bool, DebugPanelRegistry::size> m_panelEnabled = detail::defaultPanelEnabled(DebugPanelRegistry{});
};

} // namespace biofuel::engine::debug
