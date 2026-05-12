# DevHandLabScreen

Debug-only full-screen screen for the live procedural robot-hand view.

- Opens from the main menu with `Ctrl+H` by replacing the menu screen.
- Returns to a fresh main menu with `ESC`.
- Can be opened after normal loading with `BIOFUEL_DEV_STARTUP_HAND_LAB=ON`.
- Renders only its own clean studio scene; no screen below it is visible.
- Uses `engine/custom/procedural` for typed rig, IK, animation, cached mesh rendering, materials, and JSON presets.
- When built with `BIOFUEL_ENABLE_HAND_TRACKING=ON`, the screen automatically
  starts the managed Python tracker, enables the live preview, and retargets
  MediaPipe hand landmarks into the procedural robot hands.
- The live camera overlay draws MediaPipe landmark lines over the preview while
  the same landmarks drive the robot-hand model.
- The robot hand follows wrist/palm translation, finger pose, and approximate
  depth from apparent palm size, with smoothing and soft separation between two
  tracked hands. Retargeting and visible-volume bounds live in
  `engine/custom/procedural/hand/HandTrackingRetarget.hpp`, not in the screen.
- `C` restarts tracking, `V` toggles preview, `X` stops tracking, `K`
  recalibrates the current hand position/depth as neutral, and `RMB` / mouse
  wheel still orbit and zoom the 3D view.

Release builds do not register this screen unless Debug/dev screen registration
is explicitly enabled elsewhere.
