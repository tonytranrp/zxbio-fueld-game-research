# Engine Hardening & Optimization — Master Task List

Companion to every prior brief. Scope: before more feature work, harden the pieces you've flagged as
built quicker than they should have been — the coordinate map, the task queue, the crash handler —
and go deep on the thing that actually matters most for a voxel engine: how compact the data is on
both sides of the CPU→GPU boundary, and how you'd know if any of this actually helped.

## 0. Reconciling with what's actually been found — read this before anything else

Two real bugs came back from Group D, and they change what this document argues, so they're
addressed directly rather than researched around.

The `ChunkStore::find` ~80× collapse was a debug-build-specific iterator lock, already fixed — this
document doesn't re-litigate that fix, and doesn't oversell the hash-map swap because of it. The
root cause was MSVC's `_ITERATOR_DEBUG_LEVEL=2` global lock around iterator construction/
destruction in a Debug build, not an inherent property of `std::unordered_map` that a different map
would obviously have avoided — in Release, that specific lock mostly disappears. The `NeighborCache`
fix (resolve each of a chunk's 27 relevant neighbors once per extraction instead of once per padded
sample) was the right fix and it's done. So the honest framing for §3's hash-map work below is
hardening, not firefighting — real, evidenced, cheap to do, but justified by chunk-granularity
lookups elsewhere (streaming's desired-set diffing, `ChunkStore` insert/erase on load/unload) and by
defense-in-depth, not by re-opening an already-closed incident.

The teardown crash and the M1.1 `ThreadPool` bug are the same root cause, one level up — worth
naming as a pattern, not two unrelated incidents. Both are "a thread-owning member destructed before
something its threads might still touch." M1.1: the mutex/condvar destructed before the workers that
used them. Group D: `ChunkStreamingSystem`'s `ThreadPool` needed to be declared last specifically so
its destructor (which joins/drains workers) runs before the queues/mutexes/heightmap generator those
workers capture go out of scope. Two independent occurrences of the same class of bug is a real
signal this codebase needs a standing rule, not just two separate fixes — §7 makes it one.

The GPU question from `PROJECT_BRIEF.md` §14 is resolved: an RTX 4070 Laptop GPU, confirmed at
runtime on both Vulkan 1.4.341 and D3D12, not just at build time. Worth noting since it means every
task below that needs a live GPU (most of §5's compression work) has one confirmed available, unlike
earlier in this project.

Table of contents: §1 Framing · §2 The 3 subagent prompts · §3 Hash map · §4 Task queue ·
§5 Crash handling · §6 GPU/voxel compression · §7 The systemic pattern · §8 Event system ·
§9 The task list (~58 tasks) · §10 External libraries · §11 Guardrails

## 1. Framing — hardening, not premature optimization

Worth stating explicitly rather than assumed: this document's groups (§9) are largely independent of
each other, unlike the strict A→B→C→D dependency chain in `PHASE_1_COMPLETION_BRIEF.md`. The hash
map, the task queue, the crash handler, GPU compression, and the event system don't block each
other — they can proceed in whatever order fits, or in parallel across sessions. That's also why
this is framed as a "GOAL" document per the request: a standing checklist to iterate against, not a
waterfall to execute once top to bottom.

And this genuinely isn't "optimize everything speculatively" despite covering a lot of ground at
once — every recommendation below is either (a) responding to a real, already-measured finding (§0),
or (b) grounded in real, dated, cited evidence that the current approach has a well-documented
better alternative (§3–§6), never "this feels like it should be faster."

## 2. The 3 subagent prompts

Per the explicit constraint: exactly three, each broad and multi-task rather than one-library-each,
each instructed to return a set of real candidates with evidence — not a single pick — because the
main session is the one that verifies and decides, per `library-research.md` §3's own read-only/
report-is-the-deliverable rules, extended here with an explicit verification step neither prior
subagent design in this project has needed before now.

### Subagent 1 — Concurrency & core data-structure libraries

```
You are a read-only research subagent surveying C++ libraries to potentially replace two hand-rolled
components in a voxel game engine: a coordinate-keyed hash map and a thread-pool task queue. Your
report is a survey with a SET of real candidates and evidence for each — not a single recommendation.
The orchestrating session will independently verify your top candidates before adopting anything; do
not present a verdict as if it's final.

PROJECT CONTEXT: C++20 voxel terrain engine (DiligentEngine/Vulkan rendering, EnTT, GLM, GLFW,
FastNoise2, CMake+CPM.cmake, MSVC on Windows). A `ChunkStore` maps 3D integer chunk coordinates to
owned chunk data, looked up frequently from multiple `std::jthread` worker threads during terrain
generation and Naive-Surface-Nets meshing. A hand-rolled `ThreadPool` (std::jthread workers +
std::mutex + std::condition_variable_any + std::queue<std::function<void()>>) schedules generation
and meshing jobs; a newer `ChunkStreamingSystem` also owns and submits through a `ThreadPool`
instance. A real, measured bug already occurred here: under MSVC Debug, `_ITERATOR_DEBUG_LEVEL=2`
serialized concurrent `ChunkStore::find` calls via a global lock (fixed via caching neighbor lookups,
not by changing the map) — so any hash-map candidate's behavior under MSVC's iterator debug checking
specifically is relevant and worth investigating, not just Release-mode throughput.

TASK 1 — Hash map candidates. Survey at least these by name, plus any other genuinely competitive
current option you find: `ankerl::unordered_dense` (map and the reference-stable `segmented_map`
variant), `absl::flat_hash_map`, `boost::unordered_flat_map`, `tsl::robin_map`. For each, using
`library-research.md`'s rubric ((a) what it is, (b) integration cost — header-only vs. needing a
larger ecosystem like Abseil or Boost, CPM-fetchability, (c) real benchmark numbers with sources, (d)
compile-time cost, (e) dated maintenance evidence and exact license, (f) real production adoption):
report findings. Specifically investigate and report on: (i) whether each candidate provides pointer/
reference stability across insert/rehash comparable to `std::unordered_map`'s guarantee — this
project may rely on that guarantee somewhere and a silent behavior change on swap is a real
correctness risk, not just a performance question; (ii) whether the key type matters — chunk
coordinates are packed 3D integers, and integer hash quality varies significantly by library (some
use the identity function by default, which clusters badly for spatially-nearby keys); (iii) whether
MSVC's iterator-debug-level checking applies to each candidate's iterators the same way it applies to
`std::unordered_map`'s, if you can determine this from source or documentation.

TASK 2 — Task queue / thread pool candidates. Survey at least: `moodycamel::ConcurrentQueue` (and its
`BlockingConcurrentQueue` variant specifically, since the current design needs blocking-wait
semantics for idle workers), `BS::thread_pool`, oneTBB's task system. For each, the same rubric, plus:
(i) whether it preserves per-task result retrieval (`std::future`-equivalent) the current
`ThreadPool::enqueue` API provides; (ii) its ordering guarantees (global FIFO vs. per-producer FIFO
vs. unordered) and whether that's a meaningful behavior change for chunk generation/meshing jobs
specifically (largely independent, order-insensitive work — assess whether that assumption is safe or
whether some ordering dependency has crept in); (iii) how cleanly it composes with the existing
`std::jthread` + `std::stop_token` cooperative-cancellation pattern, since that's a deliberate,
already-established choice (`concurrency-and-parallelism.md` §1) this project doesn't want to lose.

TASK 3 — Synthesis. For each area, close with a short comparison table (candidate × the rubric axes)
per `library-research.md` §5's "stays broad" format, and one paragraph on which candidate you'd
personally rank first and why — clearly labeled as your assessment for the orchestrator to verify,
not as a decision already made.

RULES: WebSearch/WebFetch only, no Edit/Write, no Skill tool, no spawning further subagents. Every
claim needs a real source URL; if something can't be verified, say so explicitly rather than guessing
or inventing a plausible-sounding number. Long, structured, complete report — this is the deliverable
itself, not a summary of one.
```

### Subagent 2 — Crash handling, debugging & benchmarking tooling

```
You are a read-only research subagent surveying C++ crash-handling and benchmarking tooling for a
solo-developer voxel game engine project. Your report is a survey with a SET of real candidates and
evidence for each — not a single recommendation; the orchestrating session verifies before adopting.

PROJECT CONTEXT: C++20 voxel terrain engine, Windows/MSVC primary target, DiligentEngine (Vulkan/
D3D12), EnTT, GLM, GLFW, FastNoise2, CMake+CPM.cmake. A hand-rolled SEH-based crash handler
(`app/src/crash_handler.cpp`) currently exists, added specifically to catch an access-violation bug
during multithreaded teardown, and is considered under-built. Tracy (real-time profiler, ~15ns/zone
overhead, already being integrated) and Google Benchmark (named in this project's own house-style
reference docs as the standard micro-benchmark harness, not yet added as a dependency) are both
relevant to the benchmarking half of this research.

TASK 1 — Crash/stack-trace handling candidates. Survey at least: `bombela/backward-cpp` (or an
actively-maintained fork if the canonical repo looks stale — check dated evidence either way),
Crashpad, and Google Breakpad (note: Sentry's own documentation states Breakpad is deprecated in
favor of Crashpad — confirm this independently and report what you find, don't just take one source's
word for it). For each: what it actually does (local stack-trace pretty-printing vs. full minidump-
plus-upload-pipeline — these solve different problems and the report should be explicit about which),
integration cost specifically on Windows/MSVC with CMake+CPM, real evidence of current maintenance
and Windows support quality (search for recent open issues about Windows-specific problems, not just
the README's claims), and a clear-eyed assessment of whether the heavier options (Crashpad/Breakpad)
are proportionate for a solo-dev project with no crash-report server/telemetry pipeline, versus a
lighter local-stack-trace-only tool. Also investigate: can whichever tool you'd rank first coexist
with or wrap the existing custom SEH handler, or does adopting it mean replacing that handler
entirely — report what you find rather than assuming a clean drop-in.

TASK 2 — Benchmarking methodology and tooling. Google Benchmark: confirm its current CPM-fetchability,
CMake integration pattern, and survey at least 2 real, current examples of a game/graphics engine
project using it alongside a real-time profiler like Tracy (i.e., Google Benchmark for isolated,
repeatable micro-benchmarks of specific functions — hash map ops, compression encode/decode, mesh
extraction — versus Tracy for live, in-context frame-time/timeline profiling) — report what
methodology those real projects actually use to decide "did this optimization help," including
whether they track benchmark results over time (regression detection) and how. Also specifically
research: does Google Benchmark's `DoNotOptimize`/`ClobberMemory` pattern interact correctly with
MSVC specifically (it's originally a GCC/Clang-oriented library) — report any known MSVC-specific
caveats or workarounds you find, don't assume it's identical across compilers.

RULES: WebSearch/WebFetch only, no Edit/Write, no Skill tool, no spawning further subagents. Every
claim needs a real source URL; say so explicitly when something can't be verified. Long, structured,
complete report, comparison tables per `library-research.md` §5 for both tasks, closing with your own
ranked assessment clearly labeled as an assessment for the orchestrator to verify, not a decision.
```

### Subagent 3 — GPU/voxel data compression & transfer

```
You are a read-only research subagent going deep on GPU-side voxel mesh data compression for a
Vulkan-via-DiligentEngine voxel terrain engine. This is the highest-priority research area of the
three subagents launched — treat it accordingly. Your report is a survey with real, cited findings —
not invented plausible-sounding numbers.

PROJECT CONTEXT: Naive Surface Nets meshing already produces one vertex per active cell (position +
normal + a small material ID) per chunk (chunk size 32³, chunk-local vertex positions therefore range
0–32 per axis with sub-voxel fractional precision from edge-crossing averaging). Current vertex
layout is uncompressed: float3 position (12B) + float3 normal (12B) + a material ID field, uploaded
as an interleaved (AoS) vertex buffer to DiligentEngine, targeting Vulkan primarily (also D3D12,
OpenGL, confirmed all building/running). CPU-side chunk voxel storage already uses a real palette
compression scheme (bit-packed indices into a small per-chunk distinct-material palette, with a
zero-cost fully-homogeneous fast path) — this research is about what's next, not re-deriving that.

TASK 1 — GPU vertex compression, in depth. Confirm and go deeper than a single source on: octahedral
normal-vector encoding (real, current production precedent already found in Godot, Blender, Unity's
mesh-compression presets, and Bentley's iTwin.js — verify these independently and find at least 2
more real production or well-cited academic references, ideally including the original Cigolle et al.
JCGT paper and Krzysztof Narkowicz's widely-cited blog post if you can locate and characterize them);
real bit-budgets other engines actually ship (Unity's documented 10/6/8-bit position/normal/UV
quantization is one data point — find others, and specifically anything about *chunk-local* (small,
fixed-range) position quantization specifically, as opposed to arbitrary-bounding-box position
quantization, since chunk-local range is much smaller and likely needs fewer bits for equivalent
precision — do the arithmetic and show it, don't just assert it); real, measured decode-cost
tradeoffs (the Blender PR context mentioned measurable overhead from octahedral decode — find more
data on how expensive this actually is in a vertex shader specifically, on both AMD/NVIDIA modern
GPUs if such data exists).

TASK 2 — HLSL/cross-backend correctness for bit-packed vertex attributes. This project cross-compiles
HLSL to GLSL/SPIR-V/MSL via DiligentEngine — research what HLSL bit-manipulation intrinsics
(`f16tof32`/`f32tof16`, bit shifts/masks on `uint`, `asuint`/`asfloat` reinterpretation) are needed to
unpack a compressed vertex attribute in a vertex shader, and whether there's any known
cross-backend inconsistency in how these specific intrinsics translate through Diligent's shader
converter or through SPIRV-Cross/glslang generally — this is a real risk area (a bit-packing scheme
that works on the primary backend but silently corrupts on another isn't hypothetical for this
project, which explicitly targets multiple backends). Report what you find, including if you can't
find a definitive answer and it needs to be verified empirically instead.

TASK 3 — Voxel storage compression beyond the current palette scheme. Research whether layering
run-length encoding (RLE) on top of an already-paletted index stream is a real, established technique
worth the added complexity for a project already getting most of the win from palette compression's
homogeneous-chunk fast path — find real sources (established voxel engines, papers, technical blogs)
discussing this specific combination and what workloads make it worth it vs. not. Also research
real GPU upload/transfer patterns specifically for streaming many small mesh buffers frequently (this
project's chunk streaming creates/destroys GPU buffers continuously as chunks load/unload) — staging
buffer reuse pools, persistent-mapped buffers, or whatever DiligentEngine itself documents or its
samples demonstrate as the idiomatic pattern for this specific access pattern (not a one-time asset
load, which most tutorials assume).

RULES: WebSearch/WebFetch only, no Edit/Write, no Skill tool, no spawning further subagents. Every
claim needs a real source URL; explicitly flag anything you couldn't verify rather than inventing a
plausible number — this is the research area where a fabricated benchmark would be most damaging to
trust. Long, structured, complete report; don't truncate for brevity. This is the deliverable the
orchestrating session will read to design the actual compressed vertex format.
```

The verification step this document requires, stated once here rather than per-task: for each
subagent's top-ranked candidate, before writing any code that depends on it — confirm the repository
and license independently (don't take the subagent's citation on faith), do a trivial standalone CPM
fetch-and-build smoke test in isolation before wiring it into the real engine, and re-check any
claim that materially drives the decision (a specific benchmark number, a specific API guarantee)
against the primary source directly. This is what "the main agent checks it rather than just use it"
means operationally — §9's Group G makes it concrete per subagent.

## 3. Hash map — research done this pass

Real, dated, cross-referenced numbers, not a single vendor's claim. From Boost.Unordered's own
benchmark repository (a real, current comparative harness, string-key workload — the closest
available proxy; chunk-coordinate keys will differ in absolute numbers but the relative ranking is
well-established across many independent sources, not just this one): `std::unordered_map` at
36,883ms baseline; `boost::unordered_flat_map` at 12,346ms (a single 323MB allocation — genuinely
contiguous); `ankerl::unordered_dense::map` fastest of the mainstream options at 10,966ms, in just 2
allocations total. `ankerl::unordered_dense` itself (confirmed from its own repository) stores data
in a contiguous `std::vector` — "perfect iteration speed" by construction — and is described as
performing in the same range as `absl::flat_hash_map`, one of the most respected options in this
space.

The one real caveat, worth having found before it becomes a bug: `ankerl::unordered_dense::map`'s
base variant does not guarantee pointer/reference stability across insert or rehash the way
`std::unordered_map` does — its own documentation is explicit about this tradeoff. If `ChunkStore`
or any caller holds a raw pointer/reference into the map across a subsequent insert anywhere (worth
Subagent 1 and a direct local check confirming either way), the `segmented_map` variant trades a
small amount of indexing speed (one extra indirection) for stable references and still meaningfully
outperforms `std::unordered_map`. This is exactly the kind of "what could break it" a naive drop-in
swap would miss.

Also worth having in hand: integer hash quality varies meaningfully by library — several default to
treating an integer key as its own hash (the identity function), which is fine for a
well-distributed key but can cluster badly for spatially-nearby keys packed a certain way (chunk
coordinates are exactly this kind of key) — worth confirming whatever candidate is chosen either
provides or is paired with a real mixing hash for the packed `ChunkCoord` type specifically, not
assumed adequate by default.

## 4. Task queue — research done this pass

`moodycamel::ConcurrentQueue` is real, current, and has a track record beyond its own marketing:
NVIDIA's Holoscan SDK (current, v4.2.0) uses it directly as its `LockFreeQueue` implementation; a
2023 ISO C++ SG14 (the low-latency/games/finance study group) mailing-list thread has a
self-identified HFT engineer stating it's "the go to lock-free mpmc queue at most places" in that
industry. Concretely: single-header, C++11, move-based, supports bulk enqueue/dequeue (materially
faster than one-at-a-time even under contention, per its own documented design), and ships a
`BlockingConcurrentQueue` variant specifically — this is the one that matters here, since it
preserves the blocking-wait-when-idle semantics the current `std::condition_variable_any`-based
workers need, meaning the swap can keep the existing `std::jthread` + `std::stop_token` worker-loop
shape (§7's cooperative-cancellation pattern stays) and only replace the lock-protected
`std::queue<std::function<void()>>` underneath it.

The one real semantic change worth stating explicitly, not discovering by surprise: per-producer
FIFO, not global FIFO — items from the same submitting thread stay ordered relative to each other,
but interleaving across different submitting threads isn't guaranteed. For largely-independent chunk
generation/meshing jobs this is very likely a safe trade, but it's a genuine behavior change from
the current single global queue, worth a one-line confirmation that nothing implicitly depends on
global submission order before treating the swap as risk-free.

## 5. Crash handling — research done this pass

Breakpad is deprecated — confirmed directly from Sentry's own documentation ("Breakpad is the
predecessor of Crashpad... please consider using our Crashpad integration instead"), so it's ruled
out as a fresh choice regardless of its long history. Sentry's own "Backend Tradeoffs" doc gives
real, current reasoning for Crashpad's advantages where it's used: an out-of-process handler
(survives corruption in the crashed process, snapshots immediately rather than waiting for next
launch), broader crash-type coverage on Windows/Linux (`abort()`, fast-fail, heap corruption), more
active upstream maintenance. But Crashpad has real, evidenced integration friction — a live GitHub
issue shows a developer's Windows Crashpad build hanging and producing 0-byte dump files, a
genuinely concrete signal its external-handler-process architecture isn't a trivial integration,
consistent with what that architecture inherently costs.

For this project specifically, that architecture is disproportionate — there's no crash-report
server or field-telemetry pipeline; the actual need is "print a good, readable stack trace locally
when a dev build crashes," which is a materially smaller problem than Crashpad/Breakpad solve.
`bombela/backward-cpp`, confirmed real and header-only (`backward.hpp` plus a trivial
`backward.cpp`, CMake-integrable via `add_subdirectory` + `add_backward(target)`), fits that smaller
problem directly — current documentation confirms Windows support (an older mirror's docs claimed
Linux-only, so this is worth Subagent 2 double-checking isn't a stale claim either way) and needs
debug symbols present (`/Zi`-equivalent PDB generation — worth confirming this is already on for
Debug and deciding deliberately whether to keep it on for Release too, since a Release crash with no
symbols is exactly the situation a crash handler exists to prevent).

What this doesn't resolve on its own: whether Backward-cpp's stack-trace printing replaces the
existing custom SEH handler outright, or needs to be called from inside it (the SEH handler is what
actually intercepts the access violation on Windows; Backward-cpp's contribution is turning "we
crashed" into "here's a readable trace," which are different jobs that likely compose rather than
one replacing the other) — Subagent 2's task 1 is scoped specifically to answer this rather than
assuming a clean swap.

## 6. GPU/voxel compression — research done this pass

The area named as most important, so the grounding here is the deepest — four independent, real,
current production/academic references, not one blog post: Godot has shipped octahedral normal/
tangent compression (`vec4`→`vec2` per attribute, a real merged PR) explicitly to cut vertex
bandwidth, particularly called out as valuable on mobile — the same bandwidth-sensitivity argument
applies to a CPU that's uploading many small chunk mesh buffers continuously as terrain streams.
Blender has a similar effort in progress citing the same foundational sources this pass also
surfaced: Krzysztof Narkowicz's widely-cited encoding technique and Cigolle et al.'s JCGT paper on
efficient unit-vector representations — with an honest, measured note that decode does add some real
overhead, not a free lunch, though cheap enough in a vertex shader that Blender's own PR still
considers it worth shipping. Unity's own documented "High" mesh-compression preset quantizes
position/normal-tangent/UV to 10/6/8 bits per component respectively — real, currently-documented,
shipped numbers, not a guess. Bentley's iTwin.js (a real, current production BIM rendering engine)
ships a documented `addQuantizedVertex` API taking normals as a 16-bit `OctEncodedNormal` directly —
independent confirmation of exactly the encoding width worth targeting here.

Worth doing the arithmetic explicitly, cross-checked, rather than asserting "smaller" — his own
standing rule for any compression claim. Current uncompressed vertex: `float3` position (12B) +
`float3` normal (12B) + a material field (assume 1B with padding) ≈ 28B/vertex, including alignment.
A compressed layout — quantized chunk-local position (chunk-local range is only 0–32 per axis, far
smaller than an arbitrary mesh's bounding box, so meaningfully fewer bits than Unity's generic
10-bit preset likely still gives more relative precision here — a concrete number for Subagent 3 to
pin down, provisionally ~12–16 bits/axis ≈ 4.5–6B packed for all three axes given the small range
and the sub-voxel fractional part Surface Nets needs) + a 16-bit octahedral normal (2B, matching
iTwin.js's shipped precedent directly) + material ID (1B) ≈ roughly 7.5–9B/vertex — first pass:
`(4.5 to 6) + 2 + 1`. Second, independent cross-check by ratio rather than re-adding the same
numbers: the normal field alone drops from 12B→2B, a 6× reduction on that one field, consistent with
Godot's own "vec4→vec2" framing (a 2× reduction per Godot's specific 2-component-vs-4-component
comparison, times a further factor from `float`→`byte`-scale packing within those components) — both
routes land in the same rough 3–3.5× total per-vertex reduction versus the uncompressed 28B
baseline. Real chunk-count/vertex-count numbers from the actual engine (already available via §0's
GPU-memory tracking work) are what turn "roughly" into an exact, measured number — that's explicitly
Group K's job (§9), not asserted here as final.

The real, non-obvious risk worth flagging before it's discovered as a rendering bug on one backend
and not another: this project cross-compiles one HLSL source to Vulkan/D3D12/OpenGL via Diligent's
shader converter — bit-manipulation intrinsics needed to unpack a compressed attribute in the vertex
shader aren't guaranteed to behave identically across every translation path by default just because
the un-compressed shader already builds and runs correctly cross-backend (confirmed in Group B, but
that shader does no bit manipulation at all). Subagent 3's task 2 is aimed directly at this before
it becomes a "renders correctly on Vulkan, garbled on D3D12" bug discovered the hard way.

## 7. The systemic pattern: thread-owning members declared last

Stated as a standing rule, not a one-off fix, given it's now caused two separate real bugs: any
class that owns both a `std::jthread`/`ThreadPool`-family member and other state that member's
threads might still touch during shutdown must declare the thread-owning member last — C++ destructs
members in reverse declaration order, so declaring it last means its destructor (which joins/drains
workers) runs before anything those workers capture goes out of scope, not after. `ThreadPool`
itself already had to learn this the hard way once (M1.1); `ChunkStreamingSystem` needed the same
lesson applied one level up (Group D). §9 (Group O) makes checking every other thread-owning class
in the codebase for the same risk an explicit task, not an assumption that two fixes means it's
handled everywhere.

## 8. Event system: `entt::dispatcher`, not a new dependency

`modular-architecture.md` §1's own reference folder layout names `events/dispatcher.hpp` as "the ONE
shared dispatch mechanism" worth having — and the project's already-pinned EnTT (confirmed from
EnTT's own current documentation, matching or older than the pinned v3.16.0) ships exactly this:
`entt::dispatcher` with `trigger<T>(event)` for immediate/synchronous dispatch, `enqueue<T>(args...)`
to queue an event for later, and `update<T>()`/`update()` to flush queued events to connected
listeners (`sink<T>().connect<&Handler::method>(instance)`). Zero new dependency — this is already
sitting in the CPM cache. A natural first real use: chunk lifecycle (loaded/unloaded/mesh-ready) as
events other systems (the debug overlay, a future save system, a future audio/ambience system) can
subscribe to instead of polling `ChunkStore`/ECS state directly — named as Group L's proof case (§9)
rather than a speculative "add an event system" with nothing concrete hanging off it yet.

## 9. The task list (~58 tasks)

Groups are independent of each other (§1) — pick any order. Every task states its own check, per the
request that each one "needs checks to perform," not just a description of the work.

### Group G — Verification (do this per-subagent-finding, not once)

1. For Subagent 1's top hash-map candidate: independently confirm the repo/license/current release
   date directly (not from the subagent's citation alone). Check: you've personally opened the real
   repository, not just trusted a quoted snippet.
2. For Subagent 1's top task-queue candidate: same independent confirmation. Check: same as above.
3. Standalone CPM smoke-test each of the two, in isolation, before touching `ChunkStore` or
   `ThreadPool`. Check: a throwaway target that fetches and links the candidate and calls one
   trivial API, builds clean under the real MSVC toolchain from `CLAUDE.md`.
4. For Subagent 2's top crash-handler candidate and Subagent 3's top compression references: the
   same independent-confirmation step. Check: same standard as tasks 1–2.
5. Decide, in writing (a short note in `CLAUDE.md` or `research/`), the fallback for each of the
   three subagent areas if the top candidate fails its smoke test. Check: the fallback is a named
   second candidate from the same subagent's survey, not "figure it out later."

### Group H — Hash map hardening

6. Confirm whether anything currently holds a pointer/reference into `ChunkStore`'s map across an
   insert — the concrete question §3's caveat raises. Check: a grep-and-read pass through every
   `ChunkStore` consumer, answered explicitly yes/no with the specific call sites if yes.
7. Based on task 6, pick `ankerl::unordered_dense::map` (no cross-insert pointer reliance found) or
   `segmented_map` (reliance found) as the concrete target — or Subagent 1's alternate top pick if
   its findings changed the picture. Check: the decision is written down with the task-6 evidence
   it's based on, not just asserted.
8. Confirm/design the hash function for the packed `ChunkCoord` key specifically — don't inherit
   whatever the default happens to be without checking it avalanches well for spatially-clustered
   keys (§3). Check: a small standalone test feeding a realistic run of nearby chunk coordinates
   through the chosen hash and inspecting bucket distribution, not assumed adequate.
9. Migrate `ChunkStore` behind a type alias (`using ChunkMap = ...;`) rather than the concrete type
   spelled out at every use site, so a future re-evaluation doesn't mean another full migration.
   Check: exactly one line changes if the underlying map type changes again.
10. Audit for a second hash/coordinate map candidate: `world/streaming`'s per-chunk unload-delay
    timer tracking is a strong candidate (§7's "check everywhere" spirit, applied to data structures
    too). Check: found and listed explicitly, or explicitly confirmed not to exist.
11. Migrate whatever task 10 found, if anything, using the same type-alias pattern from task 9.
    Check: same as task 9.
12. Google-Benchmark the swap, chunk-granularity (not the already-fixed per-sample path) — insert/
    find/erase at realistic loaded-chunk-set sizes (the ~528–588 range Group D's autofly run
    established as realistic), before and after. Check: a real before/after number in the benchmark
    output, not "should be faster."

### Group I — Task queue hardening

13. Confirm Subagent 1's task-queue findings against the specific requirement that `ThreadPool`'s
    current `enqueue()` returns a `std::future`-compatible handle — does the chosen candidate
    preserve this, or does adopting it change every call site's API. Check: answered explicitly with
    the exact resulting call-site signature.
14. Redesign `ThreadPool` keeping the `std::jthread` + `std::stop_token` worker-loop shape (§7's
    pattern stays) but replacing the mutex+`std::queue`+condvar with the chosen library's blocking
    queue. Check: the member-declares-last rule from §7 re-verified explicitly on the redesigned
    class, not assumed carried over from the old version.
15. Confirm nothing relies on the current global-FIFO ordering before treating per-producer FIFO as
    a safe trade (§4's flagged semantic change). Check: explicit yes/no with reasoning, not assumed.
16. Apply the same swap to `ChunkStreamingSystem`'s job submission — the exact code Group D's
    teardown bug already touched once. Check: the same TSan-unavailable caveat from `CLAUDE.md`
    applies — run the full existing streaming stress tests (the ones that caught both Group D bugs)
    under ASan at minimum, repeated, per the standard this project already set for `ThreadPool`
    itself in M1.1.
17. Benchmark job-submission throughput before/after under realistic concurrent load (the 16-worker
    stress scenario Group D's own bug report already used as its stress case). Check: real
    before/after numbers, ideally reproducing or improving on the "268s → ~3s" order of magnitude
    already seen from the other Group D fix, stated honestly if this swap's contribution is smaller.
18. Update `CLAUDE.md`'s dependency notes with this addition's pins and any MSVC-specific quirks
    found, matching the existing convention for every other dependency in that file.

### Group J — Crash handler hardening

19. Resolve Subagent 2's open question directly: does the chosen tool replace or wrap the existing
    SEH handler. Check: a one-paragraph written answer with the specific reasoning, before any code
    changes.
20. Integrate accordingly. Check: a deliberately-triggered test crash (a debug-only "crash now" code
    path, removed or `#ifdef`'d out before anything resembling a release build) produces a readable
    stack trace, not just "it compiles."
21. Confirm PDB/debug-symbol generation is deliberately configured for Debug, and make an explicit,
    written decision about Release (symbols stripped vs. kept separately) rather than whatever the
    CMake defaults happen to produce. Check: the decision and its reasoning are in `CLAUDE.md`.
22. Re-run the exact scenario that originally motivated the custom handler (Group D's teardown
    access violation, or a synthetic equivalent if that specific bug is already fixed) through the
    new/augmented handler. Check: the trace correctly identifies the actual crash site, not just "a
    crash was caught."
23. Document the crash-handler setup in `CLAUDE.md` at the same level of operational detail as every
    other machine-specific note already there (the ATL/git-shim/MAX_PATH precedent).

### Group K — GPU/voxel compression

24. Pin down Subagent 3's chunk-local position bit-budget with real arithmetic (§6's provisional
    12–16 bits/axis, refined). Check: the derivation is shown in full, not just the final bit count,
    per the standing "always show the full derivation" rule.
25. Implement octahedral normal encoding (CPU-side, in the meshing/upload path) at 16 bits, matching
    the iTwin.js precedent directly. Check: a standalone round-trip test (encode → decode) on a
    representative sample of Surface-Nets-generated normals, checking angular error against a stated
    acceptable threshold, not just "it compiles and looks fine."
26. Implement the corresponding HLSL decode (unpack position + octahedral normal) in the terrain
    vertex shader, informed directly by Subagent 3's task 2 findings on cross-backend intrinsic
    behavior. Check: `--verify-frame` (Group B's existing mechanical correctness check) passes
    identically on at least Vulkan and D3D12 with the compressed format, the same
    bit-identical-cross-backend standard Group B already established for the uncompressed path.
27. Update the vertex/index upload path and the §0-confirmed allocation-tracking hook for the new,
    smaller per-vertex byte count. Check: the tracked VRAM number visibly drops on the same autofly
    stress scenario Group D used (~528–588 loaded chunks), by roughly the ratio §6's arithmetic
    predicted — a real before/after comparison, not assumed.
28. Benchmark actual upload bandwidth/time before vs. after on the real streaming workload (chunks
    continuously loading during autofly), not just the static byte-count reduction. Check: a Tracy
    capture comparing the two, or a Google Benchmark harness isolating just the upload call.
29. Decide, with Subagent 3's task 3 findings in hand, whether RLE-on-top-of-palette is worth it for
    this project's actual workload — and write down the decision either way, including "no, and
    here's the specific reason" if that's the honest answer. Check: the decision cites real evidence
    from the subagent's findings, not a default assumption either direction.
30. If task 29 says yes: implement it behind the same kind of reversible boundary as task 9's type
    alias. Check: same standard as task 9.
31. Review GPU upload/transfer pattern against Subagent 3's task 3 findings on staging-buffer reuse
    for frequent small uploads specifically (as opposed to one-time asset loading, which most
    tutorials assume and this project's access pattern is not). Check: a written comparison of
    current behavior vs. what was found, with a concrete follow-up task if a change is warranted.

### Group L — Event system

32. Wire `entt::dispatcher` into `engine/core` (or a new `engine/events` module, per §7's
    folder-nesting convention if it grows past a thin wrapper). Check: a unit test connecting a
    listener, triggering/enqueuing an event, and confirming it fires — the same standard every other
    module in this project has met since M1.1.
33. Convert chunk lifecycle notifications (loaded/unloaded/mesh-ready) to dispatcher events as the
    proof case named in §8, replacing whatever polling currently serves this purpose in the
    debug-overlay work already in flight. Check: the debug overlay's chunk-count display still reads
    correct values, now sourced from events rather than a poll.
34. Document the event-vs-polling convention (`SKILL.md`-style: when to reach for one over the
    other) so it doesn't need re-deriving next time a candidate event shows up.
35. Identify at least 2 more concrete future uses for the dispatcher named in the original request
    (systems/subsystems that could use it later) — not speculative infrastructure with nothing real
    behind it, but named candidates with a one-line reason each (a save/serialization system
    reacting to chunk-modified events; a future audio/ambience system reacting to biome/material
    transitions).

### Group M — Cache locality & SIMD review

36. Now that Group E's Tracy zones exist, capture a real profile of `extract_mesh`
    post-`NeighborCache`-fix and confirm where time actually goes now — don't assume the fix fully
    closed the question just because the headline number improved. Check: a real Tracy capture, not
    an assumption from the aggregate before/after numbers alone.
37. Review `NeighborCache`'s own memory layout (3×3×3 chunk-pointer array) for anything working
    against cache locality at the scale it's actually used. Check: confirmed adequate with
    reasoning, or a specific concrete change proposed — not "probably fine."
38. Check auto-vectorization reports (`/Qvec-report:2` on MSVC, per `memory-and-performance.md` §4)
    on the Surface Nets inner loops specifically. Check: the actual compiler output is read and
    quoted, not assumed either way — and stated honestly if the branchy per-cell corner-testing
    structure turns out to not auto-vectorize well, since that's a plausible, honest outcome per
    §4's own framing, not a failure to fix.
39. Review `ChunkStreamer`'s per-tick desired-set diff (Group D) for allocation churn — does it
    reallocate a fresh set every tick, or reuse storage across ticks. Check: confirmed either way
    with a Tracy memory-zone capture, not assumed from reading the design alone.
40. Write up findings from tasks 36–39 in one place, explicitly separating "confirmed fine, don't
    touch" from "real opportunity, here's the concrete next task" — per the standing rule that a
    pass which changes nothing is still a real, documented result, not padding.

### Group N — Tracy + Benchmark methodology

41. Add Google Benchmark via CPM, confirmed against Subagent 2's findings on MSVC-specific behavior
    before assuming it's a clean drop-in. Check: a trivial benchmark target builds and runs under
    the real MSVC toolchain.
42. Build a benchmark harness for hash-map operations at realistic chunk-count scale (ties directly
    to Group H task 12 — one harness, not two). Check: results reproducible across repeated runs
    within a stated tolerance.
43. Build a benchmark harness for the compression encode/decode path (ties to Group K task 28).
    Check: same reproducibility standard as task 42.
44. Build a benchmark harness for mesh extraction itself, now that Group A/D's fixes are in, as a
    standing regression baseline — not to re-litigate the already-fixed bug, but so the next
    regression is caught by a number, not by a stalled frame someone happens to notice.
45. Write down, concretely, the actual answer to "how do we know if an optimization worked": a
    Google Benchmark number for the isolated function, cross-checked against a Tracy capture of the
    same code in real context, both before and after — one number alone (either kind) isn't
    sufficient per this project's own established standard (§0's cross-checking of every real bug
    fix so far). Check: this methodology is written down somewhere findable (`CLAUDE.md` or a new
    `research/benchmarking.md`), not just implied by having done it once.
46. Establish a lightweight regression-tracking convention — even just committing benchmark output
    alongside significant changes, if a full historical-tracking tool is more than this project's
    current stage warrants. Check: a concrete, stated convention exists, sized to the project's
    actual current scale rather than over-built.

### Group O — Systemic review & extensibility

47. Audit every other class in the codebase that owns a `std::jthread`/`ThreadPool`-family member
    for the §7 pattern — don't stop at the two known instances. Check: every thread-owning class is
    explicitly checked and listed, with its declaration order confirmed correct or fixed.
48. Consider whether §7's rule is worth encoding as more than documentation — a small base class or
    a static_assert-checkable pattern that makes getting the order wrong harder, not just documented
    as a convention to remember. Check: a concrete decision either way, with reasoning, not left
    open.
49. Review the modularity of what Groups H–L just touched — does the hash-map/queue swap stay behind
    the type-alias boundaries tasks 9/30 established, or has anything leaked a concrete type into a
    public interface. Check: a grep confirming no leakage, or leakage found and fixed.
50. Identify and write up, concretely (not speculatively), what other subsystems are genuinely
    likely needed soon given the project's actual trajectory (a settings/config system beyond
    `engine/core`'s current `Config`; a save/serialization system now that chunk data has real
    structure worth persisting) versus which are premature. Check: each candidate has a specific,
    named reason it's likely-soon vs. speculative, not a generic "might be useful."
51. Revisit `PROJECT_BRIEF.md`/`PHASE_1_BRIEF.md`/`PHASE_1_COMPLETION_BRIEF.md`'s status sections
    and fold in this document's findings once real work against it has happened — the same doc-sync
    discipline `PHASE_1_COMPLETION_BRIEF.md`'s own verification pass already modeled.

### Group P — Consolidation

52. Re-run `PHASE_1_COMPLETION_BRIEF.md` Group E–F (profiling wiring, CI matrix) against the changes
    this document makes — confirm Tracy zones still cover the swapped hash map/queue code, not just
    the code that existed when Group E was originally scoped.
53. Confirm the CI matrix includes whatever new dependencies this document adds (Google Benchmark,
    the chosen crash-handler library, the chosen hash-map/queue libraries) in its dependency-fetch
    step, not just the original five.
54. Full-suite regression run (currently 49/49 per the last report) — confirm the number only goes
    up from here, and any net-new test count is stated explicitly in whatever status update follows
    this document, matching this project's own established reporting standard.
55. A final written summary tying every group's outcome back to the specific concern that motivated
    it (the hash map to the reference-stability check, the queue to the ordering-semantics check,
    the crash handler to the wrap-vs-replace answer, compression to the measured VRAM delta) — so
    the next session inherits conclusions, not just a list of things that got touched.

## 10. External libraries — what's actually new

Contingent on Group G's verification, not asserted here as final picks: one hash-map library (likely
`ankerl::unordered_dense`, exact variant per task 6/7), one queue library (likely
`moodycamel::ConcurrentQueue`), one crash-handling library (likely `backward-cpp`), and Google
Benchmark. `entt::dispatcher` (§8) needs nothing new — already in the pinned EnTT. Every addition
gets pinned in `Dependencies.cmake` with the same inline-reasoning convention every existing entry
already follows, per `CLAUDE.md`'s own established standard.

## 11. Guardrails

Everything in every prior brief's guardrails section, unchanged, plus:

- Don't adopt any subagent's top pick without Group G's verification step actually happening —
  "gather details... main agent checks it rather than just use it" is the explicit standard this
  document was asked to meet, not a suggestion.
- Don't re-litigate the already-fixed `ChunkStore::find` bug — §0 is the final word on that
  incident; Group H's work is justified on its own, separate merits.
- Don't treat this document's groups as sequential — §1 says they're independent; working them in
  parallel across sessions, or in whatever order surfaces the most value first, is the intended use.
- Every task's "Check" is the actual acceptance criterion — a task marked done without its check
  having actually been performed is not done, regardless of how confident the implementation looks.

## Sources

Boost.Unordered's own benchmark repository and martinus/unordered_dense's own repository and wiki
(§3); NVIDIA Holoscan SDK's documented `LockFreeQueue` and a 2023 ISO C++ SG14 mailing-list thread
(§4); moodycamel/concurrentqueue's own repository; Sentry's own Crashpad/Breakpad "Backend
Tradeoffs" documentation and a real backtrace-labs/crashpad GitHub issue documenting Windows
integration friction, and bombela/backward-cpp's own repository and documentation (§5); real,
current production/academic references for GPU vertex compression — a merged Godot Engine PR, a
draft Blender PR citing Krzysztof Narkowicz's encoding technique and the Cigolle et al. JCGT paper,
Unity's own documented mesh-compression quantization bit-widths, and Bentley Systems' iTwin.js
documented `addQuantizedVertex` API (§6); EnTT's own current API documentation for
`entt::dispatcher` (§8).

In-repo: this document extends every prior brief's own findings rather than re-deriving them —
`PROJECT_BRIEF.md` §5–§6 (memory/concurrency, hardened not replaced), §14 (the GPU open question,
resolved per §0); `PHASE_1_BRIEF.md` §2.4 (multithreading caution, the same measure-before-building
discipline applied here to library swaps); `M1_2_BRIEF.md` §1 (the palette scheme, §6's compression
work builds on top of rather than replacing); `PHASE_1_COMPLETION_BRIEF.md`'s own progress log (§0's
entire framing); `modular-architecture.md` §1 (the `events/dispatcher.hpp` convention, realized in
§8); `concurrency-and-parallelism.md` §1, §3–4 (`std::jthread`/`stop_token`, the "reach for a
library" rule this document is a direct application of); and `library-research.md` in full (§2's
subagent design and the verification step extending its own conventions).
