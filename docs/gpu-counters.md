# GPU counters: the decision, and the recipe

Goal 222 asked for a decision with reasoning, not a preference. This is it.

## The decision: Nsight Graphics, driven externally against a harness scenario. Not in-app.

The alternative on the table was NVIDIA's **Nsight Perf SDK**, which genuinely can do what was
asked from inside the process — `research/gpu-voxel-streaming-and-profiling-research.md` §5(d)
quotes NVIDIA directly: *"Integrate GPU performance metric collection into your application…
Activate profiling from your own custom programmatic triggers"*, with SM occupancy, warp-stall
reasons and L1/L2/VRAM throughput, and NVIDIA markets it explicitly for CI perf-regression gating.
It is a real option, not a straw man.

Four reasons it is the wrong one **for this project, now**:

1. **The gating question is already answered by something free.** Timestamp queries are
   unprivileged, permanently enabled, and already in every harness report as `gpu_ms`. A frame
   budget is asserted on `gpu_ms_p95` over a fixed camera path. Nothing in Prompt 004's plan is
   blocked on occupancy numbers; what it is blocked on is *which pass* costs what, which is goal
   220's per-pass ranges, not a counter.
2. **The counters are permission-gated on exactly the two machines that matter.** §5(d):
   `ERR_NVGPUCTRPERM` — hardware counter access needs a per-machine driver setting on Windows and
   possibly `CAP_PERFMON`/root on Linux. An in-app path would therefore work on this laptop after a
   manual setting and fail on the GitHub runner, which is the *opposite* of where a build-time
   dependency earns its keep.
3. **It is a dependency with a licence and a redistribution story**, entering the build for a
   number that is read a handful of times per pass by one person on one machine. This project's
   standing rule is that a dependency needs a written case; the case here comes out negative.
4. **The measurement it would give is not one this project can act on yet.** Warp occupancy is
   actionable when the traversal loop is being restructured — which is Prompt 004's AK-E/AK-F, and
   an external capture is enough to steer that. If 004 reaches the point where a counter has to be
   read *per frame, in a loop, under automation*, that is the moment to reopen this and the reason
   to.

**What would change the answer:** a perf regression that timestamps cannot localise (the pass times
are flat but the frame is slower), or a CI runner with counter access. Either one makes the SDK's
programmatic triggers worth their cost.

## The recipe

Nsight Graphics is driven against a **harness scenario**, so the thing being profiled is the same
thing the report measured — same pose, same script, same options, reproducible tomorrow.

```bash
# 1. Enable GPU performance counter access once, as Administrator:
#    NVIDIA Control Panel -> Desktop -> Enable Developer Settings,
#    then Help -> Developer -> "Allow access to the GPU performance counters to all users".
#    (Without it every capture below fails with ERR_NVGPUCTRPERM.)

# 2. Launch under Nsight Graphics' GPU Trace activity:
"C:\Program Files\NVIDIA Corporation\Nsight Graphics 2026.1\host\windows-desktop-nomad-x64\ngfx.exe" ^
  --activity "GPU Trace Profiler" ^
  --exe "C:\b\windows-relwithdebinfo\dev\harness\voxel_harness.exe" ^
  --args "--scenario stress_pose --backend vk --frames 900" ^
  --dir "C:\Users\Tonyt\Documents\GitHub\zxbio-fueld-game-research"
```

`stress_pose` is the right scenario to profile: `CLAUDE.md` records 76 fps there against 155–159
panoramic, so it is where a GPU budget is actually set. Use `--backend vk` — Nsight's Vulkan trace
is the richer of the two.

What to read, in order: **SM throughput and warp occupancy** (is the marcher latency-bound or
throughput-bound), **Ray Tracing Live State / register pressure** (the traversal loop's live set is
the classic cause of low occupancy in an SVO marcher), **L1TEX and L2 hit rates** (whether the
brick pool's access pattern is coherent), **VRAM throughput** (whether it is bandwidth-bound at
all).

For a *frame* capture rather than a trace, swap `--activity "Frame Debugger"`. `--frames N` makes
the process exit on its own, which is what lets this run unattended.

## What was actually measured

See `research/dev-harness-log.md` §"GPU counters" for the numbers this recipe produced on
`stress_pose`, and for what they say about Prompt 004's starting point.
