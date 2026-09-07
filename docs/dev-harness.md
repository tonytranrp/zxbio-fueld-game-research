# The development harness

The thing you read at 2am. Longer reasoning lives in `research/dev-harness-log.md`.

## What it is

`voxel_harness` runs a **scenario**: a named, checked-in description of "put the camera here, drive
these inputs for this long, capture at these moments, assert these budgets." It drives the *same*
frame loop `voxel_app` runs — literally the same `app_run.cpp` object file, with a scripted
`FrameInput` instead of a keyboard — so a scenario cannot exercise a simulation the player cannot
reach. That is not a style preference: the previous pass shipped `--autofly` as a teleport applied
outside the simulation it was testing, and it reported 74 ground violations that were its own.

```bash
cmake --build --preset windows-relwithdebinfo --target voxel_harness
```

Run it **from the repo root** — `dev/scenarios` and `dev/goldens` are resolved relative to the
working directory.

## Running one

```bash
C:/b/windows-relwithdebinfo/dev/harness/voxel_harness.exe --scenario spawn_stand --backend vk,d3d12
```

```bash
C:/b/windows-relwithdebinfo/dev/harness/voxel_harness.exe --list-scenarios
```

Useful flags: `--headless` (window hidden — same swap chain and readback, nothing on screen),
`--out-dir DIR` (captures and diff images), `--report FILE.json`, `--no-golden`, `--accept-golden`,
`--frames N` (a hard ceiling; normally the *script* ends the run). `--help` prints the rest.

Windowed is the default, because watching a scenario run is how you find out that the pose is
inside a hill.

## Writing one

A `.scn` file in `dev/scenarios/`. It is **data** — adding one needs no rebuild.

```
name walk_shoreline
describe One paragraph saying what this is for and why it exists.
backend both
option --no-taa                  # any voxel_app option; see `voxel_app --help`

pose_ground 64,0 -90 -8 1.7      # x,z  yaw  pitch  metres above the ground
hold forward 6                   # keys are '+'-joined: forward+boost
hold none 1
look 90 -5 1                     # turn to this yaw/pitch over this many seconds
goto 12,66,24 4 20               # drive toward a point until within R, or the timeout
wait 2

capture event grounded on_ground # frame N | time T | event NAME | end
capture end final

assert walk_violations == 0      # metric  op  threshold
assert frame_ms_p95 < 12
```

**Use `pose_ground`, not `pose`.** An absolute `pose` is a number that has to be right about a
world it cannot see; the first `walk_shoreline` was authored 18 m inside a hill and every capture
was a flat grey square. `pose_ground` resolves against `HeightmapGenerator::height_at`, so it also
survives Prompt 006 replacing the terrain.

Events: `tree-swapped`, `grounded`, `slow-frame`, `world-ready`.
Metrics: `frames`, `frame_ms_{mean,median,p95,p99,max}`, `slow_frames`, `slow_frames_{on_swap,uploading,building}`,
`gpu_ms_{mean,p95,max}`, `gpu_memory_mb`, `resident_bricks`, `resident_mb`, `contrast_percent`,
`golden_distance`, `walk_violations`, `uploads`.
`include other.scn` pulls in a shared fragment; it resolves relative to the including file and
rejects cycles.

**Assert on percentiles, never on fps.** This machine's 165 Hz FIFO_RELAXED panel pins fps at
155–159 regardless of headroom, so an fps assertion cannot fail for the reason you want it to.

## Goldens

`dev/goldens/<scenario>/<vk|d3d12>/<capture-name>.png`. **Per backend**, because vk and d3d12
differ by 12.8% of pixels at the same pose with the same settings — 37× the noise floor.

```bash
voxel_harness --scenario macro_ground --backend vk --accept-golden
```

A promotion prints the distance it is about to erase. A failure writes
`<backend>_<name>_diff.png` — unchanged pixels dimmed and grey, changed pixels magenta — so you can
see *where*, not just read a number.

Two gates, both measured rather than chosen: mean absolute difference ≤ **1.2**/255 and changed
pixels ≤ **1.5%**. The full four-case calibration table is in `dev/harness/src/image_compare.hpp`.
Two consequences worth knowing before you write a scenario:

- **Capture scenarios run `--no-taa`.** With TAA on, vk-vs-vk moves 0.996% of pixels; with it off,
  0.34%.
- **The overlay is off** under the harness unless a scenario says `option --overlay`. Its fps and
  VRAM digits change every run, and a golden containing them is holding still a picture of
  something that never holds still.
- **A one-pixel change cannot be caught.** The noise floor is ~3,100 of 921,600 pixels. What this
  metric does catch is a shading-term change; a deliberate `--grain 0.5` reads 7.2%.

## ctest

```bash
ctest --preset windows-relwithdebinfo -L scenario
```

The cheap headless scenarios. Plain `ctest` still runs the unit tests and needs no GPU.

## GPU counters

Timestamp queries are in the report already (`gpu_ms`). For occupancy, warp-stall reasons and
cache hit rates, drive **Nsight Graphics externally against a harness scenario** — see
`docs/gpu-counters.md` for the exact invocation and why it is not built into the app.

## A note on repository size

`dev/goldens` is **21 MB** for ten scenarios × two backends × 1280×720 PNG. That is committed
deliberately — a golden nobody else can check out is not a shared reference — but it churns: every
re-promotion writes a new 21 MB into history permanently, and Prompts 005 and 006 both change the
look on purpose. **If `dev/goldens` passes ~100 MB of history, move it to Git LFS or an out-of-tree
store** rather than deleting scenarios to stay under a limit. The `dev/baselines/*.json` reports
are 124 KB for the same ten scenarios and carry every number; they are the cheap half and should
always be committed.
