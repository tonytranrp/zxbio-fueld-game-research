# Menu Transition Hands

This folder contains the runtime model used during the main-menu dismissal and dimension-shift transition.

## Current runtime asset

- Model: `wrad_arms.glb`
- Source page: [WRAD ARMS - Low Poly FPS Arms (PSX / Half-Life 1 Style)](https://wriks.itch.io/wrad-arms)
- Author: `wriks`
- License: `CC0 / Public Domain`
- Included local license copy: `WRAD_LICENSE.txt`
- Package contents used from source archive: paired first-person rigged arms in `.glb`

## Integration notes

- The runtime now uses a **single paired-arms rigged model** instead of mirroring one hand with negative X scale.
- Finger, wrist, and arm motion are authored in code through the model keyframe runtime in `src/AnimationController/animation/`.
- The old `rigged_hand.glb` asset is kept only as a historical fallback/reference and is no longer the active runtime model.

## Repo policy

If this asset is replaced later, keep the new attribution in this folder, keep a local copy of the source license text when available, and update the registered runtime path in `ModelSystem.cpp`.
