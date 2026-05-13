#pragma once

#include "engine/core/Types.hpp"
#include "engine/core/typed/Meta.hpp"
#include <concepts>
#include <string>
#include <string_view>
#include <unordered_map>

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
struct HandTrackingDebugPanel : DebugPanelTag {};
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
    static constexpr bool DefaultEnabled = true;
};

template<>
struct DebugPanelSpec<MemoryTelemetryDebugPanel> {
    using Panel = MemoryTelemetryDebugPanel;
    static constexpr std::string_view Name = "debug.memory";
    static constexpr std::string_view Title = "Memory";
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
struct DebugPanelSpec<HandTrackingDebugPanel> {
    using Panel = HandTrackingDebugPanel;
    static constexpr std::string_view Name = "debug.hand_tracking";
    static constexpr std::string_view Title = "Hand Tracking";
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
    HandTrackingDebugPanel,
    AssetDebugPanel>;

template<typename TRegistry>
struct DebugPanelRegistryValidator;

template<typename... TPanels>
struct DebugPanelRegistryValidator<::biofuel::typed::Registry<TPanels...>> {
    static consteval bool valid() {
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

class DebugOverlayService final {
public:
    DebugOverlayService() noexcept = default;

    void init() noexcept {}
    void shutdown() noexcept { m_panelEnabled.clear(); }

    void setEnabled(const bool enabled) noexcept { m_enabled = enabled; }
    void toggle() noexcept { m_enabled = !m_enabled; }
    [[nodiscard]] bool enabled() const noexcept { return m_enabled; }

    template<typename TPanel>
    void setPanelEnabled(const bool enabled) {
        static_assert(RegisteredDebugPanel<TPanel>, "Debug panel must have DebugPanelSpec<T>.");
        m_panelEnabled[std::string(DebugPanelSpec<TPanel>::Name)] = enabled;
    }

    template<typename TPanel>
    [[nodiscard]] bool panelEnabled() const {
        static_assert(RegisteredDebugPanel<TPanel>, "Debug panel must have DebugPanelSpec<T>.");
        const auto found = m_panelEnabled.find(std::string(DebugPanelSpec<TPanel>::Name));
        return found == m_panelEnabled.end() ? DebugPanelSpec<TPanel>::DefaultEnabled : found->second;
    }

    void render(const DebugOverlayContext& context) const;

private:
#ifndef NDEBUG
    bool m_enabled = true;
#else
    bool m_enabled = false;
#endif
    std::unordered_map<std::string, bool, ::biofuel::TransparentHash, std::equal_to<>> m_panelEnabled;
};

} // namespace biofuel::engine::debug
