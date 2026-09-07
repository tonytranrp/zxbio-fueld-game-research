# Prompt 002 — The development harness: stop hand-driving the engine

**To:** the main coding session (`[CC]`), branch `C++-voxel`
**From:** the side session, 2026-09-06
**Read this whole file before touching anything.**

**This prompt is the FIRST of a six-prompt arc (002–007). Execute it first and completely.**
Every later prompt's Checks are written in terms of the harness this one builds. Doing 003–007
without it means going back to hand-typing 43-flag command lines and eyeballing PNGs, which is
exactly the problem the owner asked to have removed.

---

## 0. What this pass is and is not

**The gap.** `voxel_app` has 43 command-line flags, `tools/svo_render` has 24, and both parse them
in a hand-written `if (arg == "--x")` chain in `main()`. Every verification in this project's
history — the ribbon bug, the shadow rings, the water checkerboard, the wind diffs, the Gerstner
spectrum — was performed by a human composing a novel flag combination, launching a windowed
build, waiting for a world, taking a PNG, and comparing it by eye or with a throwaway Python
script. That worked, and it found five real bugs last pass. It does not scale to the next five
prompts, and the owner has said plainly that running the builds and the exes has become the
dominant cost of making a change.

**What this pass turns it into.** One declarative option layer shared by every executable, and one
*scenario* system: a named, versioned, deterministic description of "put the camera here, drive
these inputs for this long, capture at these moments, assert these budgets," runnable headless,
runnable on both backends, runnable from `ctest`, emitting one machine-readable report plus the
captures. Adding a new verification becomes writing a scenario file, not composing a command line.

**Named outcome.** `voxel_harness --scenario walk_shoreline --backend vk,d3d12` runs, and prints:
per-pose captures written, the frame-time histogram with the slow-frame attribution, the GPU
per-pass timings, the golden-image comparison verdict, and PASS/FAIL against declared budgets — in
one invocation, with no window if you don't want one, and with the exact same numbers on a re-run.

**What this pass is NOT.** It is not a renderer change, not a terrain change, not a gameplay
change. No shading term moves. No goal from 157–199 gets closed here except by the harness proving
it. If you find yourself editing `svo_march.psh.hlsl`, you have left this prompt. The one exception
is instrumentation that reads state without changing what is drawn (§4, Group AI-D), and even that
must be provably free when disabled.

---

## 1. Context to read FIRST, in this order

Do not summarise these from memory. Read them.

1. **`CLAUDE.md`** — the build non-negotiables. In particular: build to `C:/b/<preset>` (MAX_PATH),
   real MSVC from a `vcvars64.bat` shell of the *same edition* the build dir was configured with,
   never benchmark in `windows-debug` (measured 3.4×–80× slower), shaders load at runtime so a
   shader edit needs no rebuild, and the RTK/Bash heredoc backslash trap (use the Edit tool for
   escape-sensitive edits).
2. **`docs/progress.md`** — current state, the architecture map, and *"Decisions that survived
   contact with evidence."* That last section is binding: do not re-litigate anything on it without
   new evidence. Read it before proposing any alternative to something it names.
3. **`docs/goals.md`** — the living backlog. Read the group notes for AB (the lag, measured), Y
   (micro-voxel measurements), and B (visual self-verification infrastructure). Group B is the
   ancestor of this prompt; the harness must subsume `--verify-frame`, not sit beside it.
4. **`research/engine-hardening-log.md`** §benchmarks — the Google Benchmark methodology already
   established here (Mann-Whitney via `tools/compare.py`, non-const lvalues to `DoNotOptimize`, no
   LTCG on benchmark targets, baselines saved to `benchmarks/baselines/<date>-<change>.json`). The
   harness's performance assertions must not contradict this; they extend it to whole frames.
5. **`research/lin-look-log.md`** and **`research/gameplay-pass-log.md`** — how the last two passes
   actually verified things. Every ad-hoc technique in those logs (the `--lod-center` CPU
   reproduction, the shader-return-line bisection, the pixel-row sampling, the wind on/off pixel
   diff, the 900-frame autofly slow-frame attribution) is a scenario this harness should be able to
   express declaratively. Read them as a requirements document.
6. **`research/profiling-tooling-integration.md`** — the existing Tracy integration research. Check
   what it already concluded before re-deriving it.
