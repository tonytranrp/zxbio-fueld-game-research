# Verified Bug and Code Quality Findings

Obsidian links: [[Project Hub]] | [[Notes/Source Map]] | [[Notes/Build and Verification]]

Audit date: 2026-05-12

Scope audited: `src/`, `tools/python/biofuel_hand_tracking/`, `cmake/`, `tests/`, and source-adjacent README files. Generated Cargo output under `src/engine/physics/rapier_bridge/target/` was excluded from source findings.

Original verification notes:

- `cargo test --manifest-path src\engine\physics\rapier_bridge\Cargo.toml --locked` passed.
- `python -m compileall tools\python\biofuel_hand_tracking` passed.
- `cmake --build build --config Debug --parallel` passed.
- `ctest --test-dir build -C Debug --output-on-failure` passed.
- `cmake --build out\build\x64-Debug --config Debug --parallel` passed when run through the Visual Studio developer environment.
- `ctest --test-dir out\build\x64-Debug -C Debug --output-on-failure` passed.
- `cargo clippy --manifest-path src\engine\physics\rapier_bridge\Cargo.toml --locked -- -D warnings` failed on the Rust bridge style issue listed as B022.

Resolution notes:

- Fixed B001-B026 on 2026-05-12.
- `cargo test --manifest-path src\engine\physics\rapier_bridge\Cargo.toml --locked` passed.
- `cargo clippy --manifest-path src\engine\physics\rapier_bridge\Cargo.toml --locked -- -D warnings` passed.
- `python -m compileall tools\python\biofuel_hand_tracking` passed.
- `cmake --build build --config Debug --parallel` passed.
- `ctest --test-dir build -C Debug --output-on-failure` passed.
- `cmake --build out\build\x64-Debug --config Debug --parallel` passed when run through the Visual Studio developer environment.
- `ctest --test-dir out\build\x64-Debug -C Debug --output-on-failure` passed.

## B001 - Animation callbacks can use destroyed screen objects during shutdown [Solved]

Severity: P1

Evidence:

- `src/engine/app/App.cpp` shuts down `ScreenService` before `AnimationService`.
- `src/engine/animation/AnimationManager.cpp:18` calls `cancelAll()` during animation shutdown.
- `src/engine/animation/AnimationManager.cpp:50-54` invokes each animation's cancel callback.
- `src/game/screens/pause_popup/PausePopupScreen.cpp:318-325` and `src/game/screens/pause_popup/PausePopupScreen.cpp:351-355` register callbacks that capture `this`.

Description:

If the app closes while the pause popup slide animation is still active, `ScreenManager::shutdown()` destroys the screen first. After that, `AnimationManager::shutdown()` cancels remaining animations and invokes callbacks that still capture the destroyed `PausePopupScreen`. This is a use-after-free risk that can crash on shutdown or corrupt screen state.

## B002 - AnimationManager is not safe against callback reentrancy [Solved]

Severity: P1

Evidence:

- `src/engine/animation/AnimationManager.cpp:23-27` range-iterates `m_animations` while calling `anim->update(dt)`.
- `src/engine/animation/AnimationManager.cpp:50-63` similarly iterates while calling cancel callbacks.
- `src/engine/animation/Animation.hpp:154-170` allows user callbacks to run during update/complete.

Description:

Animation callbacks can call back into `Runtime::animation().add()`, `cancelAll()`, or other animation-manager methods. That mutates `m_animations` while it is being iterated, which can invalidate iterators and produce skipped animations, double cancellation, or memory corruption. This is a manager-level bug because the public callback API makes reentrant calls possible.

## B003 - Animation easing can be set to null and later dereferenced [Solved]

Severity: P2

Evidence:

- `src/engine/animation/Animation.hpp:151` calls `m_easing(rawT)`.
- `src/engine/animation/Animation.hpp:210` assigns any incoming function pointer without validation.

Description:

`Animation::setEasing(nullptr)` is accepted, but the next update dereferences the null function pointer. This can crash any screen or system using a dynamically configured easing function.

## B004 - Color animation can wrap channels when easing overshoots [Solved]

Severity: P2

Evidence:

- `src/engine/animation/Animation.hpp:57-64` casts interpolated channel values directly to `u8`.
- Back/elastic easing functions can produce values outside `[0, 1]`.

