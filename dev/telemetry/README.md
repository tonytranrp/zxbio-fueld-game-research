# `dev/telemetry` — one frame report, two consumers

## What belongs here

The per-frame record and the statistics over it:

- `frame_record.hpp` — `FramePhases` (the six phase times plus the four cause flags) and
  `FrameRecord` (a phase breakdown plus the counters). This is the struct that used to be declared
  *inside* `run_svo`, where only `run_svo` could see it.
- `frame_report.hpp` / `src/frame_report.cpp` — `FrameReport`: accumulate records, then answer
  count / mean / median / p95 / p99 / max, a histogram, slow frames by cause, and the worst five
  frames with their full breakdown.
- `json.hpp` / `src/json.cpp` — a ~90-line object/array/value writer. See the note in `json.hpp`
  for why this is not a dependency.

## What does not belong here

Anything that knows how to *produce* a frame. This module is handed numbers; it does not call
Diligent, does not own a clock, and builds with `-DVOXEL_BUILD_RENDERER=OFF`.

## The rules of this folder

1. **`voxel_app`'s 2-second stats line and `voxel_harness`'s report derive from the same object.**
   The pass before this one shipped an fps/ms mismatch precisely because two numbers were computed
   in two places; the fix then was to derive both from one smoothed frame time, and the rule here
   is the same one generalised.
2. **The six phases must account for the frame.** `FrameReport::phase_coverage()` reports the
   fraction of wall time the phases add up to. If it is not ~1.0, a phase is missing — and that is
   a finding to write down, not a tolerance to widen.
3. **Percentiles, not means, and never fps.** This machine's 165 Hz FIFO_RELAXED panel pins fps
   readings at 155–159 regardless of headroom
   (`research/gpu-voxel-streaming-and-profiling-research.md` §6.8), so an fps assertion cannot fail
   for the reason you want it to.
4. **A record is cheap.** One `FrameRecord` per frame is ~120 bytes; a 900-frame run is 100 KB.
   Nothing here samples, decimates or rolls up during a run — a report you cannot go back to the
   worst frame of is a report that will make you re-run.
