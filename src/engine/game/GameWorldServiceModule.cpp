#include "engine/game/GameWorldServiceModule.hpp"
#include "engine/game/GameWorldService.hpp"

namespace biofuel::engine::runtime::typed {

ServiceModule<GameWorldRuntimeService>::Backend& ServiceModule<GameWorldRuntimeService>::get() {
    return ::biofuel::engine::gameworld::GameWorldService::instance();
}

} // namespace biofuel::engine::runtime::typed