Description:

When a `Color` animation uses an overshooting easing curve, channel values can go below `0` or above `255`. The direct `u8` cast wraps instead of clamping, so a fade or color pulse can flash unexpected colors or alpha values.

## B005 - Initial two-hand calibration can lose per-hand profiles [Solved]

Severity: P1

Evidence:

- `src/game/screens/dev_hand_lab/DevHandLabScreen.cpp:231-234` starts calibration during screen entry/reset before a camera frame is guaranteed.
- `src/engine/custom/procedural/hand/HandTrackingRetarget.hpp:184-191` copies the current `m_frameSpace` into the left/right calibration profiles.
- `src/engine/custom/procedural/hand/HandTrackingRetarget.hpp:153-175` updates only the combined calibration frame space on the first real camera frame and returns early when the session was not started yet.
- `src/engine/custom/procedural/hand/HandTrackingRetarget.hpp:763-766` falls back to the combined calibration when a side profile has invalid frame space.

Description:

The first calibration is often started before `m_frameSpace` has a valid camera size. When the first camera frame arrives, `beginSession()` updates `m_frameSpace` and `m_calibration.frameSpace`, but returns before updating `m_leftCalibration.profile.frameSpace` and `m_rightCalibration.profile.frameSpace`. The left/right profiles can complete with invalid frame-space metadata, so mapping falls back to the combined profile and the separate two-hand calibration is effectively not used.

## B006 - Dev hand lab disables adaptive two-hand stage lanes [Solved]

Severity: P2

Evidence:

- `src/engine/custom/procedural/hand/HandTrackingRetarget.hpp:19` defaults `layoutPolicy` to `StageLayoutPolicy::Shared`.
- `src/game/screens/dev_hand_lab/DevHandLabScreen.cpp:208-212` explicitly passes `StageLayoutPolicy::Shared`.

Description:

The mapper has adaptive/fixed lane support, but the main live test screen forces both hands into the shared full stage. When both hands are visible, the renderer does not reserve a separate left/right volume, so model separation depends mostly on the tiny post-separation distance. This can make two tracked hands overlap, swap visual space, or fail to reflect the user's real left/right spacing.

## B007 - Preview JPEG decoding and texture upload run on the main frame path [Solved]

Severity: P2

Evidence:

- `src/game/screens/dev_hand_lab/DevHandLabScreen.cpp:201` calls `updatePreviewTexture()` every hand-lab update.
- `src/game/screens/dev_hand_lab/DevHandLabScreen.cpp:246-257` decodes JPEG bytes and creates a new GPU texture for every new preview sequence.

Description:

When preview is enabled, every new MJPEG frame can trigger `LoadImageFromMemory()` and `LoadTextureFromImage()` on the main thread. At webcam rates this can add repeated CPU decode work and GPU allocation/upload work to the interactive hand lab frame, causing stutter exactly while hand tracking needs low latency.

## B008 - Hand-tracking startup leaks partial runtime state on failure [Solved]

Severity: P1

Evidence:

- `src/engine/vision/hand_tracking/HandTrackingService.cpp:297` starts the UDP receiver before the worker process and control connection are fully verified.
- `src/engine/vision/hand_tracking/HandTrackingService.cpp:298-305` returns `false` on worker/control failure without stopping the receiver or worker process.
- `src/engine/vision/hand_tracking/HandTrackingService.cpp:617-686` can start the worker process successfully before the later control connection fails.

Description:

If the Python worker starts but the control port never responds, or if worker setup fails after the receiver starts, `start()` reports failure while leaving background resources alive. This can leave UDP/TCP ports occupied, an orphan worker process running, or a receiver thread waiting until a later full shutdown.

## B009 - Camera config is ignored because C++ always sends camera index 0 [Solved]

Severity: P2

Evidence:

- `src/engine/vision/hand_tracking/HandTrackingService.cpp:165-172` emits control JSON with `"camera_index":0`.
- `tools/python/biofuel_hand_tracking/worker.py:192` only uses the config camera index when the command does not provide `camera_index`.

Description:

The Python config supports choosing a camera index, but the C++ control command always overrides it with `0`. Users with an external webcam or a non-default camera cannot select the intended device through `default_config.json`.

