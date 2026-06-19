#include "engine/debug/DebugOverlayService.hpp"
#include "engine/debug/MemoryTelemetry.hpp"
#include "engine/graphics/Render.hpp"
#include "engine/runtime/Runtime.hpp"
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
        return 210;
    } else if constexpr (std::is_same_v<TPanel, PhysicsDebugPanel>) {
        return 70;
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
    constexpr f64 kMiB = 1024.0 * 1024.0;
    const i32 x = static_cast<i32>(rect.x) + 8;
    const i32 panelRight = static_cast<i32>(rect.x + rect.width) - 8;
    i32 y = static_cast<i32>(rect.y) + 26;

    const auto memory = MemoryTelemetry::processMemory();
    drawLine(
        TextFormat(
            "RAM %.1f MiB (resident) | Private %.1f MiB",
            static_cast<double>(memory.workingSetBytes) / kMiB,
            static_cast<double>(memory.privateBytes) / kMiB),
        x,
        y,
        Color{170, 222, 196, 255});
    y += 18;

    // Collect the tracked resource kinds and sort them by their largest byte
    // footprint so the biggest consumers float to the top of the list. Fixed-
    // size storage — no per-frame heap allocation.
    struct Row {
        ResourceKind kind = ResourceKind::Count;
        ResourceStats stats{};
        i64 bytes = 0;
    };
    std::array<Row, static_cast<size_t>(ResourceKind::Count)> rows{};
    size_t rowCount = 0;
    i64 maxBytes = 0;
    for (size_t index = 0; index < static_cast<size_t>(ResourceKind::Count); ++index) {
        const auto kind = static_cast<ResourceKind>(index);
        const auto stats = MemoryTelemetry::stats(kind);
        if (stats.liveCount == 0 && stats.peakCount == 0 && stats.liveBytes == 0 && stats.peakBytes == 0) {
            continue;
        }
        const i64 bytes = std::max<i64>(stats.liveBytes, stats.peakBytes);
        rows[rowCount++] = Row{kind, stats, bytes};
        maxBytes = std::max<i64>(maxBytes, bytes);
    }
    std::sort(rows.begin(), rows.begin() + static_cast<std::ptrdiff_t>(rowCount),
        [](const Row& a, const Row& b) { return a.bytes > b.bytes; });

    if (rowCount == 0) {
        drawLine("(no tracked game resources yet)", x, y, Color{120, 140, 136, 255});
        return;
    }

    for (size_t i = 0; i < rowCount; ++i) {
        const Row& row = rows[i];
        const double liveMiB = static_cast<double>(row.stats.liveBytes) / kMiB;
        const double peakMiB = static_cast<double>(row.stats.peakBytes) / kMiB;
        if (row.bytes > 0) {
            drawLine(
                TextFormat(
                    "%-15s %6.2f MiB  (peak %.2f, x%lld)",
                    MemoryTelemetry::name(row.kind).data(),
                    liveMiB,
                    peakMiB,
                    static_cast<long long>(row.stats.liveCount)),
                x, y, Color{198, 216, 210, 255});
        } else {
            // Count-only resources (e.g. shaders) report no byte footprint yet.
            drawLine(
                TextFormat(
                    "%-15s      -- MiB  (live x%lld, peak x%lld)",
                    MemoryTelemetry::name(row.kind).data(),
                    static_cast<long long>(row.stats.liveCount),
                    static_cast<long long>(row.stats.peakCount)),
                x, y, Color{140, 160, 156, 255});
        }
        y += 13;

        // Proportional usage bar relative to the biggest tracked consumer.
        const i32 barW = std::max(1, panelRight - x);
        Renderer::drawRect(x, y, barW, 3, Color{28, 42, 38, 200});
        if (maxBytes > 0 && row.bytes > 0) {
            const i32 fill = static_cast<i32>(static_cast<i64>(barW) * row.bytes / maxBytes);
            Renderer::drawRect(x, y, fill, 3, Color{96, 200, 150, 235});
        }
        y += 7;

        if (y > static_cast<i32>(rect.y + rect.height) - 10) {
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
