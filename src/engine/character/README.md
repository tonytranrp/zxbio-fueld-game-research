# engine/character

Reusable first-person character building blocks: camera (yaw/pitch/head-bob) and a
physics-backed capsule character controller. Both are plain classes, not typed services.

## Own these as members, not services

`FirstPersonCamera` and `CharacterController3D` are meant to be owned directly by whatever
screen has a first-person character (e.g. `game/screens/exploration/ExplorationScreen`), the
same way `engine/graphics/components/Camera/README.md` already documents for its own
shader-camera component: *"There is no component manager -- own a `CameraComponent` directly
(e.g. as a screen member)."* Same shape as `ScreenBlurEffect`, `ScreenBackdropController`.

Do not wrap either of these in a `BIOFUEL_SERVICE_MODULE`. `BIOFUEL_STATIC_SERVICE` expands to
a process-lifetime singleton (`static Backend backend{}`); a character controller holds Rapier
handles that must die with the screen that owns it, and a singleton would both outlive
`PhysicsService::shutdown()` (leaving stale handles) and make a second character impossible.
`Runtime` already has a dozen accessors and CLAUDE.md is explicit that a second facade is not
wanted -- the one consumer of physics here already has `Runtime::physics()`.

## `FirstPersonCamera`

Owns yaw/pitch/head-bob state and converts it to a raylib `Camera3D` on demand. Deliberately
polls no input itself -- unlike raylib's own `CAMERA_FIRST_PERSON` mode (which reads
`GetMouseDelta()`/keys internally and can't be driven from a decoupled fixed-timestep loop),
the owner reads raw mouse delta once per render frame in `onInput()` and calls
`addLookDelta()`. Movement/physics stay entirely separate and update at the fixed tick rate.

## `CharacterController3D`

Owns the `PhysicsBody3D`/`PhysicsCollider3D` handles for a kinematic capsule and calls
`PhysicsWorld3D::moveCharacter(...)` once per fixed tick. Kinematic, not dynamic, because
`PhysicsBodyDesc3D` does not carry rotation-lock fields all the way to Rapier -- a dynamic
capsule would tip over. See `engine/physics/README.md` for the underlying FFI.

Rapier internals (handles, descriptors, the character-controller FFI call) stay inside
`engine/physics/`; this folder only holds the higher-level per-character logic on top, per
`engine/physics/README.md`'s own guidance to keep input-driven interaction logic outside the
physics service itself.