## B010 - Worker camera failure can leave C++ stuck in Starting state forever [Solved]

Severity: P1

Evidence:

- `tools/python/biofuel_hand_tracking/worker.py:194-198` clears `tracking_event` and continues when `cv2.VideoCapture` cannot open.
- `src/engine/vision/hand_tracking/HandTrackingService.cpp:226-236` only marks tracking offline when `m_latestFrame` already exists.

Description:

If the worker starts but the camera cannot open, no UDP frames are ever sent. The C++ side has no first frame, so its stale-frame timeout never runs and the status can remain `Starting`/worker-running indefinitely. The UI then waits for tracking that will never become online or produce a clear error.

## B011 - Windows hand-tracking process launch breaks non-ASCII paths [Solved]

Severity: P2

Evidence:

- `src/engine/vision/hand_tracking/HandTrackingService.cpp:644-653` builds a UTF-8 command string.
- `src/engine/vision/hand_tracking/HandTrackingService.cpp:655-660` converts it to `std::wstring` by widening bytes.

Description:

The process command uses byte widening instead of UTF-8 to UTF-16 conversion. On Windows, project paths, user names, or asset paths containing non-ASCII characters can produce an invalid command line, causing the Python worker launch to fail even though the files exist.

## B012 - Video pause discards video while audio keeps buffering [Solved]

Severity: P1

Evidence:

- `src/engine/video/VideoManager.cpp:368-379` only flips `m_paused` and pauses/resumes the Raylib audio stream.
- `src/engine/video/VideoManager.cpp:449-466` keeps reading decoded video while paused and discards those frames.
- `src/engine/video/VideoManager.cpp:477-494` keeps reading and queueing audio chunks while paused.

Description:

Pausing playback does not pause the ffmpeg decode processes. Video frames are consumed and thrown away, while audio chunks continue filling the queue. On resume, video has advanced to a later point and audio data can be stale, causing A/V desync and unnecessary CPU/memory work during pause.

## B013 - Video-only MP4 files fail prebuffering [Solved]

Severity: P2

Evidence:

- `src/engine/video/VideoManager.cpp:320-328` starts a separate audio decode process for every video.
- `src/engine/video/VideoManager.cpp:546-558` requires both video frames and audio chunks before playback can start.

Description:

A valid MP4 with no audio track, or an audio decode failure on an otherwise playable video, cannot satisfy `MIN_AUDIO_PREFILL_CHUNKS`. The video times out during prebuffering and is marked failed instead of playing silently.

## B014 - Video backend is Windows-only despite desktop platform target [Solved]

Severity: P2

Evidence:

- `src/engine/video/VideoManager.cpp:600-616` returns errors for all non-Windows builds.
- Project docs and CMake still target Windows/Linux/macOS desktop.

Description:

Linux and macOS builds compile, but any screen depending on `VideoManager` playback cannot load or play videos. Idle video and video-screen behavior fall back or fail on those platforms, which contradicts the repo's stated desktop support.

## B015 - Audio load failures are cached as valid assets [Solved]

Severity: P1

Evidence:

- `src/engine/audio/AudioManager.cpp:48-58` stores the result of `LoadSound()` without validating the returned handle.
- `src/engine/audio/AudioManager.cpp:116-125` stores the result of `LoadMusicStream()` without validating the returned stream.

Description:

Missing or corrupt audio files are inserted into `m_sounds`/`m_musicTracks` and are treated as present by `hasSound()` or `hasMusic()`. Later play/update/unload calls can operate on invalid Raylib handles, and telemetry records assets that did not actually load.

## B016 - Master volume is attenuated after mute/unmute [Solved]

Severity: P1

Evidence:

- `src/engine/audio/AudioManager.cpp:214-218` applies `m_masterVolume * m_mutedVolume`.
- `src/engine/audio/AudioManager.cpp:239-249` stores `m_masterVolume` into `m_mutedVolume`, then unmute applies the product again.

Description:

At master volume `0.8`, muting records `m_mutedVolume = 0.8`; unmuting sets Raylib master volume to `0.8 * 0.8 = 0.64`. Repeated mute/unmute or volume changes around mute can make the actual output quieter than the manager's reported master volume.

## B017 - Per-play sound volume permanently changes the loaded sound [Solved]

