# Menu Transition Hands

This folder contains the rigged runtime hand model used during the main-menu dismissal and dimension-shift transition.

## Current runtime asset

- Model: `rigged_hand.glb`
- Source page: [GetGLB - Rigged Hand](https://www.getglb.com/anatomy/rigged-hand/)
- Direct download: `https://www.get3dmodels.com/download/Rigged-Hand.glb`
- Author listed on source page: `J-Toastie`
- License listed on source page: `CC-BY`
- Reported geometry: `1.1k vertices / 1.5k triangles`

## Integration notes

- The runtime uses this asset as a **single rigged hand** and spawns two typed `ModelInstance`s from `ModelSystem`.
- Left/right presentation is produced by screen-side choreography plus mirrored draw transforms.
- Finger and wrist motion are authored in code through the model keyframe runtime in `src/AnimationController/animation/`.

## Repo policy

If this asset is replaced later, keep the new attribution in this folder and update the registered runtime path in `ModelSystem.cpp`.