7. **`research/gpu-voxel-streaming-and-profiling-research.md`** — §5 and §6 specifically: the
   documented overheads of Tracy zones / `TRACY_ON_DEMAND`, Vulkan timestamp and performance
   queries on NVIDIA, D3D12 query heaps and pipeline statistics, NVIDIA Perf SDK programmatic
   counters, and the survey of how real engines script screenshot/perf regression tests and which
   image metrics tolerate GPU nondeterminism. **This file is the evidence base for Group AI-D and
   AI-E; read those two sections in full before writing either.**
8. **The C++ skill reference files** (`~/.claude/skills/cpp-heavy-templates/references/`):
   - `modular-architecture.md` — §1 folder/CMake organization, §2 interface/implementation
     separation, and the self-registering-factory pattern. The scenario registry is exactly that
     pattern; use the version written down there rather than inventing one.
   - `templates-and-metaprogramming.md` — §1 concepts, §3 policy-based design, §5 **type erasure**.
     Read §5 before deciding how scenarios are stored: a scenario is the textbook case where a
     template parameter's only job is "some type with this shape."
   - `tooling-testing-and-ci.md` — the CI shape and the "sanitizer jobs parallel, not sequential"
     rule.
   - `compile-time-performance.md` — because you are about to add a module every executable
     includes. Explicit instantiation at the choke points, per rule 13.
   This is **mandatory reading, not a suggestion.** The owner's explicit instruction for this arc is
   that the code structure follow that skill rather than accrete.

---

## 2. The systems you are building on — verified file map

Every path and claim below was checked against the file on 2026-09-06. Where a line number is
given, it was read.

- **`app/src/main.cpp`** (1131 lines). `AppOptions` at line 61; a hand-rolled parse chain; two
  entry points `run_mesh` (line 702) and `run_svo` (line 841); `struct Session` (line 615) holding
  window, device, post chain, overlay, input, camera entity, clock; per-frame phases already split
  into named functions (`update_camera_phase`, `overlay_phase`, `report_phase`, `capture_phase`).
  **The frame loop already has a phase-timing struct** (`FramePhases` inside `run_svo`, with
  `upload / camera / render / post / overlay / present` plus `swapped / uploading / building /
  refreshed` cause flags) and a slow-frame attributor at `kSlowFrameMs = 20.0`. That struct is
  local to `run_svo`. **Lift it, don't rebuild it.**
- **The 43 flags** on `voxel_app`: `--mode --renderer --frames --radius --seed --verify-frame
  --validation --autofly --walk --noclip --step-height --no-view-polish --wind-speed --no-wind
  --crosshair --no-crosshair --upload-budget --dump-every --no-post --no-sky --no-bloom
  --no-tonemap --no-shadows --no-ao --no-lod-march --lod-quality --no-grain --no-taa
  --smooth-pixels --grain --ao-radius --shadow-lod --svo-threads --svo-upload-mb --debug-view
  --voxel-log2 --region-log2 --lod-radius --no-trees --pos --yaw --pitch --crash-test`.
  **The 24 on `tools/svo_render`**: `--seed --voxel-log2 --root-log2 --lod-radius --pos --xz --yaw
  --pitch --size --fov --no-trees --no-shadows --no-ao --no-grain --no-lod-march --smooth-pixels
  --grain --ao-radius --shadow-lod --lod-center --view --verify --out --threads`. Note the overlap
  and the drift: `--region-log2` vs `--root-log2` are the same concept under two names, and
  `--verify-frame` vs `--verify` likewise. That drift is a symptom of there being no shared
  definition.
- **`render/diligent/include/render/diligent/svo_renderer.hpp`** — `Settings` (shadows, ao,
  lod_march, sky, grain, taa, lod_quality, shadow_lod, ao_lod, ao_radius_px, smooth_pixels,
  grain_amplitude, taa_blend, debug_view, wind, upload_bytes_per_frame). Already has
  `last_gpu_ms()` (one timestamp query around the march pass), `last_upload_ms()`,
  `last_upload_frames()`, `gpu_memory()`. **A Vulkan caveat is documented and real: the app faults
  if its very first command is a timestamp query, so the existing timer skips the first two
  frames.** Any new query must honour that.
- **`render/diligent/include/render/diligent/frame_verify.hpp`** + `src/frame_verify.cpp` (210
  lines) — the back-buffer readback and the LOCAL-CONTRAST metric (neighbour delta > 4/255;
  terrain 34.7%/34.6%, sky-only 0.9%, threshold 6%). This is the existing image judge. The harness
  wraps it; it does not replace it.