Severity: P2

Evidence:

- `src/engine/audio/AudioManager.cpp:100-105` calls `SetSoundVolume()` on the stored `Sound` before playback.
- `src/engine/audio/AudioManager.cpp:221-225` only reapplies global SFX volume when `setSfxVolume()` is called.

Description:

`playSoundAtVolume()` is named like a one-shot override, but it mutates the cached Raylib `Sound`. Future `playSound()` calls reuse that last ad-hoc volume until something else reapplies global SFX volume, so one quiet or loud effect can accidentally affect all later plays of the same asset.

## B018 - Music volume does not apply to tracks started later [Solved]

Severity: P2

Evidence:

- `src/engine/audio/AudioManager.cpp:228-232` applies `m_musicVolume` only to the current track.
- `src/engine/audio/AudioManager.cpp:170-189` starts a new music stream without applying the stored music volume.

Description:

If the player changes music volume before music starts, or switches to another loaded track later, the newly played stream can use Raylib's default stream volume instead of `m_musicVolume`. The UI setting and actual playback volume can disagree.

## B019 - Main-menu idle timer misses keyboard activity [Solved]

Severity: P2

Evidence:

- `src/engine/app/App.cpp:135-139` polls the input service before screen input.
- `src/engine/input/InputSystem.cpp:8-13` drains all `GetKeyPressed()` events.
- `src/game/screens/main_menu/MainMenuScreen.cpp:331` checks `GetKeyPressed()` again to reset idle.

Description:

The input service consumes the key queue before `MainMenuScreen::onInput()` runs. Keyboard activity that does not also hit a screen-specific `IsKeyPressed()` path may fail to reset the idle timer, so the idle video transition can trigger while the user is pressing keys.

## B020 - Menu navigation helpers are unsafe for empty menus [Solved]

Severity: P2

Evidence:

- `src/game/presentation/widgets/MenuHelper.cpp:283-285` and `src/game/presentation/widgets/MenuHelper.cpp:360-362` return activation success when `items.empty()`.
- `src/game/presentation/widgets/MenuHelper.cpp:309-310` and `src/game/presentation/widgets/MenuHelper.cpp:386-387` perform modulo/wrap math using `itemCount` in the empty-items path.

Description:

The helper API accepts empty spans, but pressing activation on an empty menu returns `true`, and pressing navigation can divide by zero when `itemCount == 0`. Current static call sites use non-empty menus, but the reusable helper is unsafe for future data-driven menus.

## B021 - Model actions with zero resolved duration never return [Solved]

Severity: P2

Evidence:

- `src/game/models/ModelSystem.cpp:93-105` resolves duration to `0.0f` when a state has no explicit duration and no valid clip.
- `src/game/models/ModelSystem.cpp:131-139` only processes return/default transitions when `duration > 0.0f`.

Description:

A non-looping model state with no clip and no explicit `durationSeconds` never reaches its return/default state. `playAction()` can schedule a return, but update will not execute it for zero-duration states, leaving the model stuck in the action state.

## B022 - Rust bridge fails strict Clippy checks [Solved]

Severity: P3

Evidence:

- `src/engine/physics/rapier_bridge/src/lib.rs:305` uses `KIND_FIXED | _`.
- `src/engine/physics/rapier_bridge/src/lib.rs:314` uses `KIND_FIXED | _`.
- `cargo clippy --manifest-path src\engine\physics\rapier_bridge\Cargo.toml --locked -- -D warnings` fails with `clippy::wildcard_in_or_patterns`.

Description:

The Rust code compiles and tests pass, but strict linting fails. This is a coding-standards issue for the new Rust engine layer: CI or local workflows that run Clippy with warnings-as-errors will reject the bridge.

## B023 - Physics readback API has no batch path [Solved]

Severity: P3

Evidence:

- `src/engine/physics/PhysicsSystem.hpp` exposes only per-body `bodyPose()` calls for pose readback.
- `src/engine/physics/PhysicsSystem.cpp:78-87` and `src/engine/physics/PhysicsSystem.cpp:185-194` cross the CXX bridge for one body at a time.

Description:

The integration plan calls out avoiding per-entity bridge calls during rendering and syncing poses in batches where useful. The current v1 API has no batch readback path, so gameplay with many bodies will naturally perform one C++/Rust bridge call per entity per frame. This is not visible in the smoke test, but it is a performance limitation in the engine-facing API.

