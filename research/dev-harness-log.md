# The development harness — decision log

Prompt 002, Group AI (goals 202–224). Written in this repo's established voice: every measurement,
every "decided against", and the two things that turned out to be wrong.

---

## 1. `engine/cli` — why type erasure and not a variant

The brief's own guidance was right and worth restating with the reason: **the table is
compile-time, the parse is a plain runtime loop.**

An `Option` is a `constexpr` aggregate — name, alias, kind, setter, help, default text, group —
and the whole of the type erasure is one function pointer:

```cpp
using Setter = Status (*)(void* base, std::string_view token, const Option& self);
```

produced by `bind<&AppOptions::seed>()`. The member pointer carries both the owning type (to cast
the `void*` back) and the member type (to pick the conversion), so a row naming a member of the
wrong struct, or a member with no conversion, is a compile error **at the table's own definition
site** rather than a runtime surprise.

**Decided against: `std::variant<int, float, std::string, glm::vec3, …>` per row.** It is the
obvious first idea and it is worse on every axis that matters here.
`templates-and-metaprogramming.md` §5 is explicit that the closed-set variant is right for a *known
set of alternatives you dispatch over*, and that erasure is right when *"a template parameter's only
job is 'some type with this shape'"*. That is exactly this case: the parser never wants to know
which alternative a row holds, it wants to hand a token to whatever knows how to consume it. A
variant would also have grown a case every time a target type appeared (`std::optional<bool>`,
`std::size_t`, `std::uint32_t`, `Size`, three different enums…), and `parse()` would have had to
`visit` it. With the function pointer, `parse()` is compiled **exactly once for every table in the
repo** and adding a target type is adding an `if constexpr` branch in one detail header.

**Decided against: a template-parameter table** (`parse<Table>(...)`). It would instantiate the
whole parser per program. `compile-time-performance.md` rule 14's whole point.

**Extended once, on contact with the real surface:** `bind<>` takes a *path* of member pointers,
because half the app's options land in a nested settings struct
(`bind<&AppOptions::svo_settings, &Settings::wind, &WindParams::base_speed>()`). The alternative
— a flat mirror struct copied into the nested one after parsing — is precisely the hand-mirrored
duplication this module exists to delete.

### What the port changed, and what it deliberately did not

`voxel_app`'s 43 flags, `svo_render`'s 24, and both dump tools' positional forms now go through
one table each. The **drift is gone**: `--root-log2` is an *alias* of the app's `--region-log2`,
and `--verify` an alias of `--verify-frame` — row properties, not second rows with second targets.

Five members changed from `no_*` to a positive form behind `ValueKind::Toggle` (`post`, `bloom`,
`tonemap`, `sky`, `view_polish`), so `svo_settings.sky = !no_sky` and its siblings became one
spelling instead of a mirrored negation.

**One genuinely awkward pair survives, named rather than quietly renamed:** `--grain A` sets an
*amplitude* while `--no-grain` removes the *term*. Two targets under names that look like one
option. A `Toggle` cannot express that — its `--no-` form must reach the same member — so
`--no-grain` stays a plain flag and `finalize()` applies it, identically in both programs. Goal
204's rule is that behaviour does not change; renaming it would have been a behaviour change
dressed as tidying.

**One deliberate improvement, recorded rather than discovered later.** `std::from_chars` plus an
end-of-input check refuses `"12abc"` and `"twelve"`, where `strtol` — what all three hand-rolled
parsers used — returned 12 and 0. `--seed` with nothing after it is now a diagnostic rather than
"keep the default".

### Checks performed

- **201/201 tests** (was 177): 14 mechanism cases in `engine/cli`, 10 field-by-field cases in
  `app/tests` written from the pre-port parse chain **before** deleting it.
- **`voxel_app --frames 8`, pre-port vs post-port, both backends: the app's own log lines are
  byte-identical.** The pre-port binary was `C:/b/windows-release/app/voxel_app.exe`, built at
  18:25 the same day, i.e. genuinely before the change.
- **16 documented command lines re-run, all exit 0**; 2 deliberate negatives exit 1;
  `mesh_dump`'s positional and named forms produce byte-identical `.obj`.

---

## 2. `dev/scenario` — the format, and what was left out of it

