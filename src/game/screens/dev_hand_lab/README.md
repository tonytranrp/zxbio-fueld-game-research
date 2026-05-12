# DevHandLabScreen

Debug-only full-screen screen for procedural robot-hand tuning.

- Opens from the main menu with `Ctrl+H` by replacing the menu screen.
- Returns to a fresh main menu with `ESC`.
- Can be opened after normal loading with `BIOFUEL_DEV_STARTUP_HAND_LAB=ON`.
- Renders only its own clean studio scene; no screen below it is visible.
- Provides Pose, Animation, IK, Materials, and Debug tabs.
- Supports orbit camera controls, fingertip dragging, wrist controls, target visibility, mirror actions, and selectable looping demo clips.
- Uses `engine/custom/procedural` for typed rig, IK, animation, cached mesh rendering, materials, and JSON presets.
- Materials tab can reload `assets/custom/procedural/hand/presets/biofuel_robot_default.json` and export `dev_hand_lab_export.json`.

Release builds do not register this screen.
