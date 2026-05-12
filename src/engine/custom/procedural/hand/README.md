# Procedural Robot Hand Module

This module provides the reusable engine side of the Debug hand lab:

- typed left/right robot-hand specs
- JSON-overridable rig dimensions and material tuning
- FABRIK IK and joint-limit helpers
- typed animation clips and playback controller
- procedural texture generation plus optional PNG texture overrides
- cached Raylib model/mesh resources for primitive hand parts

Game code should keep UI, camera, and tool workflow decisions outside this folder. The hand module should stay reusable and not depend on concrete screens.
