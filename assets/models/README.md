# assets/models

This folder stores authored runtime model assets for the game.

## Runtime format policy

- `.glb` is the standard runtime format for this repo.
- Other Raylib-supported model formats are allowed when needed.
- Procedural geometry (e.g. runtime-generated voxel meshes) does not belong here.

## Folder rules

- Each runtime model lives in its own subfolder.
- That subfolder should contain the model file and a local `README.md`.
- The local `README.md` should record attribution, source URL, license, and preprocessing notes.

Example:

```text
assets/models/
`-- harvester_popout/
    |-- harvester.glb
    `-- README.md
```

## Registration rule

Runtime models must be registered in `src/engine/models/ModelSystem.cpp` through the typed model registry. The registry owns debug name, model path, optional shader pairing, preload behavior, animation metadata, and optional keyframe clip factories.
