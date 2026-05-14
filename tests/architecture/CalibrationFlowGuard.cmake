if(NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "SOURCE_DIR is required")
endif()

function(read_required relative_path out_var)
    set(full_path "${SOURCE_DIR}/${relative_path}")
    if(NOT EXISTS "${full_path}")
        message(FATAL_ERROR "Calibration flow guard missing required file: ${relative_path}")
    endif()
    file(READ "${full_path}" contents)
    set(${out_var} "${contents}" PARENT_SCOPE)
endfunction()

function(require_contains label contents needle)
    string(FIND "${contents}" "${needle}" found_index)
    if(found_index EQUAL -1)
        message(FATAL_ERROR "Calibration flow guard failed: ${label}")
    endif()
endfunction()

read_required("src/engine/ui/typed/ScreenTypes.hpp" SCREEN_TYPES)
read_required("src/game/screens/GameScreenCatalog.hpp" GAME_SCREEN_CATALOG)
read_required("src/game/screens/main_menu/MainMenuScreen.cpp" MAIN_MENU_SCREEN)
read_required("src/game/screens/main_menu/MainMenuScreen.hpp" MAIN_MENU_HEADER)
read_required("src/game/screens/join/JoinScreen.cpp" JOIN_SCREEN)
read_required("src/game/screens/calibration/CalibrationScreen.hpp" CALIBRATION_HEADER)
read_required("src/game/screens/calibration/CalibrationScreen.cpp" CALIBRATION_SCREEN)
read_required("src/game/screens/calibration/CalibrationScreenModule.hpp" CALIBRATION_MODULE)
read_required("src/game/screens/pause_popup/PauseController.cpp" PAUSE_CONTROLLER)
read_required("src/game/screens/idle/IdleScreen.cpp" IDLE_SCREEN)
read_required("src/game/screens/gameplay/GamePlayScreen.cpp" GAMEPLAY_SCREEN)
read_required("src/game/presentation/hands/HandModelOverlay.cpp" HAND_MODEL_OVERLAY)
read_required("tools/python/biofuel_hand_tracking/default_config.json" HAND_TRACKING_CONFIG)
read_required("src/engine/app/App.cpp" APP_CPP)

require_contains(
    "ScreenId must include Calibration"
    "${SCREEN_TYPES}"
    "Calibration,")
require_contains(
    "Game screen catalog must include CalibrationScreen module"
    "${GAME_SCREEN_CATALOG}"
    "game/screens/calibration/CalibrationScreenModule.hpp")
require_contains(
    "Game screen registry must register CalibrationScreen"
    "${GAME_SCREEN_CATALOG}"
    "CalibrationScreen,")
require_contains(
    "CalibrationScreen must resolve transition policy"
    "${GAME_SCREEN_CATALOG}"
    "typed::TransitionPolicy<CalibrationScreen>::VALUE")
require_contains(
    "CalibrationScreen must resolve stack policy"
    "${GAME_SCREEN_CATALOG}"
    "typed::StackPolicy<CalibrationScreen>::VALUE")

require_contains(
    "Calibration overlay must render over the frozen screen below"
    "${CALIBRATION_MODULE}"
    ".renderBelow = true")
require_contains(
    "Calibration overlay must freeze updates below"
    "${CALIBRATION_MODULE}"
    ".updateBelow = false")
require_contains(
    "Calibration overlay must block input below"
    "${CALIBRATION_MODULE}"
    ".inputBelow = false")

require_contains(
    "MainMenu must begin calibration before routing to Join"
    "${MAIN_MENU_SCREEN}"
    "CalibrationFlowState::instance().begin(CalibrationRoute::Join);")
require_contains(
    "MainMenu must push CalibrationScreen instead of replacing directly while the overlay is active"
    "${MAIN_MENU_SCREEN}"
    "queuePush<CalibrationScreen>()")
require_contains(
    "MainMenu must handle CalibrationScreen pop on resume"
    "${MAIN_MENU_SCREEN}"
    "poppedScreenId == ::biofuel::engine::ui::typed::ScreenId::Calibration")
