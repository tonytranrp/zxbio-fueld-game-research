# game/screens/dev_hand_lab

Debug-only full-screen screen for the live procedural robot-hand view.

## Current contents

```text
game/screens/dev_hand_lab/
|-- DevHandLabScreen.hpp/.cpp
|-- DevHandLabScreenModule.hpp
|-- HandLabTypes.hpp
`-- README.md
```

## Responsibilities

- Opens from the main menu with `Ctrl+H`.
- Returns to a fresh main menu with `ESC`.
- Can start after loading with `BIOFUEL_DEV_STARTUP_HAND_LAB=ON`.
- Renders its own clean studio scene and camera preview.
- Draws mirrored MediaPipe landmark overlays.
- Displays the guided left-hand/right-hand calibration UI.
- Owns camera orbit/zoom tool controls for this dev view.

## Engine boundaries

This screen consumes engine systems; it should not own detector or mapping
logic. The reusable pieces live here:

- `engine/vision/hand_tracking/`: Python worker, IPC, snapshots, preview frames.
- `engine/custom/procedural/pose/`: camera-to-stage calibration and pose math.
- `engine/custom/procedural/hand/`: robot-hand rig, renderer, and retargeter.

## Controls

| Input | Action |
| --- | --- |
| `C` | Restart tracking |
| `V` | Toggle preview |
| `X` | Stop tracking |
| `K` | Restart guided per-hand calibration |
| `RMB` drag | Orbit camera |
| Mouse wheel | Zoom camera |

## Coding standards

- Keep screen code focused on UI workflow and rendering.
- Do not add reusable math here; move it to engine procedural or vision modules.
- Keep debug-only behavior guarded by registration/build flags.
- Prefer typed helper structs in `HandLabTypes.hpp` for screen-local state.
