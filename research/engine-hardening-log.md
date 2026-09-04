# Engine Hardening & Optimization — decision log

Execution log for [`ENGINE_HARDENING_BRIEF.md`](../ENGINE_HARDENING_BRIEF.md) (Groups G–P), run
2026-09-04. Every entry states the task's **check** and what actually happened — a claim without
its performed check is not marked done, per the brief's own guardrail. Three read-only research
subagents ran per §2 (verbatim prompts); their material findings were re-verified locally before
any adoption, per Group G.

## Group G — verification (tasks 1–5)

- **Tags verified live** via `git ls-remote --tags` / the GitHub release API on 2026-09-04, not
  quoted from any report: `martinus/unordered_dense` v4.9.2, `cameron314/concurrentqueue` v1.0.5,
  `bombela/backward-cpp` v1.6 (tag from 2021 — maintenance is master-branch only, last commit
  2025-04 per Subagent 2), `google/benchmark` v1.9.5, `boostorg/boost` boost-1.92.0 (the 1.91
  release was re-tagged `1.91.0-1`; the brief's guessed URL 404'd — caught by checking, which is
  the point of Group G).
- **Licenses read from the actually-fetched sources**, not READMEs: unordered_dense MIT,
  concurrentqueue dual Simplified-BSD/BSL (+ zlib for the embedded semaphore), backward-cpp MIT,
  benchmark Apache-2.0, Boost BSL-1.0. All permissive.
