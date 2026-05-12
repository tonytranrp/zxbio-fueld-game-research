#include "engine/ui/typed/RenderContext.hpp"
#include "engine/ui/ScreenManager.hpp"

namespace biofuel::engine::ui::typed {

bool RenderContext::layerEnabled(const std::string_view layerName) const noexcept {
    return manager == nullptr || manager->isLayerEnabled(screenId, layerName);
}

} // namespace biofuel::engine::ui::typed
