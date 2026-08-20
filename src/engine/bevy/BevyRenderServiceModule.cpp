#include "engine/bevy/BevyRenderServiceModule.hpp"
#include "engine/bevy/BevyRenderService.hpp"

namespace biofuel::engine::runtime::typed {

ServiceModule<BevyRendererService>::Backend& ServiceModule<BevyRendererService>::get() {
    return ::biofuel::engine::bevy::BevyRenderService::instance();
}

} // namespace biofuel::engine::runtime::typed