- **`render/diligent/include/render/diligent/memory_tracking.hpp`** — `GpuAllocationTracker`, and
  `vulkan_tools.cpp` (188 lines) for `VK_EXT_memory_budget`.
- **`render/diligent/include/render/diligent/gpu_tools.hpp`** (30 lines) — check what is already
  there before adding a second GPU-utility header.
- **`tools/svo_render/src/main.cpp`** (699 lines) — the CPU reference renderer. Prints a per-level
  sampled/kept brick histogram and a camera-column probe. Writes PNGs **stored, not compressed**
  (2,765,798 bytes at 1280×720); re-save through PIL before committing one. `png_writer.hpp` is
  local to this tool.
- **`tools/tree_dump/src/main.cpp`** (143 lines, currently untracked) — the newest tool, written
  last pass. Note it as a third copy of the same option-parsing idiom.
- **`engine/`** modules: `core` (clock/log/math/config/application), `ecs`, `events`
  (`entt::dispatcher`, **main-thread only**), `input` (`InputState` + edge `take_*`), `jobs`
  (`ThreadPool` over moodycamel). **`engine/core/include/engine/core/config.hpp` exists — read it
  before adding a config module; it may already be the right home.**
- **`benchmarks/`** — `bench_chunk_map.cpp`, `bench_mesh_extract.cpp`, `bench_octahedral.cpp`,
  `measure_world_memory.cpp`, behind `-DVOXEL_BUILD_BENCHMARKS=ON`, Google Benchmark v1.9.5.
- **Test layout**: every module has `tests/CMakeLists.txt`; **177 tests currently pass**. That
  number is the floor for this pass, not the target.
- **`.github/workflows/ci.yml`** — seven gating jobs (clang-tidy, three `core` legs, ASan+UBSan,
  TSan, Windows renderer + WARP smoke) plus a best-effort Linux `renderer` leg that fails on the
  documented goal-109 gap (`crash_handler.cpp` is unconditionally Windows-only). **Do not add a
  `branches:` filter containing `C++-voxel` — `+` is a glob quantifier and kills every run.**
- **MSVC has no UBSan.** Last pass's signed-overflow bug in the tree-species hash was invisible
  locally and only reachable through the Linux CI leg. That is a standing argument for pushing
  early and often rather than batching a whole prompt into one commit — and it applies to this
  prompt too.

---

## 3. Standing rules for this pass (the repo's own, restated)

1. **A visual change is verified by a viewed capture** — you open the PNG and look at it — on both
   backends where anything shader-adjacent is involved. `--verify-frame`'s percentage alone has
   never been "done" in this repo and is not now. (The ribbon bug: two "verified" runs of
   silhouette slivers.)
2. **CPU reference first, GPU mirror second, oracle always.** `world/svo/src/ray_trace.cpp` and
   `render/diligent/shaders/svo_march.psh.hlsl` change together, and the 7,000-ray brute-force
   oracle is the gate. Nothing in *this* prompt should touch either — if you think it must, say so
   and stop.
3. **Performance claims are measured**, with before/after numbers in the log, on a Release or
   RelWithDebInfo build, never Debug.
4. **Tests stay green and new systems get new tests.** 177 is the floor.
5. **Determinism**: same seed → same world, across renderers and tools. The
   terrain-sampler/`fill_terrain` byte-equivalence test must not break.
6. **Materials are components** (`world/materials/defs/` + entry + enumerator). No material-ID
   literals in consumers.
7. **Main-thread-only events**; background work follows the `SvoWorld` worker+pool pattern.
8. **No new dependencies without a written case in the log.** This prompt has one candidate
   (a JSON writer for the harness report) — see AI-C4; prefer a 60-line hand-rolled emitter over a
   dependency for output-only JSON, and write down which you chose and why.
9. **Read the goals.md group notes before closing any goal.**
10. **Use your skills.** `cpp-heavy-templates` for every structural decision in this prompt —
    specifically the reference files named in §1.8. `research-and-think`'s empirical mode for
    anything you cannot find documented.
11. **Web research is delegated, never done inline.** If you need a fact from the web, spawn **one
    or two** read-only research subagents with a specific question and let them return a cited
    report; you keep coding while they run. Do not run a long search sweep in the main session —
    it burns the context this prompt needs. Persist anything worth keeping into `research/` and
    cite it.
12. **Commit per completed group, and push.** Do not batch this whole prompt into one commit. The
    UBSan bug is the standing argument: CI sees defect classes this machine cannot.
