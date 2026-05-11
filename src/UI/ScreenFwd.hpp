#pragma once

#include <memory>

namespace biofuel::ui {

class Screen;

namespace screens {

// ------------------------------------------------------------------------------
// Screen factory functions — callers get a Screen without #including the
// full screen header. Each is defined in the corresponding screen's .cpp.
//
// Usage:
//   sm->push(ui::screens::makePausePopup());
//   sm->queueReplace(ui::screens::makeMainMenu());
// ------------------------------------------------------------------------------

std::unique_ptr<Screen> makeMainMenu();
std::unique_ptr<Screen> makeLoading(i32 width, i32 height, i32 targetFps);
std::unique_ptr<Screen> makeIdle();
std::unique_ptr<Screen> makePausePopup();
std::unique_ptr<Screen> makeVideoScreen(std::string_view videoName);

} // namespace screens
} // namespace biofuel::ui
