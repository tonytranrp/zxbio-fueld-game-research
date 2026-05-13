#include "engine/debug/DebugOverlayService.hpp"
#include "engine/debug/MemoryTelemetry.hpp"
#include "engine/graphics/Render.hpp"
#include "engine/runtime/Runtime.hpp"
#ifdef BIOFUEL_ENABLE_HAND_TRACKING
#include "engine/vision/hand_tracking/HandTrackingTypes.hpp"
#endif
#include <algorithm>
#include <array>
#include <raylib.h>
#include <type_traits>

namespace biofuel::engine::debug {

namespace {

using ::biofuel::engine::graphics::Renderer;

template<typename TPanel>
[[nodiscard]] constexpr i32 panelHeight() noexcept {
    if constexpr (std::is_same_v<TPanel, MemoryTelemetryDebugPanel>) {
        return 138;
    } else if constexpr (std::is_same_v<TPanel, PhysicsDebugPanel>) {
        return 70;
    } else if constexpr (std::is_same_v<TPanel, HandTrackingDebugPanel>) {
        return 86;
    } else if constexpr (std::is_same_v<TPanel, AssetDebugPanel>) {
        return 70;
    } else {
        return 64;
    }
}

void drawLine(const std::string_view text, const i32 x, const i32 y, const Color color) {
    Renderer::drawText(text, x, y, 12, color);
}

void drawPanelChrome(const char* title, const Rectangle rect) {
    Renderer::drawRect(
        static_cast<i32>(rect.x),
        static_cast<i32>(rect.y),
        static_cast<i32>(rect.width),
        static_cast<i32>(rect.height),
        Color{8, 14, 18, 184});
    Renderer::drawRectLines(
        static_cast<i32>(rect.x),
        static_cast<i32>(rect.y),
        static_cast<i32>(rect.width),
        static_cast<i32>(rect.height),
        Color{72, 166, 128, 160});
    Renderer::drawText(title, static_cast<i32>(rect.x) + 8, static_cast<i32>(rect.y) + 6, 13, Color{214, 238, 226, 255});
}

template<typename TPanel>
void renderPanelContent(const DebugOverlayContext&, const Rectangle) {
}

template<>
void renderPanelContent<FrameTimingDebugPanel>(const DebugOverlayContext& context, const Rectangle rect) {
    const i32 x = static_cast<i32>(rect.x) + 8;
    i32 y = static_cast<i32>(rect.y) + 26;
    drawLine(
        TextFormat("Window: %dx%d | FPS: %d", context.screenWidth, context.screenHeight, GetFPS()),
        x,
        y,
        Color{160, 180, 176, 255});
    y += 16;
    drawLine(
        TextFormat("Frame: %.2f ms", static_cast<double>(context.frameTime) * 1000.0),
        x,
        y,
        Color{130, 152, 148, 255});
}

template<>
void renderPanelContent<MemoryTelemetryDebugPanel>(const DebugOverlayContext&, const Rectangle rect) {
    const i32 x = static_cast<i32>(rect.x) + 8;
    i32 y = static_cast<i32>(rect.y) + 26;
    const auto memory = MemoryTelemetry::processMemory();
    drawLine(
        TextFormat(
            "RAM %.1f MiB | Private %.1f MiB",
            static_cast<double>(memory.workingSetBytes) / (1024.0 * 1024.0),
            static_cast<double>(memory.privateBytes) / (1024.0 * 1024.0)),
        x,
        y,
        Color{160, 180, 176, 255});
    y += 16;

    for (size_t index = 0; index < static_cast<size_t>(ResourceKind::Count); ++index) {
        const auto kind = static_cast<ResourceKind>(index);
        const auto stats = MemoryTelemetry::stats(kind);
        if (stats.liveCount == 0 && stats.peakCount == 0 && stats.liveBytes == 0 && stats.peakBytes == 0) {
            continue;
        }
        drawLine(
            TextFormat(
                "%s: live %lld peak %lld",
                MemoryTelemetry::name(kind).data(),
                static_cast<long long>(stats.liveCount),
                static_cast<long long>(stats.peakCount)),
            x,
            y,
            Color{130, 152, 148, 255});
        y += 14;
        if (y > static_cast<i32>(rect.y + rect.height) - 14) {
            break;
        }
    }
}

template<>
void renderPanelContent<PhysicsDebugPanel>(const DebugOverlayContext&, const Rectangle rect) {
    const i32 x = static_cast<i32>(rect.x) + 8;
    i32 y = static_cast<i32>(rect.y) + 26;
    const auto contacts = ::biofuel::engine::runtime::Runtime::physics().recentContacts();
    drawLine(TextFormat("Recent contacts: %zu", contacts.size()), x, y, Color{160, 180, 176, 255});
    y += 16;
    drawLine("Worlds: 2D + 3D Rapier", x, y, Color{130, 152, 148, 255});
}

template<>
void renderPanelContent<HandTrackingDebugPanel>(const DebugOverlayContext&, const Rectangle rect) {
    const i32 x = static_cast<i32>(rect.x) + 8;
    i32 y = static_cast<i32>(rect.y) + 26;
#ifdef BIOFUEL_ENABLE_HAND_TRACKING
    const auto status = ::biofuel::engine::runtime::Runtime::handTracking().status();
    drawLine(
        TextFormat(
            "State: %s | %.1f pps",
            ::biofuel::engine::vision::hand_tracking::toString(status.state).data(),
            static_cast<double>(status.packetsPerSecond)),
        x,
        y,
        Color{160, 180, 176, 255});
    y += 16;
    drawLine(
        TextFormat("Preview: %s | Age: %.2fs", status.previewEnabled ? "on" : "off", static_cast<double>(status.secondsSinceLastFrame)),
        x,
        y,
        Color{130, 152, 148, 255});
#else
    drawLine("Hand tracking disabled in this build.", x, y, Color{160, 180, 176, 255});
#endif
}

template<>
void renderPanelContent<AssetDebugPanel>(const DebugOverlayContext&, const Rectangle rect) {
    const i32 x = static_cast<i32>(rect.x) + 8;
    i32 y = static_cast<i32>(rect.y) + 26;
    const auto models = ::biofuel::engine::runtime::Runtime::model().registry();
    drawLine(TextFormat("Registered models: %zu", models.size()), x, y, Color{160, 180, 176, 255});
    y += 16;
    drawLine("Typed shader/audio/video registries generated.", x, y, Color{130, 152, 148, 255});
}

template<typename TPanel>
void renderPanel(const DebugOverlayService& service, const DebugOverlayContext& context, i32& y) {
    if (!service.panelEnabled<TPanel>()) {
        return;
    }

    constexpr i32 width = 310;
    constexpr i32 gap = 6;
    const i32 height = panelHeight<TPanel>();
    const Rectangle rect{
        14.0f,
        static_cast<f32>(y),
        static_cast<f32>(std::min(width, std::max(180, context.screenWidth - 28))),
        static_cast<f32>(height),
    };
    drawPanelChrome(DebugPanelSpec<TPanel>::Title.data(), rect);
    renderPanelContent<TPanel>(context, rect);
    y += height + gap;
}

template<typename... TPanels>
void renderPanels(const DebugOverlayService& service, const DebugOverlayContext& context, ::biofuel::typed::Registry<TPanels...>) {
    i32 y = 14;
    (renderPanel<TPanels>(service, context, y), ...);
}

} // namespace

void DebugOverlayService::render(const DebugOverlayContext& context) const {
    if (!m_enabled || context.screenWidth <= 0 || context.screenHeight <= 0) {
        return;
    }

    renderPanels(*this, context, DebugPanelRegistry{});
}

} // namespace biofuel::engine::debug
