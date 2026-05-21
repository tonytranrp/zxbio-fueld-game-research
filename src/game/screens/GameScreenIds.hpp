#pragma once

#include "engine/ui/typed/ScreenTypes.hpp"

namespace biofuel::game::screens::screen_id {

inline constexpr auto Loading = ::biofuel::engine::ui::typed::ScreenId::Slot0;
inline constexpr auto MainMenu = ::biofuel::engine::ui::typed::ScreenId::Slot1;
inline constexpr auto Join = ::biofuel::engine::ui::typed::ScreenId::Slot2;
inline constexpr auto GamePlay = ::biofuel::engine::ui::typed::ScreenId::Slot3;
inline constexpr auto PausePopup = ::biofuel::engine::ui::typed::ScreenId::Slot4;
inline constexpr auto Calibration = ::biofuel::engine::ui::typed::ScreenId::Slot5;
inline constexpr auto Idle = ::biofuel::engine::ui::typed::ScreenId::Slot6;
inline constexpr auto Video = ::biofuel::engine::ui::typed::ScreenId::Slot7;

} // namespace biofuel::game::screens::screen_id