A `.scn` is line-oriented text, one directive per line, `#` comments. **Not JSON**, because a
scenario is read and edited by a human at 2am and a quoted comma-terminated option list is worse
for that. **Not a scripting language**, because the moment a scenario can compute, two scenarios
stop being comparable and the file stops being a description of what was run.

Deliberately left out: conditionals, variables, arithmetic, loops, and any way to name another
scenario's result. `include` exists (relative to the including file, cycles rejected by resolved
path rather than by a depth limit — a depth limit turns "a includes b includes a" into "too deep",
which points at the wrong file).

**`InputFrame` carries `world::player::PlayerIntent`, not a second input vocabulary.** This is
load-bearing: it means a scenario *cannot express an input the player cannot make*. The previous
pass's `--autofly` was a teleport applied outside the simulation it was testing and reported 74
ground violations that were its own; the structural fix is that there is no second vocabulary to
drift.

**`jump_pressed` is an edge on exactly one tick**, per `world/player/README.md`. A `hold ... jump
1.0` presses once, not sixty times. Asserted directly.

**`goto` is closed-loop** — it reads the body's actual position, because a hill or a tree can stop
it — with a timeout so a blocked body ends the segment instead of the run.

### `pose_ground`, and the bug that caused it

The library was first authored with absolute `pose x,y,z` lines. `walk_shoreline`'s was
`pose 40,30,120` — and the ground at (40, 120) is **48 m**. The camera spawned eighteen metres
inside a hill; every capture was a flat grey square; `contrast_percent` read **0.0%**.

The fix is not "pick better numbers". It is `pose_ground <x,z> <yaw> <pitch> <metres above
ground>`, resolved by the harness against `HeightmapGenerator::height_at` (clamped to sea level the
same way `svo_render --xz` does). An absolute pose is a number that has to be right about a world
it cannot see — **and it would not have survived Prompt 006**, which replaces the terrain generator
outright: every absolute pose in the library would have silently moved underground on the day that
landed.

The replacement poses were then chosen from **measured** heights, not guessed:
`svo_render --xz X,Z` prints a 5×5 surface grid. Along z = 0, x = 48…112, the ground reads
**66.0, 56.2, 47.2, 13.5, −0.5 m** — a real shoreline, and what `walk_shoreline` now walks down.
At z = +16, x = −32…+32 it reads **−56.3, −15.8, 25.9, 61.9, 66.7** — a 40 m climb over 32 m, which
is `walk_hillside`.

---

## 3. `voxel_harness` — extracting the frame loop, not copying it

`Session`, `run_svo`, `run_mesh` and the per-frame phases moved from `app/src/main.cpp`'s anonymous
namespace into `app/src/app_run.cpp`. `main.cpp` is now 71 lines: parse, `--help`, `--crash-test`,
`app::run()`. `voxel_harness` **compiles the same translation unit** (the same pattern `app/tests`
already uses, and for the same reason: an executable's objects are not linkable, and promoting the
frame loop to a library just so two exes can share it would invert the dependency story).

The seam is `app::FrameInput` — a small virtual interface, not a template parameter, because
`run_svo` is one large function and templating it would instantiate the whole frame loop twice for
a handful of calls per frame.

`--headless` creates the GLFW window **hidden**. That is stated in the header rather than implied:
it is the same swap chain and the same back-buffer readback `--verify-frame` already uses, not a
surfaceless presentation path, so a driver that behaves differently without a visible surface would
not be caught by it.

### Two things the harness itself got wrong first

**The frame budget counted loading-screen frames.** `macro_ground` declared `option --frames 240`;
the world takes ~2 s to build, which is ~300 frames; the run reported **0 frames measured**. The
script does not advance during loading (`update_camera_phase` only runs once the world exists), so
the fix is that **the script terminates a run**, and `--frames` is only a safety ceiling — now
generous, and documented as also counting loading frames.

**A moving scenario's final capture is not reproducible on this engine.** `fly_orbit`'s vk `final`
differed from its own golden by **70.4% of pixels** on a re-run, while d3d12's was bit-identical.
The camera position at script end is deterministic; what is not is whether the **last requested
rebuild landed** before the final frame. `run_svo` asks for a whole-world rebuild every 2 m of
camera motion against a 0.6–1.3 s build, so a moving scenario ends with a rebuild in flight roughly
half the time. Every moving scenario now ends with `wait 3`, with the reason written into the file
and a note that **the line should be deletable once Prompt 004 removes the rebuild storm**.