require_contains(
    "MainMenu must remember successful calibration instead of auto-joining"
    "${MAIN_MENU_SCREEN}"
    "m_handCalibrationReady = calibrationCompleted;")
require_contains(
    "MainMenu must restore the menu after calibration instead of leaving the dismissed overlay state"
    "${MAIN_MENU_SCREEN}"
    "restoreMainMenuAfterCalibration();")
require_contains(
    "MainMenu must route to Join only on a later calibrated menu activation"
    "${MAIN_MENU_SCREEN}"
    "if (m_handCalibrationReady)")
require_contains(
    "MainMenu must own a hand model overlay for the post-calibration shader screen"
    "${MAIN_MENU_HEADER}"
    "HandModelOverlay m_handOverlay")
require_contains(
    "MainMenu must render hand models after calibration"
    "${MAIN_MENU_SCREEN}"
    "m_handOverlay.render();")

if(CALIBRATION_SCREEN MATCHES "HandModelOverlay|RobotHandModule|renderTracked")
    message(FATAL_ERROR "Calibration flow guard failed: CalibrationScreen must not render hand models while calibrating")
endif()
require_contains(
    "CalibrationScreen must show camera preview while calibrating"
    "${CALIBRATION_HEADER}"
    "HandPreviewTexture")
require_contains(
    "CalibrationScreen must disable camera preview after successful calibration"
    "${CALIBRATION_SCREEN}"
    "tracking.setPreviewEnabled(false);")
require_contains(
    "CalibrationScreen must check the HandTrackingService start result"
    "${CALIBRATION_SCREEN}"
    "const bool started = tracking.start();")
require_contains(
    "CalibrationScreen must bound camera startup instead of hanging forever"
    "${CALIBRATION_SCREEN}"
    "CAMERA_START_TIMEOUT_SECONDS")
require_contains(
    "CalibrationScreen failed startup must mark the calibration flow failed"
    "${CALIBRATION_SCREEN}"
    "CalibrationFlowState::instance().fail();")
require_contains(
    "CalibrationScreen must aspect-fit the camera preview instead of stretching it"
    "${CALIBRATION_SCREEN}"
    "cameraContentRect")
require_contains(
    "CalibrationScreen must draw the camera texture into the fitted preview rect"
    "${CALIBRATION_SCREEN}"
    "DrawTexturePro(texture, src, content")
require_contains(
    "Hand tracking preview should prefer a clearer calibration feed"
    "${HAND_TRACKING_CONFIG}"
    "\"max_width\": 960")

require_contains(
    "PauseController must reject CalibrationScreen"
    "${PAUSE_CONTROLLER}"
    "case Calibration:")
require_contains(
    "JoinScreen must render model-only hands when the user reaches the join prompt"
    "${JOIN_SCREEN}"
    "m_handOverlay.render();")
require_contains(
    "IdleScreen must render hand models after calibration"
    "${IDLE_SCREEN}"
    "m_handOverlay.render();")
require_contains(
    "Model-only hand tracking helper must keep the camera preview off"
    "${HAND_MODEL_OVERLAY}"
    "tracking.setPreviewEnabled(false);")
require_contains(
    "GamePlayScreen must render hand models after calibration"
    "${GAMEPLAY_SCREEN}"
    "m_handOverlay.render();")
require_contains(
    "IdleScreen must use the model-only hand tracking helper"
    "${IDLE_SCREEN}"
    "ensureModelOnlyHandTracking();")
require_contains(
    "GamePlayScreen must use the model-only hand tracking helper"
    "${GAMEPLAY_SCREEN}"
    "ensureModelOnlyHandTracking();")
require_contains(
    "Application must pump hand tracking even when overlay screens freeze gameplay updates"
    "${APP_CPP}"
    "services.get<::biofuel::engine::runtime::typed::HandTrackingService>().update(dt);")
require_contains(
    "Application must update persistent hand pose mapping"
    "${APP_CPP}"
    "services.get<::biofuel::engine::runtime::typed::HandPoseService>().update(dt);")

message(STATUS "Calibration flow guard passed: MainMenu calibrates first, overlay freezes below, and Idle/GamePlay render model-only hands.")