- **Standalone CPM smoke tests, isolated scratchpad project, real MSVC** (task 3's check): all
  four candidates fetch, build, and run clean — `unordered_dense` map+`segmented_map` with a
  custom ChunkCoord-shaped key; `BlockingConcurrentQueue` under 4 producer + 4 consumer
  `std::jthread`s with `stop_token` (1000/1000 jobs); backward-cpp printing a source-annotated
  trace (header-only + dbghelp); Benchmark timing a mix function with
  `DoNotOptimize`/`ClobberMemory` under MSVC. (The smoke project also reproduced the repo's
  git-shim `HEAD^0` bug — fixed the same way as the root CMakeLists.)
- **Decision-driving claims re-checked against primary sources** (task 4): the ONE claim that
  changed a decision — Subagent 1's "unordered_dense::map's iterator IS the `std::vector`
  iterator" (inferred from v1.2.0) — was confirmed directly in the fetched v4.9.2 header:
  `unordered_dense.h:1174` `using iterator = std::conditional_t<is_map_v<T>, typename
  value_container_type::iterator, const_iterator>;`. So `map` IS exposed to MSVC
  `_ITERATOR_DEBUG_LEVEL=2` checked-iterator locking in Debug; `segmented_map` (custom `iter_t`,
  line 674) is not.
- **Task 5 — written fallbacks** (each a named candidate from the same survey):
  - Hash map: primary `ankerl::unordered_dense` (variant per H7 below); fallback
    `boost::unordered_flat_map` (already fetched + benchmarked here), second fallback
    `boost::unordered_node_map` if reference stability ever becomes load-bearing.
  - Task queue: primary `moodycamel::BlockingConcurrentQueue` inside the existing ThreadPool
    shell; fallback = keep the hand-rolled mutex+condvar interior (status quo was correct, just
    lock-based), alternative wholesale replacement `BS::thread_pool` v5 if the pool shell itself
    ever becomes the problem.
  - Crash handling: primary = harden the in-house SEH handler (see J below); fallback
    `bombela/backward-cpp` pinned to master (2025-04 tip, NOT the 2021 v1.6 tag) if
    source-snippet printing or inline frames are ever wanted.

## Group H — hash map (tasks 6–12)

- **Task 6 — pointer-into-map audit: YES, long-lived `Chunk*` exist; NO, map-node stability is
  not relied on.** The full consumer read: `ChunkPipelineState::chunk`
  ([chunk_streaming.cpp:121](../app/src/chunk_streaming.cpp)) holds a `Chunk*` across arbitrary
  later inserts — but it points at the heap `Chunk` owned by `std::unique_ptr` (the map stores
  `unique_ptr<Chunk>`), and moving a `unique_ptr` during rehash never moves its pointee. Same for
  `NeighborCache`'s 27 pointers (read-only snapshot stores, no concurrent insert) and mesh-job
  snapshots. Nothing holds map iterators or references to the `unique_ptr` slots themselves
  (`get_or_create`/`find` return the pointee). Conclusion: the fast non-stable variant is safe.
- **Task 7 — FINAL decision: `boost::unordered_flat_map` — the local benchmark flipped the
  brief's lean, which is exactly what the process is for.** Task 6 had cleared the stability
  requirement, making `unordered_dense::map` the provisional pick on integration weight — but
  the task-12 harness (below) showed it is a LOOKUP REGRESSION versus even `std::unordered_map`
  at this workload's scale on this machine's MSVC, while `boost::unordered_flat_map` won every
  workload outright. Boost also structurally avoids the IDL=2 checked-iterator class (custom
  iterators — dense's are `std::vector`'s, confirmed at `unordered_dense.h:1174`), post-mixes
  the (avalanche-tested-healthy) `std::hash` automatically, and has `unordered_node_map` as a
  documented one-line stability sibling if a future consumer ever needs it. Subagent 1 ranked
  boost first for these exact reasons and asked for a local MSVC benchmark as the tie-breaker;
  it got one. Cost accepted: the Boost archive fetch (103MB, header-only usage, nothing else of
  Boost built).
- **Task 8 — hash avalanche test: run, with a surprise.** A standalone MSVC test fed the
  realistic clustered population (autofly's union of desired cubes, 3366 coords) through three
  hashes under BOTH bucket-extraction schemes (low-bit mask = MSVC `std::unordered_map`; top-bit
  shift = unordered_dense's `is_avalanching` path). The engine's existing `h*31`-combine over
  `std::hash<int32_t>` is operationally uniform under both (max bucket load 4–5, chi² within
  ~10–20% of ideal); a wyhash mix is equivalent, not better; a packed-identity hash is
  catastrophic (2 buckets used, max load 3036) — the risk class is real, the engine never had it.
  Consequence: for unordered_dense, provide the wyhash-based `is_avalanching` hash (validated
  here and in the smoke test) to skip its double-hash wrap; the current `std::hash`
  specialization stays correct for anything std-hash-based.
- **Task 9 — done**: `ChunkMap` alias in
  [chunk_store.hpp](../world/chunk/include/world/chunk/chunk_store.hpp); exactly one line (plus a
  hash argument) changes on any future container swap. Check: the concrete type appears nowhere
  else.
- **Tasks 10/11 — second-map audit found 9 more containers; ALL migrated.** `ChunkStreamer`'s
  three coord sets + `outside_since_` map, `ChunkStreamingSystem`'s `chunk_entities_` + four
  coord sets, and `TerrainRenderer::Impl::chunks` (iterated every frame for the draw loop — the
  single best fit for flat contiguous storage) now all go through `CoordMap<T>`/`CoordSet` in
  [coord_containers.hpp](../world/chunk/include/world/chunk/coord_containers.hpp). Side effect:
  `desired_`'s per-tick clear+rebuild no longer allocates per insert (flat `clear()` keeps
  capacity), retiring most of task 39's flagged churn.
- **Task 12 — harness built** (`benchmarks/bench_chunk_map.cpp`): insert/find/erase at the
  realistic 558-chunk scale over `std::unordered_map` / `unordered_dense::map` / `segmented_map`
  / `boost::unordered_flat_map`, on the real `ChunkCoord`. Numbers: appended below when the
  Release run lands.

## Group I — task queue (tasks 13–18)

- **Task 13 — future-retrieval preserved, zero call-site change.** The queue carries type-erased
  `std::function<void()>`; `submit()` keeps its `std::packaged_task` wrapper and exact signature
  `template <F, Args...> auto submit(F&&, Args&&...) -> std::future<invoke_result_t<F, Args...>>`.
  Verified by the unchanged call sites compiling untouched.
- **Task 14 — done**: [thread_pool.hpp](../engine/jobs/include/engine/jobs/thread_pool.hpp) /
  [.cpp](../engine/jobs/src/thread_pool.cpp). `std::jthread` + `stop_token` shell kept;
  `BlockingConcurrentQueue` interior; the §7 member-order rule re-verified and re-commented on
  the new `Impl` (queue first, workers LAST). Shutdown = one empty-`std::function` sentinel per
  worker (no timed-wait polling; workers exit on first sentinel WITHOUT draining — a draining
  worker could swallow siblings' sentinels and deadlock them — and the destructor thread finishes
  any leftovers after the join, preserving the drain-on-destruction contract). The queue type is
  module-private (public header has no third-party include).
- **Task 15 — no global-FIFO reliance, verified two ways.** (a) Code: gen→mesh sequencing is
  enforced by `ChunkStreamingSystem`'s explicit state machine (`neighborhood_generated` gate on
  the main thread), never by queue order; completions drain order-insensitively. (b) Structure:
  the only production submitter is the main thread — a single producer, for which per-producer
  FIFO IS the old global FIFO. Subagent 1's independent analysis reached the same conclusion,
  including the observation that completion order was never guaranteed even before.
- **Task 16** — `ChunkStreamingSystem` is covered automatically (its `pool_` is the redesigned
  ThreadPool). Check = the existing concurrency stress tests + smoke runs below; ASan pass listed
  under Group P.
- **Task 17** — throughput measured via the existing 16-thread stress suite + streaming smoke
  timings (below); stated honestly: the queue swap's contribution is expected to be far smaller
  than the NeighborCache fix's 80x, since the mutex queue was never the measured bottleneck —
  this is hardening (lock-free under future contention, bulk-ops headroom), not firefighting.
- **Task 18** — CLAUDE.md dependency notes updated (see doc-sync section).

## Group J — crash handler (tasks 19–23)

- **Task 19 — wrap-vs-replace, answered with a third option: augment in-house, no new
  dependency.** Subagent 2 verified from backward-cpp's source that it does NOT chain a previous
  SEH filter (zero uses of `LPTOP_LEVEL_EXCEPTION_FILTER`; last-writer-wins on
  `SetUnhandledExceptionFilter`), so "coexist independently" was never on the table — the real
  choice was replace, or drive backward's printer from our filter. But the survey also showed
  backward-cpp's Windows path is the SAME architecture as the existing handler
  (in-process `SetUnhandledExceptionFilter` + dbghelp symbolization) — its only deltas are
  prettier output and MORE HOOKED FAILURE CLASSES (`SIGABRT`, `std::terminate`, pure virtual
  call, CRT invalid parameter). The proportionate move for a solo-dev project: add those four
  hooks to the existing 80-line handler (~15 lines, same reporting path) instead of adopting a
  slow-cadence dependency to replace working code. backward-cpp (master pin) stays the named
  fallback if source-snippet rendering is ever wanted.
- **Task 20 — implemented + crash-tested**: `install_crash_handler` now also installs
  `std::set_terminate`, `signal(SIGABRT)` (+ `_set_abort_behavior`), `_set_purecall_handler`, and
  `_set_invalid_parameter_handler`, all funneling into the same symbolized-trace report; a
  debug-only `--crash-test <mode>` flag triggers each class deliberately. Results below.
- **Task 21 — decided and implemented, not defaulted**: Debug already gets `/Zi` from CMake's
  defaults; Release now explicitly keeps full PDBs (`/Zi` + `/DEBUG /OPT:REF /OPT:ICF` appended
  in the root CMakeLists) — a Release crash with no symbols is exactly the situation a crash
  handler exists to prevent; runtime cost zero (debug info lives beside, not inside, the image).
- **Tasks 22/23** — teardown-class scenario re-run + CLAUDE.md documentation: see below/doc-sync.

## Group K — GPU/voxel compression (tasks 24–31)

- **Task 24 — bit budget, full derivation.** Chunk-local span is [-1, 32] = 33 voxels/axis (the
  boundary layer is real geometry). 16-bit UNORM at a *dyadic* step of 1/1024 voxel:
  `q = round((p + 1) * 1024)`, max 33·1024 = 33792 < 65536 (headroom to a 64-voxel span). Step =
  0.98mm at 1m voxels — one to two orders of magnitude below Surface Nets' own edge-crossing
  placement error, and ~65x finer than Unity-style 10-bit-over-mesh-AABB would give here. The
  dyadic step (vs the "tight" 33/65535) is load-bearing: a 32-voxel chunk offset is then exactly
  32768 steps, so neighboring chunks quantize a shared seam vertex onto the same world lattice
  (residual disagreement bounded by 1 step when their input floats already differ — the same
  order of disagreement the full-float path already had). 12-bit/axis would also be invisible
  (8.1e-3 voxel) but maps to no fixed-function vertex format; 8-bit (0.13 voxel) risks visible
  faceting. Cross-check by ratio: normal 12B→2B (6x) + position 12B→6B (2x) + material flat →
  28B→12B = 2.33x total, consistent with §6's 3–3.5x estimate minus the padding/material tax.
- **Task 25 — done + round-trip tested**: 16-bit octahedral encoder
  ([octahedral.hpp](../world/meshing/include/world/meshing/octahedral.hpp)), 4 tests: max
  angular error < 1.5° / mean < 0.6° over 4096 sphere-covering directions (thresholds stated
  from the literature's oct16 error tables), near-exact axis normals, re-encode drift bounded by
  one lattice step (byte-exact idempotency is NOT a property of the plain encoding near fold/
  quantization boundaries — a first test version demanding it failed, correctly, and was replaced
  by the property the pipeline actually needs), scale invariance for unnormalized input.
- **Task 26 — done, the cross-backend risk designed OUT rather than managed.** Subagent 3's
  pivotal findings: glslang's HLSL front-end (Diligent's default Vulkan path) was deprecated by
  Khronos April 2026; `f16tof32` is absent from the HLSL2GLSLConverter's documented intrinsic
  list. So the compressed format uses NO shader bit manipulation at all: fixed-function
  normalized vertex fetch (UNORM16x3 position / UNORM8x2 octahedral / plain uint8 material,
  12-byte stride — structurally Godot 4.2's shipped compressed format) + one `mad` and an
  arithmetic octahedral refold in the VS. `65535/1024` is exactly representable in float32.
  Check: `--verify-frame` on Vulkan AND D3D12 below.
- **Task 27** — upload path converts at the render boundary (CPU `MeshData` stays full-float for
  meshing/tests); indices also dropped 32→16-bit (max 33³ = 35937 vertices/chunk < 65536, checked
  at upload rather than assumed). Tracker sees the new sizes; measured VRAM delta below.
- **Task 29 — RLE-on-palette: NO for in-memory storage, with evidence.** Subagent 3's survey of
  shipped practice: Minecraft (palette origin) keeps palette+bitpack in memory with zlib only at
  region/protocol boundaries; godot_voxel ships uniform-channel fast path + LZ4 container, no
  RLE; the one RLE precedent (Seed of Andromeda) applies it to raw un-paletted disk
  serialization; voxel.wiki's palette+RLE combination section is literally an unwritten TODO;
  0fps's principled RLE case targets iteration-dominated read-mostly workloads and trades away
  the O(1) random access this project's editing direction needs. Decision: keep palette+bitpack
  in memory; when persistence arrives, LZ4-or-Zstd over the serialized palette stream
  (godot_voxel's exact recipe), and revisit RLE only if that measurably falls short. Task 30:
  n/a by this decision.
- **Task 31 — upload-pattern review, written comparison.** Current: per-chunk
  `USAGE_IMMUTABLE` create/release (with `BufferData` at creation — no runtime transitions, no
  Map churn). Found idiomatic pattern for exactly this workload: pooled suballocation —
  DiligentCore ships `Graphics/GraphicsTools/interface/BufferSuballocator.h` + `DynamicBuffer`;
  NVIDIA's Vulkan guidance says the same (suballocate, `vkAllocateMemory` is expensive).
  Assessment: at this project's measured churn (~10–20 chunk uploads/s while streaming, 532
  resident) create/destroy cost has not shown up in any profile; adopting the suballocator now
  would be speculative. Concrete follow-up task recorded: when a Tracy capture shows
  `chunk upload` zones or memory-allocation time material to frame pacing, move chunk VB/IB into
  one `BufferSuballocator`-backed pool and bind by offset. (The 2.33x/2x shrink from tasks 26/27
  also directly cuts the upload bandwidth this pattern would optimize.)

## Group L — events (tasks 32–35): DONE

- `engine/events` module (thin `entt::dispatcher` alias — deliberately not a wrapper), 3 unit
  tests (trigger, enqueue/update deferral, disconnect). Chunk lifecycle events
  (`ChunkLoaded`/`ChunkMeshReady`/`ChunkUnloaded` in
  [chunk_events.hpp](../world/streaming/include/world/streaming/chunk_events.hpp)) fire from
  `ChunkStreamingSystem`'s main-thread drains; the overlay's ready-chunk count is now DEFINED by
  event pairs, not polling (task 33), with a poll-vs-event consistency check in the 2s stats
  report that logs an error on any divergence — zero divergences across a full verify-frame run
  and a 900-frame autofly load/unload stress.
- **Task 34** — the event-vs-polling convention is documented where it's findable, in
  [dispatcher.hpp](../engine/events/include/engine/events/dispatcher.hpp): events for
  transitions other systems react to; polling for per-frame current values; main-thread-only
  threading rule (workers reach the dispatcher only through the existing completion-queue
  drains).
- **Task 35 — named future uses**: (1) save/serialization system reacting to
  `ChunkLoaded`/future `ChunkModified` (the persistence recipe from task 29 gives it a concrete
  job); (2) audio/ambience reacting to camera-chunk material/biome transitions (event source
  already exists in the streamer's anchor-change); both are reactions to transitions — exactly
  the documented criterion.

## Group M — cache locality & SIMD review (tasks 36–40)

- **Task 38 — auto-vectorization report, actual compiler output.** `mesh_extractor.cpp` compiled
  Release with `/O2 /Qvec-report:2` (MSVC 14.51): NOTHING in the extraction vectorizes. Quoted
  reasons: the cell loops (lines 39–41, 267–269, 282–284) report `1106` (inner loop of already-
  considered nest) and `1303`/`1305` (too few iterations / insufficient computation); the
  per-cell corner tests (90, 99) report `1200` (loop contains data dependencies) and `503`
  (function calls); vector-growth paths report `501/506/1301`. This is the brief's own
  pre-approved honest outcome: Surface Nets' branchy per-cell structure does not auto-vectorize;
  a SIMD win would require structural batching (e.g. corner-mask precomputation per slab), which
  stays measured-gated — no change made.
- **Task 37 — NeighborCache layout: confirmed adequate, reasoning.** 27 `const Chunk*` = 216B =
  4 cache lines, resolved once per extraction and read ~thousands of times; slot arithmetic is
  branch-light shifts/compares; the actual per-sample cost is the palette bit-read in the target
  chunk, which no cache layout change here touches. No 3x3x3-specific improvement proposed.
- **Task 39 — desired-set churn: analyzed from source; Tracy-capture measurement deferred with
  the reason stated.** `tick()` does `desired_.clear()` + ~196 `insert`s (node reallocation per
  insert on MSVC) + two fresh `TickCommands` vectors per tick — roughly 12k small allocations/s
  at 60Hz. Order-of-magnitude: ~20µs/tick, ~0.1% of a 16ms frame — real but currently noise. A
  live Tracy memory-zone capture needs the Tracy server UI attached (client-only is integrated);
  recorded as the same follow-up capture as task 36. If/when H's migration lands, `desired_` as
  a flat set (or a reused sorted vector) removes the churn as a side effect.
- **Task 36 — deferred to a Tracy-server session, honestly**: the zones exist and are exercised
  (client integrated, `TRACY_ON_DEMAND`), but a *capture* requires attaching the Tracy server
  UI interactively — queued as the first item for the next profiling session rather than
  fabricating a capture. The stress-suite timing (16-thread meshing suite, seconds-scale) is the
  standing proxy number in the meantime.
- **Task 40** — this section is that write-up: "confirmed fine, don't touch" = NeighborCache
  layout, auto-vectorization status, current upload pattern at current scale; "real opportunity,
  next task named" = Tracy capture session (36/39), suballocator adoption trigger (31),
  streamer-set flat-map migration (11).

## Group N — benchmarks & methodology (tasks 41–46)

- **Task 41 — done**: Benchmark v1.9.5 via CPM behind `VOXEL_BUILD_BENCHMARKS` (OFF by default —
  Debug-build numbers are meaningless; built via a Release core-only configure). MSVC caveats
  from Subagent 2 applied as code/discipline: non-const lvalues to `DoNotOptimize` (the const-ref
  overload's deprecation fired as a real /WX break here and was fixed), no LTCG on benchmark
  targets (upstream's own LTO FIXME), clang-cl as the cross-check if a result looks
  compiler-suspicious.
- **Tasks 42/43/44 — three harnesses in `benchmarks/`**: chunk-map ops at 558-chunk scale (4 map
  configs), octahedral encode/decode throughput, and `extract_mesh` on a real generated 27-chunk
  neighborhood as the standing regression baseline. Numbers appended below.
- **Task 45 — the methodology, stated**: an optimization "worked" when BOTH (a) the isolated
  Google Benchmark number moved (compared via `tools/compare.py`'s Mann-Whitney U-test between
  saved `--benchmark_out` JSON baselines, not eyeballed single runs) AND (b) a Tracy capture of
  the same code in real frame context confirms the win survives — one number alone is
  insufficient; this mirrors how every real fix in this project has been cross-checked.
- **Task 46 — regression convention, sized to the project**: save `--benchmark_out=<name>.json`
  beside significant changes (repo `benchmarks/baselines/`, committed by the user with their
  snapshots), judge with `compare.py`; no CI benchmark gating (hosted-runner noise makes ns-scale
  thresholds meaningless — Subagent 2's survey found even large engines mostly do point-in-time
  comparisons).

## Group O — systemic review (tasks 47–50)

- **Task 47 — full thread-owner audit, every site listed**: (1) `ThreadPool` — workers last ✓
  (comment, re-verified on the redesigned Impl); (2) `ChunkStreamingSystem` — `pool_` last ✓
  (comment); (3–5) three test-local `std::vector<std::jthread>` blocks
  (`test_concurrent_meshing`, `test_concurrent_stress`, `test_memory_tracking`) — all declared in
  inner scopes after the state they capture, joining at block exit before that state dies ✓. No
  other thread-owning classes exist (`grep std::jthread|ThreadPool`).
- **Task 48 — decision: convention + audit, NO structural enforcement, with reasoning.** A
  "ThreadOwner" base class enforces the WRONG order (bases destruct after members — threads would
  die last); the inverted pattern (state in a base, threads in the derived) contorts otherwise
  simple classes to protect exactly two production instances that already carry loud
  declaration-site comments. C++ offers no portable static_assert over member declaration order.
  The rule lives in: the two in-code comments, this log, and CLAUDE.md; any new thread-owning
  class gets checked by the task-47 grep, which is now trivial to repeat.
- **Task 50 — likely-soon vs premature subsystems**: likely-soon: (1) chunk save/serialization —
  data structure worth persisting now exists, the format recipe is researched (palette stream +
  LZ4), and the event hooks are live; (2) a settings/config layer — `AppOptions` already carries
  7 user-tunable flags parsed ad hoc. Premature: audio/ambience (no audio dependency or design),
  networking (nothing points at it), scripting (no content pipeline to script).
- **Task 49** — grep after H/I landed: the concrete map type appears only inside `ChunkMap`'s
  alias; the queue type only inside `thread_pool.cpp`. (Re-run recorded under Group P.)

## Appendix — measured numbers (2026-09-04, RTX 4070 Laptop, MSVC 14.51)

**Task 12 — chunk-map harness** (Release, 558 real `ChunkCoord` keys, 5 repetitions, means;
baseline JSON: `benchmarks/baselines/2026-09-04-hardening.json`):

| workload (558 ops) | std::unordered_map | unordered_dense | segmented | boost_flat |
|---|---|---|---|---|
| build+teardown | 38.0µs | 30.3µs | 63.1µs | **17.3µs** |
| find (all hits) | 3.68µs | 6.20µs | 6.83µs | **3.04µs** |
| find (all misses) | **2.54µs** | 7.64µs | 9.41µs | 3.25µs |

boost_flat wins two of three outright and is within noise of std on misses; unordered_dense
LOSES to std on both lookup workloads at this scale — the published rankings (string-heavy,
larger-N, GCC-run) did not transfer to this exact workload/compiler, which is why the harness
exists. Honest scale note: these are microseconds per whole-tick's-worth of operations —
hardening data, not a hot-path fix.

**Task 42/43/44 baselines** (same run): octahedral encode 4096 normals ≈ (see JSON) / decode
25.2µs per 4096 ≈ 6.1ns/normal (176M/s); `extract_mesh` on a real generated surface chunk:
**3.97ms, 869 vertices** — the standing regression number.

**Task 27 — VRAM, before/after the compressed vertex format** (autofly, 900 frames, radius 5,
532 chunks at exit both runs): 17.0 MiB → **7.1 MiB** live, 18.1 → **8.3 MiB** peak = **2.39x**,
matching the predicted blend of 2.33x VB + 2.0x IB. `--verify-frame`: Vulkan 13.4% (identical to
the uncompressed baseline), D3D12 13.1% — both far above the 5% threshold; the 0.3pp
cross-backend difference is noted rather than hidden (sub-pixel rasterization divergence on the
quantized data is the plausible cause; Group B's bit-identical claim was for the uncompressed
fixed scene).

**Two real bugs the checks caught during this pass** (the checks are the story):
1. 3-component `VT_UINT16` position asserted in Diligent's D3D12 mapping ("Unsupported number of
   components") while rendering fine on Vulkan — DXGI simply has no `R16G16B16_UNORM`. Caught by
   the per-backend `--verify-frame` requirement; fixed with a 4-component position at the same
   12B stride.
2. The crash handler's refactor passed `CONTEXT` by value (loses its declspec(align(16)) on the
   parameter → StackWalk64 unwound stack garbage), and `SymInitialize` failed with
   ERROR_INVALID_PARAMETER because Tracy's client had already initialized dbghelp — both caught
   by task 20's deliberate `--crash-test`, both fixed (aligned local copy; `SymRefreshModuleList`
   fallback). Final traces name the exact crash site (`parse_args + 0xb76` for the injected AV).

## Sources

Subagent reports (2026-09-04, in-session): Subagent 1 (hash maps/queues — Boost.Unordered docs +
benchmark suite, unordered_dense repo/issue #141, MSVC STL `xlock.cpp` `_Lockit` source, moodycamel
repo/issue #72, Tracy's vendored `tracy_concurrentqueue.h`, BS::thread_pool v5, oneTBB/Taskflow);
Subagent 2 (crash/benchmark — backward-cpp source-level SEH audit via grep.app, Sentry
Crashpad/Breakpad docs, google/benchmark `utils.h` verbatim `DoNotOptimize` paths + issue #747,
nCine/O3DE/Bevy methodology precedents); Subagent 3 (compression — Cigolle et al. JCGT 2014 PDF,
Narkowicz 2014, Godot PRs #46800/#60309/#81138 with measured FPS deltas, Blender/Cycles PR
#153836, Cesium quantized-mesh, KHR_mesh_quantization, WebGPU vertex-format guarantees, glslang
README deprecation notice, Diligent HLSL2GLSLConverter supported-features page, Diligent resource
-update docs + Tutorial10/11 + `BufferSuballocator.h`, Minecraft/godot_voxel/SoA/0fps voxel
storage specs). Local verifications: `git ls-remote`/GitHub release API listings, fetched LICENSE
files, `unordered_dense.h:1174`, MSVC `/Qvec-report:2` output, and every smoke/benchmark run in
this log. **In-repo**: [ENGINE_HARDENING_BRIEF.md](../ENGINE_HARDENING_BRIEF.md) §0–§11;
[PHASE_1_COMPLETION_BRIEF.md](../PHASE_1_COMPLETION_BRIEF.md) (Group D bug history this pass
builds on); [research/diligent-core-api-surface.md](diligent-core-api-surface.md) (the API
groundwork the render changes sit on).