## B024 - Hand-tracking Python runtime uses an unpinned `uv` installer [Solved]

Severity: P3

Evidence:

- `cmake/SetupHandTrackingPython.cmake:8` sets `UV_VERSION` to `latest`.
- `cmake/SetupHandTrackingPython.cmake:19`, `:28`, and `:37` download from the moving GitHub `latest` release URL.

Description:

The Python worker dependencies are pinned through requirements, but the tool used to create/install the environment is not pinned. A future `uv` release can change behavior and break reproducible builds even when the source tree has not changed.

## B025 - Cargo target output lives under `src/` [Solved]

Severity: P3

Evidence:

- Running Cargo in `src/engine/physics/rapier_bridge/` creates `src/engine/physics/rapier_bridge/target/`.
- `.gitignore` hides this directory from Git, but source tools such as `rg --files src` still traverse it unless manually excluded.

Description:

The generated Cargo target directory sits inside the source tree. This does not affect commits because it is ignored, but it slows source scans, makes audits noisy, and can confuse tooling that treats everything under `src/` as project source.

## B026 - Model README references a non-existent asset id [Solved]

Severity: P3

Evidence:

- `src/game/models/ModelSystem.hpp` defines `enum class ModelAssetId : u32 { };`.
- `src/game/models/README.md` shows `ModelAssetId::Harvester`.

Description:

The README example cannot compile against the current model registry. This is a documentation bug that makes the folder-level standards/example less reliable for adding the next model asset.

---

Audit date: 2026-08-16 (professional-codebase cleanup pass)

Scope audited: the full `src/`, `cmake/`, `tests/`, and documentation tree, via 15 parallel deep-research
passes (one per subsystem) plus 5 rounds of direct fix application, run against the repo as it stood
after adopting an existing branch's cleanup work via fast-forward merge and deleting the confirmed-dead
old terrain/world subsystem (~2000 lines: `WorldSystem`, `WorldManager`, `HeightmapWorld3D`,
`TerrainGenerator`, `VoxelChunkRenderer`, `Terrain3D`).

Verification notes:

- `cmake --build build --config Debug --parallel` passed, 0 warnings.
- `ctest -C Debug --output-on-failure` — 21/21 passed.
- `cargo test --manifest-path src/engine/physics/rapier_bridge/Cargo.toml --locked` — 4/4 passed.
- `cargo clippy --manifest-path src/engine/physics/rapier_bridge/Cargo.toml --locked -- -D warnings` passed.
- Net change: 84 tracked files modified/deleted, 2 new files, +1112/-2572 lines.

## B027 - ModelSystem could use-after-free during shutdown [Solved]

Severity: P1

Evidence:

- `src/engine/models/ModelSystem.hpp:66-79` — `SharedAssetData` (Raylib model + animation resources).
- `ModelInstance` objects hold `shared_ptr<const SharedAssetData>` and can outlive `ModelSystem::shutdown()`.

Description:

`ModelSystem` previously had its own `unloadAsset()` path that could free a `SharedAssetData`'s Raylib
resources while a live `ModelInstance` still held a `shared_ptr` to it, spanning a shutdown boundary.
Fixed by making `SharedAssetData`'s own destructor the sole lifetime authority (it now unloads its
Raylib resources itself, `ModelSystem.hpp:67-72`), so the `shared_ptr` refcount — not an explicit unload
call — decides when resources actually free. `unloadAsset()` was deleted as redundant/unsafe.

## B028 - Physics collider-group membership was never purged on body destruction [Solved]

Severity: P2

Evidence:

- `src/engine/physics/PhysicsSystem.cpp:58-59` — `bodyColliders2D`/`bodyColliders3D` reverse-lookup maps.
- `src/engine/physics/PhysicsSystem.cpp:639` — `trackBodyCollider()`.

Description:

Collider handles were tracked per body at creation (`trackBodyCollider`, called from every collider-add
path) but nothing removed them when the owning body was destroyed, leaking stale handles into the
collider-group bookkeeping. Fixed by adding the reverse-lookup maps so body destruction can now purge
every collider it owned.

## B029 - TaskManager result history grew without bound [Solved]