---

## 4. The phase accounting — goal 215's check found a real gap, twice

`FramePhases` had six terms. The check is that they sum to the wall time.

**First attempt was circular and I caught it before trusting it:** the record's `wall_ms` was set
to `phases.sum()`, so coverage read 100% by construction and proved nothing. Fixed by holding each
record over to the top of the next frame and taking `Clock::delta_seconds()` — the loop's own
measurement of how long the frame took.

With an honest wall time, `spawn_stand`'s tree-swap frame read **19.2 ms of phases against a
205.4 ms wall time**. Coverage 99.7%, 4 frames outside ±1%.

**The first hypothesis was wrong.** I assumed `Session::begin_frame()` (poll_events, clock tick,
resize check), added it as a `frame_start` phase, and re-measured: still **16.5 ms of phases
against a 204.6 ms frame**, with `frame_start` reading ~0.

**An end-to-end probe of the loop body found it.** A temporary `steady_clock` around the whole
iteration confirmed the 204.6 ms was *inside* the loop, and the two frames it happened on were the
tree-swap frame and the **last** frame — both **capture** frames. `capture_phase` performs a
staging copy, a **full `WaitForIdle`**, and a libpng encode, and sat between the `overlay` and
`present` timers with no phase covering it.

| | before | after |
|---|---|---|
| phase coverage, `spawn_stand` vk | 99.7% | **99.9%** |
| frames outside ±1% | 4 | **1** |
| worst frame's unattributed time | 186 ms | attributed to `capture` |

Both new phases stay. `frame_start` is genuinely ~0 ms — it stays because *"measured and near
zero"* is a different statement from *"not measured"*, which is the entire lesson of this section.

`voxel_app`'s 2-second stats line and the harness's report now derive from the same
`dev::telemetry::FrameReport`, so they can no longer disagree.

---

## 5. The golden-image metric — calibrated, with one honest negative

Two image metrics have already died in this repo (bytewise, then a tolerance band), so this one had
to earn its thresholds.

**The metric**: per-pixel mean absolute difference over all channels, **plus** the fraction of
pixels whose max-channel delta exceeds 8/255. Two numbers deliberately — §6.5's own conclusion is
that a perceptual mean can *hide* a small localized error while a pure pixel count is drowned by
driver noise. Unity's two-level structure with the cheap per-pixel term.