13. **`git add` explicit paths, never `git add -A`.** The side session writes into `research/`
    concurrently; two of its in-progress documents have already been swept into main-session
    commits. Stage what you changed.

---

## 4. Task groups

Groups are in dependency order. Within a group, tasks are roughly sequential. Each task's
**Check** is the acceptance criterion — mechanical wherever it can be.

### Group AI-A — One option layer (goals 202–207)

The point is not tidiness. It is that a scenario file cannot reference an option that only exists
as a string literal inside one `main()`.

**202. `engine/cli`: a declarative option table.**
A new module `engine/cli` (own folder, own `CMakeLists.txt`, own `README.md` stating the folder's
rules — `modular-architecture.md` §1). An option is *declared* as data: long name, optional short
name, value kind, target, help text, and a default. The table is a `constexpr` aggregate; parsing
is one function over it. Constrain every template parameter with a concept
(`templates-and-metaprogramming.md` §1) — no bare `typename T` on the public API.
Support the shapes the existing 43 flags actually use, and no more: `bool` presence flags,
paired `--x` / `--no-x` toggles, `int`, `float`, `std::string`, `vec3` (`--pos x,y,z`),
`WxH` (`--size`), and a named enum (`--debug-view`, `--mode`, `--renderer`). Unknown option, missing
value, and unparseable value are three distinct diagnostics naming the option.
Guidance: this is a case where the *table* is compile-time and the *parse* is a plain runtime loop.
Do not build an expression-template parser. Read `templates-and-metaprogramming.md` §5 and note
that the storage of "some type I can assign a parsed value into" is type erasure, one small
`void*` + function-pointer setter per row — not a variant of every option type in the program.
**Check**: a unit test drives a fake table through: every value kind round-trips; `--no-x` and
`--x` both reach the same target; each of the three diagnostics fires with the option's name in the
message; an option table with a duplicate long name fails to compile (`static_assert`, and a test
that documents it as a compile-time property rather than trying to compile it).

**203. `--help`, generated from the table.**
`voxel_app --help` prints every option, grouped, with its default and help text, and exits 0.
**Check**: `--help` output contains all 43 current option names; a test asserts the count matches
the table size, so a future option cannot be added without appearing in help.

**204. Port `voxel_app` to the table.**
Delete the parse chain. `AppOptions` stays the destination struct. **Behaviour must not change**:
same defaults, same flag names, same semantics, including the awkward ones (`--crosshair` defaults
to `!verify_frame`; `--upload-budget 0` means unlimited; `--svo-threads 0` means three quarters of
the hardware threads).
**Check**: a table-driven test enumerates every current flag with a representative value and
asserts the resulting `AppOptions` field-by-field against the pre-port behaviour (write the
expectations from the current code *before* deleting it). Plus: launch with `--frames 8` and no
other flag on both backends, and confirm the log's opening lines are byte-identical to a
pre-change run.

**205. Port `tools/svo_render`, `tools/mesh_dump`, `tools/tree_dump` to the same table.**
Reconcile the drifted names: keep `--region-log2` and make `--root-log2` an alias; keep
`--verify-frame` and make `--verify` an alias. Aliases are a row property, not a special case in
the parser.
**Check**: every existing command line in `CLAUDE.md`, `docs/`, and `research/*.md` still runs. Grep
for `svo_render ` and `voxel_app ` across the repo, run each one you find, and list them in the
log with their exit codes. A documented command line that no longer works is a failure of this
task, not a doc problem.

**206. A response-file / config form.**
`@file` on the command line expands to that file's lines as arguments (one option per line,
comments with `#`). This is the mechanism scenarios use to carry their engine settings, so it must
exist before Group AI-B.
**Check**: a config file reproducing a 12-flag command line yields a field-identical `AppOptions`;
a nested `@file` is rejected with a clear message; a missing file names the path.

**207. One options struct, two consumers.**
`AppOptions` currently holds `SvoRenderer::Settings` by value. Make the *renderer settings* half
independently constructible from the table so a tool that has no window can build the same
settings the app would. This is what lets a headless scenario and a windowed scenario be the same
scenario.
**Check**: a test builds `SvoRenderer::Settings` from a config file and asserts equality with the
same settings parsed through `voxel_app`'s full table.

---

### Group AI-B — Scenarios (goals 208–214)

A **Scenario** is: an engine configuration, an initial pose, a deterministic input script, a set of
capture points, and a set of assertions. It must be expressible as a file, and it must run
identically headless and windowed.