Severity: P2

Evidence:

- `src/engine/tasks/TaskManager.cpp:68` — eviction loop.
- `src/engine/tasks/TaskManager.cpp:103` — `kHistoryCap = 256`.

Description:

Completed-task records accumulated for the lifetime of the process with no eviction, a slow unbounded
memory-growth path for any long play session that schedules many background tasks. Fixed with a
256-record cap and eviction of the oldest entries once exceeded.

## B030 - Rapier contact-force events were never captured despite being requested [Solved]

Severity: P2

Evidence:

- `src/engine/physics/rapier_bridge/src/lib.rs:228-233` — `contact_force_event_count_2d/3d`,
  `contact_force_event_2d/3d`, `clear_contact_force_events_2d/3d` cxx-bridge exports.
- `src/engine/physics/rapier_bridge/src/lib.rs:369,388` — `contact_force_events: Receiver<...>` fields.

Description:

Rapier only emits `ContactForceEvent`s for colliders whose `ActiveEvents` bitflag includes
`CONTACT_FORCE_EVENTS`; the collider builders never set it, so the feature was silently a no-op
regardless of any C++-side code written against it. Fixed by adding the flag to collider construction
and implementing real event storage/drain mirroring the existing `drained_contacts` pattern. No C++
consumer exists yet (tracked as a known limitation below, not a bug) — this fix makes the Rust-side
plumbing correct and ready for one.

## B031 - Shader build-time validation would fail on every shader if the Vulkan SDK were installed [Solved]

Severity: P2

Evidence:

- `src/CMakeLists.txt` (shader-validation block, ~line 239) — `glslc --target-env=opengl` invocation.
- glslc refuses a `.glsl` extension without an explicit `-fshader-stage=`; every shader in
  `assets/shaders/` uses a literal `.glsl` extension.

Description:

The project's own build message told a developer to "Install Vulkan SDK for shader validation," but
doing so would have made every subsequent build fail — glslc errors on `.glsl` files with no stage
flag, and none was passed. Nobody had hit this yet because nobody had glslc installed. Fixed by adding
`-fshader-stage=fragment` (all 8 shaders confirmed fragment shaders) and a `glslangValidator -S frag`
fallback for machines without the full Vulkan SDK. Also closed a coverage gap: `raymarched_voxels.glsl`
(loaded from disk at runtime, not text-embedded) was never validated even when glslc was present; it
now has its own validation entry, kept separate from `SHADER_NAMES` so it isn't wrongly text-embedded.

## B032 - Window could stick to the cursor if drag capture was stolen mid-drag [Solved]

Severity: P3

Evidence:

- `src/engine/window/DragHandler.hpp:52` — `WM_CAPTURECHANGED` constant (added).
- `src/engine/window/DragHandler.hpp:138-144` — new handler.

Description:

`DragHandler`'s custom window-drag subclass ended a drag on `WM_LBUTTONUP`. If Windows stole mouse
capture mid-drag for any OS-level reason, no `WM_LBUTTONUP` would ever arrive, so the dragging flag
never reset and every subsequent `WM_MOUSEMOVE` kept moving the window — it would stick to the cursor
until a full click. Fixed by handling `WM_CAPTURECHANGED` the same way as button-up, without
re-releasing capture that's already gone, and still forwarding the message to the original wndproc.

## B033 - Engine typed-event manifest briefly included a game-layer header, breaking the engine/game compile boundary [Solved]

Severity: P1 (self-caught same session, never reached a committed state)

Evidence:

- `src/CMakeLists.txt` — `ENGINE_TYPED_MODULE_HEADERS` (engine-scoped only, by design).
- `src/CMakeLists.txt:137-142` — `biofuel_engine`'s `target_include_directories` deliberately does not
  expose `src/game/`; only an `engine-include-root/engine/` junction is visible to it.

Description:

While closing what looked like a live event-registration gap for `game/gameplay/FutureEventModule.hpp`,
it was added to the engine-scoped typed-module manifest. The generated registry header is compiled as
part of `biofuel_engine`, which structurally cannot see `game/` headers — this is the project's own
one-way engine→game dependency boundary, enforced at compile time via a scoped include-root junction,
not an oversight. The build failed immediately (`C1083: Cannot open include file`) on re-verification.
Root-caused and reverted in the same session before landing in a green build. Re-examining the original
finding: it was also overstated — `Events::publish<T>()`'s compile-time gate only requires
`EventSpec<T>` to exist (`src/engine/runtime/typed/Events.hpp:13-16`), not registry membership, so the
event in question already worked correctly via the raw `entt::dispatcher` regardless of manifest
inclusion. Logged here because the same "grep before you delete/rename" discipline applies in
reverse — verify an architectural boundary before crossing it, not just before removing something.

---

## Known limitations found this session, NOT fixed (flagged for a future decision)

These are real, well-evidenced findings from the 2026-08-16 research pass that were deliberately left
alone rather than fixed unilaterally, either because they require a design decision this log shouldn't
make on the project owner's behalf, or because the affected subsystem isn't wired into the live game
yet. Not marked `[Solved]` — nothing below should be read as fixed.

**Physics: `CollisionGroup` never reaches Rapier's `InteractionGroups`.** Colliders configured with
"disjoint" collision groups still physically collide with each other today — only the C++-side contact
*event* reporting is filtered by group, not the actual physics response. This is the most likely source
of a real, currently-live gameplay physics bug if collision groups are relied on anywhere for gameplay
logic. Separately, `CollisionGroup::groupOnly(g)` (`src/engine/physics/PhysicsTypes.hpp`) returns a mask
of `0` (collides with nothing), contradicting its own name. Not fixed because the correct membership/
filter bit semantics to pass into Rapier is a design decision, not a mechanical bug fix.

**Gameplay pipeline system (`FarmState`/`TurnPipeline`/`HarvestPipeline`/etc.) is not wired into
`GamePlayScreen` and is not production-ready if it were.** Confirmed real bugs in `src/game/gameplay/`:
the harvest pipeline computes `fuelGallons`/`revenueCents` but never credits them to `FarmState`
(`UpdateInventory.cpp`), making it economically a no-op; the entire `PipelineEventObserver` event
bridge is dead code because no gameplay stage defines `stage_name()`, so Pipeline-c- generates numeric
stage keys ("0","1","2"...) that never match anything the observer looks for; `FuelProcessPipeline`'s
inner sub-engine doesn't forward the parent observer either, so its stages' events are doubly dead; and
`bakeTileColliders` skips any tile with `buildingId >= 0`, but `setTileType(Built)` sets `buildingId=0`,
so `SampleFarm`'s solid perimeter border silently gets zero physics colliders. Domain data (crop yields,
BTU/gallon) was cross-checked against `Research/` and is accurate. Not fixed because wiring this system
up requires deciding how `FarmState` should expose mutation (public setters vs. a delta the caller
applies) — a design call, and because none of this runs in the shipped game today.

**No top-level `LICENSE` file for this project's own code.** `THIRD-PARTY-NOTICES.md` (added this
session) covers dependency attribution — every dependency is permissive and commercial-distribution-
safe — but the project's own license is undecided. That's the project owner's call.

**Typed-registry codegen is the one over-complex piece of an otherwise reasonable architecture.** The
service-locator (`Runtime::service<T>()`) and screen-stack patterns match real shipped engines. The
CMake-regex-scans-C++-macros registration mechanism recreates what Qt/Unreal do with genuine automatic
header discovery, but without that, inherits real fragility — B033 above is a direct instance of that
fragility. A hand-written single X-macro list header (pure C++, no CMake regex parsing) was the research
pass's concrete recommendation; not attempted this session as it's a larger structural change.

**Rust bridge:** 19 `pub fn`s (Force/Impulse/AngularVelocity/SolverGroup/PhysicsShapeRole/JointType-
tuning) have no C++ caller anywhere; `JointType::Spherical` is invalid for 2D per Rapier's own feature
gating but the shared enum lets 2D code specify it anyway; no radian-to-degree conversion point exists
yet (nothing renders physics rotation currently). All pre-existing and explicitly left unaddressed per
this session's own scoping decision, not newly discovered.

**`VideoScreen::preloadVideo`** is an unused arbitrary-path API (zero callers today). If a future caller
feeds it dynamic/untrusted input, the POSIX `/bin/sh -c` backend branch (non-shipping, correctly
escaped today) would deserve re-review at that point.