**Decided against FLIP**, despite it being the best-motivated metric in the literature (§6.5, and
the authors even say which scalar to threshold). It is BSD-3-Clause and single-header and would
have been a genuinely good fit — but it is a new dependency for a comparison whose job here is
"did this change", not "how bad does this look to a human", and the calibration below separates the
cases by more than an order of magnitude without it. **If cross-backend comparison is ever wanted,
FLIP is the thing to reach for** (goal 218's follow-up).

### The measurements

`macro_ground`'s pose, 1280×720, promote-then-rerun:

| case | mean/255 | changed % | max chan | verdict |
|---|---|---|---|---|
| vk vs vk, **TAA on** | 0.7376 | 0.99609 | 212 | — |
| vk vs vk, **TAA off** | 0.7226 | 0.34082 | 213 | the noise floor |
| vk vs vk, `--grain 0.5` | 1.8007 | 7.19032 | 216 | the signal |
| vk vs **d3d12**, TAA off | 2.5481 | 12.76360 | 219 | a different backend |

Thresholds set at the **geometric midpoint** of floor and signal — √(0.34 × 7.19) = 1.56% and
√(0.72 × 1.80) = 1.14 — rounded to **1.5%** and **1.2/255**. 4.4× above the floor, 4.8× below the
signal. Not a round number chosen to make the current build pass.

**Then the overlay came out, and the floor dropped by an order of magnitude.** Viewing the contact
sheet of every capture showed the ImGui panel — fps, ms, brick count, VRAM — baked into every
golden. Its digits change every run. `--overlay/--no-overlay` was added and the harness defaults it
off; re-measured against fresh goldens:

| scenario | mean/255 | changed % |
|---|---|---|
| macro_tree | 0.007 | **0.0000** |
| fly_orbit `quarter` | 0.014 | 0.0000 |
| valley_far | 0.032 | 0.0038 |
| stress_pose | 0.058 | 0.0097 |
| throughput_ramp | 0.089 | 0.0852 |
| spawn_stand | 0.102 | 0.0979 |
| macro_ground | 0.087 | 0.1337 |

The thresholds are left where the calibration put them: they now sit **11–200× above** the floor,
which is margin against a driver update rather than slack.

### The honest negative

**A one-pixel change cannot be caught, and no threshold over this metric can catch one.** The
noise floor even with the overlay off is ~0.13% of 921,600 pixels — about 1,200 pixels. A single
changed pixel is 0.0001%. `max_channel_difference` cannot rescue it either: it reads **212–219 in
every case above, including vk-vs-vk**, because AA always moves *some* pixel on a high-contrast
edge by most of its range. What this metric does catch is a shading-term change — the `--grain`
row is the evidence — and that is what a regression in this engine actually looks like.

**Per-backend goldens are mandatory**, not a convenience: vk and d3d12 differ by 12.8% at the same
pose with the same settings, 37× the TAA-off floor and larger than a deliberate shading change.
Exactly what §6.6 predicted, and what UE (goldens keyed by RHI and hardware hash) and Khronos'
KTX-Software CTS (a designated primary platform) both do.

---

## 6. JSON: hand-rolled, and the bug that justified the test

~90 lines. The case is in `dev/telemetry/json.hpp`: the requirement is *emit* one object per run to
a file nothing in this project reads back in C++ (the consumers are `python -m json.tool`, a diff,
and a human). nlohmann/json is a 25k-line header that would land in every TU that touches it;
RapidJSON and simdjson are parsers, and reading is the half of the problem this project does not
have. **What would change the answer: the moment the harness needs to read a report back in C++.**

**And it shipped broken.** `key()` called `separate()` and then `value(name)`, which separates for
itself — every key came out preceded by a stray comma. The unit test passed, because it checked
brace balance and searched for substrings. Balanced braces and a findable substring are not the
same thing as parseable.

The test now runs a **real recursive-descent JSON grammar validator** (90 lines, test-only) over
the output, plus a guard on the guard: eight inputs the validator must reject, the first of them
verbatim the shape the bug produced.

---

## 7. Decided against, in writing

- **A `--verify-frame` that samples only the first ready frame, for a moving scenario.** It read
  0.0% on `walk_shoreline` about a run whose later frames were fine. The harness samples contrast
  at **every capture point** and asserts on the maximum — sampling where the scenario asked for a
  picture is sampling where it meant.
- **Gating on `max_channel_difference`.** Measured 212–219 in every case including clean re-runs.
- **A depth limit for `include` cycles.** Rejecting by resolved path names the right file.
- **Nsight Perf SDK in-app.** Full reasoning in `docs/gpu-counters.md`; short version: timestamps
  already answer the gating question, counters are permission-gated on both machines that matter,
  and it is a licensed dependency for a number read by one person on one machine.
- **A seventh and eighth phase "for symmetry".** Both were added because a measurement demanded
  them, and `frame_start` was kept *despite* reading ~0.

---

## 8. Numbers this pass produced, for the next one to move

`spawn_stand`, vk, headless, RelWithDebInfo, this machine:

```
frames: 409 measured (+313 warm-up over 2.0 s)
frame ms: mean 7.36  median 6.48  p95 9.72  p99 10.39  max 229.20  min 0.86
gpu ms  : mean 6.16  median 6.16  p95 7.37  p99 8.11  max 9.09
slow frames (> 20 ms): 1 of 409 -- 1 on a tree swap
phase coverage: 99.9% of wall time, 1 frame outside +-1%
```

The single slow frame is the first tree swap, and its cost is `upload` plus `capture`. **The
tree-swap frame is the only slow frame in a stationary scenario** — which is the number Prompt 004's
AK-B/AK-C/AK-D have to move, and `fly_transect` is the scenario that turns it from one frame into
a storm.