**208. `dev/scenario`: the scenario model.**
New top-level module `dev/` with `dev/scenario/` inside it (`include/dev/scenario/…`,
`src/`, `tests/`, `README.md`, `detail/` for the template internals). Define:
- `struct Pose { glm::vec3 position; float yaw; float pitch; }`
- `struct InputFrame { world::player::PlayerIntent intent; glm::vec2 look_delta_pixels; }` — note
  this reuses `world::player::PlayerIntent` (already the controller's input vocabulary, already
  independent of GLFW) rather than inventing a second one.
- A **motion script**: a sequence of timed segments, each producing `InputFrame`s at the fixed
  timestep. Segment kinds, minimum set: `hold` (an intent for N seconds), `look` (turn to a target
  yaw/pitch over N seconds), `goto` (drive toward a world point until within R), `wait` (N
  seconds of no input).
- **Capture points**: at frame N, at time T, at a named pose, or on an event (first tree swapped,
  first grounded, first slow frame).
- **Assertions** (declared, evaluated at the end): frame-time percentiles, a hard max frame time,
  slow-frame counts by cause, GPU pass budgets, memory ceilings, `frame_verify` contrast floor,
  golden-image verdict, and gameplay invariants (walk violations == 0).
Keep the *model* free of Diligent, GLFW and FastNoise2 — `dev/scenario` is a description, the
runner in AI-B2 is what executes it. Same firewall discipline as `render/interface`.
**Check**: `dev/scenario` compiles and its tests pass with `-DVOXEL_BUILD_RENDERER=OFF`. A test
builds a two-segment script and asserts the exact `InputFrame` sequence it produces for a given
timestep, including that `jump_pressed` is an edge on exactly one tick (the convention
`world/player/README.md` documents).

