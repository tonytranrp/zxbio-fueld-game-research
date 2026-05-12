#include "engine/ui/typed/ScreenServiceModule.hpp"
#include "engine/ui/ScreenManager.hpp"

namespace biofuel::engine::runtime::typed {

ServiceModule<ScreenService>::Backend& ServiceModule<ScreenService>::get() {
    return ::biofuel::engine::ui::ScreenManager::instance();
}

} // namespace biofuel::engine::runtime::typed

