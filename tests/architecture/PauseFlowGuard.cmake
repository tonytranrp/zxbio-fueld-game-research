if(NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "SOURCE_DIR is required")
endif()

function(read_required relative_path out_var)
    set(full_path "${SOURCE_DIR}/${relative_path}")
    if(NOT EXISTS "${full_path}")
        message(FATAL_ERROR "Pause flow guard missing required file: ${relative_path}")
    endif()
    file(READ "${full_path}" contents)
    set(${out_var} "${contents}" PARENT_SCOPE)
endfunction()

function(require_contains label contents needle)
    string(FIND "${contents}" "${needle}" found_index)
    if(found_index EQUAL -1)
        message(FATAL_ERROR "Pause flow guard failed: ${label}")
    endif()
endfunction()

read_required("src/game/app/GameApp.cpp" GAME_APP)
read_required("src/game/screens/pause_popup/PauseController.cpp" PAUSE_CONTROLLER)
read_required("src/game/screens/main_menu/MainMenuScreen.cpp" MAIN_MENU_SCREEN)
read_required("src/game/screens/idle/IdleScreen.cpp" IDLE_SCREEN)
read_required("src/game/screens/video/VideoScreen.cpp" VIDEO_SCREEN)
read_required("src/engine/app/App.cpp" APP_CPP)
read_required("src/engine/ui/ScreenManager.hpp" SCREEN_MANAGER_HEADER)
read_required("src/engine/ui/ScreenManager.cpp" SCREEN_MANAGER_CPP)

if(NOT GAME_APP MATCHES "PauseController::handleGlobalInput")
    message(FATAL_ERROR "Pause flow guard failed: game app must wire global pause input through PauseController")
endif()
if(NOT PAUSE_CONTROLLER MATCHES "queuePush<PausePopupScreen>\\(")
    message(FATAL_ERROR "Pause flow guard failed: PauseController must push PausePopupScreen")
endif()
require_contains(
    "PauseController must reject loading, pause, and transient screens before the true branch"
    "${PAUSE_CONTROLLER}"
    "case Loading:
    case PausePopup:
    case Idle:
    case Video:
    case Unknown:
    case Count:")
require_contains(
    "PauseController true branch must be limited to MainMenu and GamePlay"
    "${PAUSE_CONTROLLER}"
    "case MainMenu:
    case GamePlay:
        return true;")
require_contains(
    "IdleScreen must keep ownership of Escape dismissal"
    "${IDLE_SCREEN}"
    "KEY_ESCAPE")
require_contains(
    "VideoScreen must keep ownership of Escape dismissal"
    "${VIDEO_SCREEN}"
    "KEY_ESCAPE")
if(MAIN_MENU_SCREEN MATCHES "queuePush<PausePopupScreen>\\(")
    message(FATAL_ERROR "Pause flow guard failed: pause activation must not be hard-coded in MainMenuScreen")
endif()
if(NOT APP_CPP MATCHES "blocksUnderlyingUpdates")
    message(FATAL_ERROR "Pause flow guard failed: Application update loop must consult screen stack freeze state")
endif()
if(NOT SCREEN_MANAGER_HEADER MATCHES "blocksUnderlyingUpdates" OR NOT SCREEN_MANAGER_CPP MATCHES "blocksUnderlyingUpdates")
    message(FATAL_ERROR "Pause flow guard failed: ScreenManager must expose underlying-update freeze state")
endif()

message(STATUS "Pause flow guard passed: pause activation is global and underlying updates can freeze.")
