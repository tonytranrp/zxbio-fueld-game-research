#include "engine/core/units/EngineUnits.hpp"
#include "engine/debug/DebugOverlayService.hpp"
#include "engine/runtime/typed/AssetCatalog.hpp"
#include "engine/ui/typed/ScreenCatalog.hpp"
#include <cstdlib>
#include <iostream>
#include <type_traits>

namespace {

bool check(const bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        return false;
    }
    return true;
}

struct CatalogCounter {
    biofuel::usize count = 0U;

    template<typename>
    void operator()() noexcept {
        ++count;
    }
};

} // namespace

int main() {
    using namespace ::biofuel::engine::core::units;
    using namespace ::biofuel::engine::debug;
    using namespace ::biofuel::engine::runtime::typed;
    using namespace ::biofuel::engine::ui::typed;

    static_assert(!std::is_convertible_v<ScreenPixels2D, WorldMeters2D>);
    static_assert(!std::is_convertible_v<WorldMeters2D, ScreenPixels2D>);
    static_assert(!std::is_convertible_v<NormalizedCameraCoord2D, ScreenPixels2D>);
    static_assert(validateAssetCatalog<EngineStartupCatalog>());
    static_assert(validateScreenRegistry<AppScreenRegistry>());

    bool ok = true;

    const PixelToMeterScale scale{.pixelsPerMeter = 40.0f};
    const ScreenPixels2D pixels{120.0f, 80.0f};
    const WorldMeters2D meters = toWorldMeters(pixels, scale);
    const ScreenPixels2D roundTrip = toScreenPixels(meters, scale);
    ok = check(meters.x == 3.0f && meters.y == 2.0f, "pixel-to-meter conversion failed") && ok;
    ok = check(roundTrip.x == pixels.x && roundTrip.y == pixels.y, "meter-to-pixel round trip failed") && ok;

    const TileCoord tile = toTileCoord(ScreenPixels2D{95.0f, 65.0f}, TileSizePixels{.value = 32.0f});
    ok = check(tile.x == 2 && tile.y == 2, "screen-pixel to tile conversion failed") && ok;

    const auto clamped = clampNormalized(NormalizedCameraCoord2D{-0.5f, 1.6f});
    ok = check(clamped.x == 0.0f && clamped.y == 1.0f, "normalized camera clamp failed") && ok;

    ::biofuel::engine::debug::DebugOverlayService overlay;
    overlay.setEnabled(false);
    overlay.setPanelEnabled<FrameTimingDebugPanel>(true);
    overlay.setPanelEnabled<PhysicsDebugPanel>(false);
    ok = check(!overlay.enabled(), "debug overlay global enabled state failed") && ok;
    ok = check(overlay.panelEnabled<FrameTimingDebugPanel>(), "debug panel enabled state failed") && ok;
    ok = check(!overlay.panelEnabled<PhysicsDebugPanel>(), "debug panel disabled state failed") && ok;
    overlay.toggle();
    ok = check(overlay.enabled(), "debug overlay toggle failed") && ok;

    CatalogCounter counter{};
    AssetCatalogView<EngineStartupCatalog>::forEach(counter);
    ok = check(
        counter.count == AssetCatalog<EngineStartupCatalog>::Assets::size,
        "asset catalog iteration count failed") && ok;

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