**209. The scenario file format.**
A small line-oriented text format, not JSON, not a scripting language. One directive per line:
`option <name> <value>` (fed through AI-A's table), `pose <x,y,z> <yaw> <pitch>`,
`hold <keys> <seconds>`, `look <yaw> <pitch> <seconds>`, `goto <x,y,z> <radius>`,
`capture <when> <name>`, `assert <metric> <op> <value>`, `backend <vk|d3d12|both>`,
`golden <path>`, `include <other.scn>`.
Scenario files live in `dev/scenarios/*.scn`, are checked in, and are data — **adding a scenario
must not require a rebuild.**
**Check**: a round-trip test parses every checked-in `.scn`, re-emits it, and re-parses to an
equal model. A malformed line reports file, line number, and what was expected. `include`
resolves relative to the including file and rejects cycles.

**210. Self-registering built-in scenarios.**
Some scenarios want code (a custom assertion, a computed pose). Use the self-registering factory
from `modular-architecture.md` §2 so a built-in scenario is one translation unit that registers
itself by name; `--list-scenarios` enumerates both built-ins and `.scn` files.
**Check**: adding a built-in scenario requires touching exactly one new `.cpp` and no registry
list (a test asserts a scenario added in the test's own TU appears in the registry). `--list-scenarios`
shows both kinds with their source.

**211. `voxel_harness`: the runner.**
A new executable `dev/harness/`. It: resolves a scenario by name, builds the engine from its
options (reusing `Session` from `main.cpp` — **extract `Session` into a shared header first**, do
not copy it), drives the fixed-step controller from the motion script instead of from GLFW,
services the world/upload/render/post/overlay phases exactly as `run_svo` does, captures at the
capture points, evaluates assertions, and exits non-zero on any failed assertion.
Modes: `--headless` (no window; use the same offscreen path `--verify-frame` already relies on),
windowed (so a human can watch the scenario run — this is how the owner will look at things),
and `--backend vk,d3d12` to run the whole scenario once per backend.
**Critical**: the input must come from the script, *not* from the frame loop's real input, and the
simulation must be the same simulation. Last pass found `--autofly` was not part of the simulation
it was testing. Do not repeat that: the harness drives `PlayerIntent` into the *same*
`step_player` call the app uses, at the same fixed timestep.
**Check**: run the same scenario twice on the same backend and diff the report — frame count,
capture hashes, and every gameplay-invariant number identical (frame *times* will differ; they are
not part of the identity check). Then run it headless and windowed and assert the capture PNGs are
identical. A deliberately failing assertion makes the process exit non-zero with the metric, the
threshold, and the measured value on one line.

**212. Port the existing verification flags onto scenarios.**
`--verify-frame`, `--autofly`, `--dump-every`, `--frames` keep working on `voxel_app` (they are in
`CLAUDE.md` and in a dozen research logs) but are *reimplemented as scenarios* the app can run, so
there is one code path. `--autofly --walk`'s "0 ground violations per frame" becomes a scenario
assertion.
**Check**: `voxel_app --verify-frame` still prints the same contrast percentage it does today
(34.7% vk / 34.6% d3d12) and the equivalent harness scenario prints the same number to the same
precision. `--autofly --walk` and its scenario twin both report 0 violations.

**213. The starting scenario library.**
Write these `.scn` files, each with a one-paragraph header comment saying what it is for:
- `spawn_stand` — spawn, stand still 3 s. The cheapest smoke test.
- `walk_shoreline` — walk from inland to the waterline, into the water, swim, climb out.
- `walk_hillside` — walk up and down a steep slope; asserts no ground violations and no
  step-jitter (see 006).
- `fly_transect` — a long straight flight at speed across the region. **This is the stutter
  reproduction**; it must exist before Prompt 003.
- `fly_orbit` — orbit a hilltop at 60 m, looking inward. The panoramic pose.
- `macro_ground` — camera 30 cm above ground looking down at 45°, still. The close-up look shot.
- `macro_tree` — camera 2 m from a tree trunk. The vegetation look shot.
- `valley_far` — a 300 m view down a valley. The distance/LOD look shot.
- `stress_pose` — the worst pose found so far (ground level at a hilltop, shadows and AO on).
- `throughput_ramp` — see 219.
**Check**: every scenario runs to completion on both backends, headless, and its report is
committed under `dev/baselines/<date>-<scenario>-<backend>.json`. The captures are viewed — say so
in the log, and name the file you looked at.

**214. `ctest` integration.**
Register the cheap scenarios as tests with a label (`ctest -L scenario`), so `ctest --preset
windows-relwithdebinfo -L scenario` runs the whole visual/perf suite. Keep them out of the default
`ctest` run if they need a GPU — mirror how the existing renderer tests are gated.
**Check**: `ctest -L scenario` passes locally on both backends. `ctest` with no label still runs
the 177 unit tests and does not require a GPU.

---

### Group AI-C — Reports and golden images (goals 215–218)

**215. The frame report.**
Lift `FramePhases` and the slow-frame attributor out of `run_svo` into a real type in `dev/`
(candidate: `dev/telemetry/`), used by both `voxel_app` and `voxel_harness`. Record per frame:
wall time, the six phase times, the cause flags, the GPU march+resolve time, and the counters
(bricks, node words, MB resident, uploads in flight). Emit at end of run: count, mean, median, p95,
p99, max, a histogram, slow frames by cause, and the worst five frames with their full breakdown.
**Check**: the report's own numbers are internally consistent — the six phases sum to within 1% of
the wall time for every frame (they must, or a phase is unaccounted for; if they don't, you have
found a real gap and should say where it is). `voxel_app`'s 2-second stats line derives from the
same type, so the two can no longer disagree.

**216. Machine-readable output.**
`--report <path.json>`. One object per run: scenario name, backend, git SHA, build preset, GPU
name and driver, every option that differed from its default, the frame statistics, per-capture
records with image hashes, and every assertion with its threshold and measured value.
See rule 8 about the dependency: prefer a small hand-rolled emitter; JSON output is a
30-line problem and this project's culture is to write down the choice.
**Check**: a schema test parses a produced report and asserts every required key exists.
Two runs of `spawn_stand` produce reports that differ only in the timing fields and the timestamp
— assert that mechanically (diff the objects with the timing keys removed).

**217. Golden-image comparison that survives two backends and a driver update.**
Per `research/gpu-voxel-streaming-and-profiling-research.md` §6, pick a metric that tolerates GPU
nondeterminism instead of a bytewise compare — and **state the reasoning in the log**, because this
project has already had two image metrics die (bytewise, then a tolerance band). The existing
LOCAL-CONTRAST metric is a *scene-content* check, not a regression check; you need both.
Requirements: a verdict on `(golden, actual)` with a numeric distance and a threshold; a written
diff image on failure (so a human can look at *where*); and a documented threshold calibrated by
actually measuring vk-vs-d3d12 on the same scenario and vk-vs-vk across two runs. Calibrate,
don't guess: measure the noise floor, set the threshold above it, and record both numbers.
**Check**: the measured vk-vs-vk distance, the vk-vs-d3d12 distance, and the chosen threshold are
all three in the log with the scenario they came from. A deliberately introduced 1-pixel change
fails; a re-run of the identical scenario passes. The diff image for a failure is written and
viewed.

**218. `--accept-golden`.**
A run can promote its captures to goldens. Goldens live in `dev/goldens/<scenario>/<backend>/`.
**Check**: promoting and re-running gives PASS. A promotion that would overwrite a golden prints
the old and new distances first and requires the flag (no silent overwrite).

---

### Group AI-D — Instrumentation that must not cost a frame (goals 219–222)

The owner's constraint is explicit: profiling must not affect frame generation. Treat "it's free"
as a claim requiring measurement, not an assumption.

**219. The voxel-throughput scenario.**
`throughput_ramp`: from a fixed pose, sweep the parameter that controls how much voxel detail is
resident and marched — `--lod-radius` and `--lod-quality` — across a declared ladder, holding the
pose fixed, and record for each rung: resident bricks, resident MB, node words, primary-ray steps
(the `steps` debug view's mean, read back), GPU march ms, and fps. Output a table.
This answers "how many voxels can this GPU actually march at 150 fps" with a number instead of an
opinion, and it is the yardstick Prompt 003 is judged against.
**Check**: the table for the current build is in the log and committed as a baseline. It must show
a monotone relationship between resident detail and GPU ms (if it doesn't, something else is the
bottleneck — say so, that is a finding). State the rung at which the current build first drops
below 150 fps and below 60 fps.

**220. Per-pass GPU timing, on both backends, honouring the first-command fault.**
Today there is one timestamp pair around march+resolve. Extend to a small, named set of ranges:
march, TAA resolve, post (bloom), overlay, present. Use the backend query mechanisms documented in
the research file §5(b)(c). **The documented Vulkan fault — the app crashes inside the NVIDIA
driver if a timestamp query is the app's very first command — is real and already worked around by
skipping two frames; preserve that.**
**Check**: the five ranges sum to within 10% of the frame's GPU time on both backends across a
300-frame scenario. Toggling the timers off and on across two runs of `stress_pose` moves the
median frame time by less than the vk-vs-vk noise floor you measured in 217 — report both numbers.
If the timers *do* cost more than that, say so and make them opt-in.

**221. Tracy, on demand and provably free when off.**
Per the research file §5(a): wire `TRACY_ON_DEMAND` so an un-connected build pays close to nothing,
and add GPU zones for the ranges from 220. Then *measure* it: `stress_pose` with Tracy compiled
out, compiled in and not connected, and compiled in with a client connected.
**Check**: three median frame times in the log. Compiled-in-not-connected must be within the noise
floor of compiled-out; if it isn't, Tracy stays behind a build option that is OFF in the shipping
preset, and you write that down. Do not report "no measurable impact" without the three numbers.

**222. Counter readback for the deep questions.**
The research file §5(d)/(e) covers programmatic access to SM occupancy, warp-stall reasons, and
L1/L2 hit rates. Decide — with the research in hand — whether in-app counter collection is worth
building now or whether Nsight Graphics driven externally against a harness scenario is the right
answer for this project. **Either answer is acceptable; an unargued answer is not.** If external:
document the exact Nsight invocation against a named scenario in `docs/` so it is reproducible.
If in-app: gate it behind a build option, measure its cost like 221, and expose it in the report.
**Check**: the decision, its reasoning, and a reproducible recipe for getting warp-stall and
cache-hit data for `stress_pose` are in `docs/` and in the pass log. Run the recipe once and put
the numbers in the log — they are Prompt 003's starting evidence.

---

### Group AI-E — CI and hygiene (goals 223–224)

**223. The headless subset in CI.**
The Windows renderer leg already runs a WARP smoke test. Add the cheap headless scenarios
(`spawn_stand`, and one capture-and-compare) to it. WARP is a software rasteriser — its images will
not match a golden taken on an NVIDIA GPU, so either take WARP-specific goldens or assert only the
non-image parts. Choose, and say which.
**Check**: CI green, with the new step visible in the run log. Do not add a `branches:` filter.

**224. Retire what the harness replaces.**
Once the harness covers them, remove the ad-hoc paths it duplicates rather than leaving two ways to
do everything: the `run_svo`-local `FramePhases`, the per-tool option chains, and any throwaway
verification script still checked in. `--verify-frame`/`--autofly`/`--dump-every` stay (they are
documented), but as thin wrappers.
**Check**: `grep -rn "argv\[" app tools dev` finds exactly one parse site. The count of hand-rolled
option comparisons across the repo is zero. 177+ tests still pass.

---

## 5. Sequencing, budget, and how to not get stuck

- **AI-A → AI-B → AI-C** are a chain. AI-D depends on AI-B (it needs scenarios to measure) but is
  otherwise independent of AI-C. AI-E is last.
- **Commit and push after each group.** Five or six commits, not one. CI catches what this machine
  cannot (no UBSan under MSVC).
- **The riskiest task is 217** (the image metric). Two metrics have already died in this repo. If
  the calibration shows no threshold cleanly separates "same scene, two backends" from "a real
  1-pixel regression," that is a legitimate negative result: report the measured distances, ship
  the metric as *per-backend goldens only* (vk goldens compared to vk, d3d12 to d3d12), and open a
  goal for cross-backend comparison. Do not ship a threshold you had to hand-wave.
- **The second riskiest is 211** (the runner driving the real simulation). If extracting `Session`
  from `main.cpp` turns out to be a large refactor, do it anyway — a harness that runs a *copy* of
  the frame loop is worse than no harness, because it will drift and then lie. That is exactly the
  `--autofly` bug from last pass.
- If a task is blocked on something outside this prompt, finish everything that is not blocked,
  and state precisely what is blocked and why. Do not silently reduce scope.

---

## 6. Explicitly out of scope this pass

- **Any renderer or shading change.** `svo_march.psh.hlsl` and `ray_trace.cpp` do not change.
  That is Prompts 003 and 004.
- **Any terrain change.** `heightmap_generator.cpp` does not change. That is Prompt 005.
- **Any gameplay change**, including removing fly mode. That is Prompt 006 — and it *depends* on
  this harness to prove it works.
- **Goals 157 (palette compression), 158 (incremental rebuild), 160 (editing), 163 (DAG dedup),
  173 (octree-backed collision)** — all named in later prompts. Do not start them here.
- **Trees C4–C6 (190–192) and grass (193–195)** — Prompt 007.
- **Eye-tracked foveation, varifocal, path tracing, multiplayer, asset pipelines.** Not this arc.

---

## 7. When you are done

1. Write **`research/dev-harness-log.md`**: decision-log style, in this repo's established voice.
   It must contain: the option-table design and why type erasure rather than a variant; the
   scenario format and what was deliberately left out of it; the golden-image metric with its three
   calibration numbers; the Tracy three-way measurement; the throughput table; the counter-access
   decision; and every "decided against, in writing."
2. Add **Group AI** to `docs/goals.md` with goals 202–224 numbered exactly as above, each `[x]`
   with its Check recorded as *performed* (numbers, not intentions).
3. Refresh `docs/progress.md`: the architecture map gains `engine/cli`, `dev/scenario`,
   `dev/harness`, `dev/telemetry`; the current-state paragraph gains the harness and the throughput
   yardstick; add this pass's entries to *"Decisions that survived contact with evidence."*
4. Add `docs/dev-harness.md`: how to write a scenario, how to run one, how to promote a golden, how
   to get GPU counters. Short and operational — the thing the owner reads at 2am.
5. Update `CLAUDE.md` with the operational deltas only (new executable, new preset invocations, the
   `ctest -L scenario` line, the Nsight recipe pointer).
6. Update `Prompts/README.md`'s index row for 002.
7. Full suite green, both backends, captures viewed and named.

---

*Provenance: written by the side session on 2026-09-06 after reading `app/src/main.cpp`,
`app/src/svo_world.{hpp,cpp}`, `app/src/spectator_camera.hpp`,
`render/diligent/include/render/diligent/svo_renderer.hpp`, `render/diligent/src/svo_renderer.cpp`
(upload and PSO sites), `render/diligent/shaders/svo_march.psh.hlsl` in full,
`world/svo/include/world/svo/{tree_layout,brick_tree,tree_builder,terrain_sampler}.hpp`,
`world/player/include/world/player/{controller,player_state,tuning}.hpp`,
`world/collision/include/world/collision/terrain_collider.hpp`,
`world/generation/{include/world/generation/heightmap_generator.hpp,src/heightmap_generator.cpp}`,
the full flag surface of `voxel_app`/`svo_render`/`tree_dump`, `docs/goals.md`,
`docs/progress.md`, and the viewed captures `lin_water_checkerboard_after.png`,
`lin_final_vk.png`, `svo_ground_hilltop.png`.*
