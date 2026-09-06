# C++ Voxel Engine (branch: `C++-voxel`)

Phase-based build per [`docs/progress.md`](docs/progress.md) (current state) and
[`docs/goals.md`](docs/goals.md) (living backlog) — John Lin-inspired voxel terrain
engine on DiligentEngine + EnTT + GLM + GLFW + FastNoise2, CMake + CPM.cmake. Read those two for
vision/architecture/phase roadmap — this file is operational build notes only, specific to this
machine, learned the hard way during Phase 0. Read it before running `cmake` on this project.

## Building — must read before running cmake

**Use the `CMakePresets.json` at the repo root** — this is now the primary, recommended way to
build, and it's what makes Visual Studio's/VS Code's own "Open Folder" CMake integration work
without any manual setup: opening this repo's folder in either one auto-detects the
`windows-debug`/`windows-release`/`windows-relwithdebinfo` presets and Just Works. From a command
line (after sourcing a real MSVC environment — see below), the equivalent is:

```
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug
```

**Prefer `windows-relwithdebinfo` over `windows-debug` for interactive runs from Visual Studio**
(chunk-generation profiling pass, 2026-09-05): `windows-debug` links the MSVC debug CRT, which
defaults `_ITERATOR_DEBUG_LEVEL=2` — measured on this exact codebase at ~80x slower for
concurrent hash-map-heavy code (`mesh_extractor.cpp`'s `NeighborCache` comment), and confirmed
again end-to-end (`research/chunk-generation-optimization-log.md`): the SAME small world loaded
in 32.1s under `windows-debug` vs. 9.4s under `windows-relwithdebinfo` — a 3.4x difference from
the build config alone, nothing to do with world size. `windows-release` remains the
near-zero-compromise choice when raw speed matters most; `windows-relwithdebinfo` exists
specifically for "I want to run/debug this interactively from the IDE without paying Debug's
tax." Switch to it via Visual Studio's own configuration dropdown, or:

```
cmake --preset windows-relwithdebinfo
cmake --build --preset windows-relwithdebinfo
```

The preset builds to `C:/b/<preset-name>` (e.g. `C:/b/windows-debug`), **not** `out/build/...`
under the repo — see the MAX_PATH note below for why this is load-bearing, not a style choice.
`GIT_EXECUTABLE` is now handled automatically by the root `CMakeLists.txt` itself (see the comment
there) rather than needing a manual `-D` flag — this was the actual fix that makes plain
`cmake --preset ...`, this repo's own scripts, and Visual Studio's own CMake integration all work
identically regardless of entry point; see the git-shim note below for why it was ever needed.

**Build to a short directory outside the repo — this is why the preset doesn't use `out/build/...`.**
This repo's own path is long enough that DiligentCore's `ThirdParty/glslang` build (whose object
paths mirror the full absolute CPM cache source path) exceeds Windows' 260-char MAX_PATH once
nested under almost any in-repo build directory (`<repo>\build\_deps\...` *and* Visual Studio's own
default `<repo>\out\build\x64-Debug\_deps\...` both blow it), and `cl.exe` fails with
`fatal error C1083: Cannot open compiler generated file: '': Invalid argument`. If you ever build
without the preset, use a short absolute path like `C:\b` explicitly (`cmake -B C:\b`) — don't let
it default to an in-repo directory.

**Must run from a real MSVC environment**, not a plain shell — `cl`/`link` aren't on PATH by
default. Source a VS toolchain first (either edition works — both are on this machine and both are
verified: see "Multiple VS installs" below):

```
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
```

(Visual Studio's/VS Code's own CMake integration sources this for you automatically when you open
the folder — you only need this manual step for a command-line build.)

### Multiple VS installs on this machine — both verified working

There are **two separate VS "18"/2026 installs** on this machine, at different MSVC point versions
— both confirmed to build this whole project (all 5 dependencies, D3D11/D3D12/OpenGL/Vulkan
backends, all our own tests) cleanly via `CMakePresets.json`:

- **Build Tools**: `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\` — MSVC
  `14.50.35717`. Note the `(x86)` in the path despite being a 64-bit-capable toolchain; this is
  where the ATL component had to be added manually during Phase 0 (see below).
- **Community**: `C:\Program Files\Microsoft Visual Studio\18\Community\` — MSVC `14.51.36231`
  (no `(x86)`; this is the edition Visual Studio's own IDE "Open Folder" flow uses). Already had
  ATL preinstalled.

`vswhere.exe` does **not** detect either one (its registry/COM-based discovery is broken on this
machine; confirmed working via direct filesystem search + `vcvars64.bat` instead). Don't trust
`vswhere -all` returning empty as proof MSVC is absent — check for `cl.exe` directly:
`C:\Program Files*\Microsoft Visual Studio\**\Hostx64\x64\cl.exe` (note the two different
`Program Files` roots above).

**Do NOT mix the two installs on one build directory** (hit for real 2026-09-04): a build dir
whose cache pins Community's 14.51 compiler (what VS's own "Open Folder" configure picks) will
**fail to link** from a BuildTools-14.50 `vcvars64` shell with `LNK2019: unresolved external
symbol __std_max_element_4i` (and friends) — 14.51's STL headers emit calls to vectorized
algorithm helpers that only exist in 14.51's static libs, and the linker takes its LIB paths from
the shell, not the cache. Source the vcvars64.bat of the SAME edition the build dir was
configured with (check `CMAKE_CXX_COMPILER` in `CMakeCache.txt` if unsure). Also beware: piping a
build through `findstr`/`Select-String` makes `$?`/exit code report the FILTER's status, silently
masking exactly this kind of link failure — check the build's own exit code, never a pipe's.

**MinGW/clang (llvm-mingw) does NOT work for this project** — tried first, hit multiple real
incompatibilities: FastSIMD's `-Wa,-muse-unaligned-vector-move` flag (fixed, see the guard in
`cmake/Dependencies.cmake`), then unfixable ones — `lld` rejects DiligentCore's
`--version-script=export.map` GNU-ld-only linker flags, and `HLSL2GLSLConverter` needs the
MSVC-only `_CrtSetDbgFlag` CRT symbol. Also, DiligentCore's own CMake disables
`D3D11_SUPPORTED`/`D3D12_SUPPORTED` outright under MinGW (`if(MINGW) ...`), so even a fully
patched MinGW build would only ever get OpenGL/Vulkan. Use real MSVC.

**MSVC needs the ATL component specifically**, not just the base C++ desktop workload —
DiligentCore's `Platforms/Win32/src/Win32FileSystem.cpp` includes `atlbase.h` unconditionally
(not gated behind D3D support, so even OpenGL/Vulkan-only builds need it). If `atlbase.h` is
missing:

```
vs_buildtools.exe modify --installPath "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools" ^
  --add Microsoft.VisualStudio.Component.VC.ATL --quiet --wait --norestart
```

**`git` on PATH may resolve to a `.cmd` shim**, not real `git.exe` (seen at
`C:\Users\Tonyt\AppData\Roaming\npm\git.cmd`, likely from a token-optimizer/RTK hook). That shim
goes through cmd.exe's batch interpreter, which mangles a literal `^` in arguments — this broke
FetchContent's internal `git rev-parse HEAD^0` into `HEAD0` (`fatal: ambiguous argument 'HEAD0'`)
for *any* CPM package, not just one (first found via DiligentEngine, but it later hit `entt` too
under Visual Studio's own CMake integration, which doesn't go through this repo's own scripts).
**Fixed at the project level now** — the root `CMakeLists.txt` explicitly searches
`C:/Program Files/Git/{cmd,bin}` for the real `git.exe` and sets it as the `GIT_EXECUTABLE` cache
variable before any dependency fetch runs, so this works from any entry point (plain `cmake`, this
repo's scripts, VS's/VS Code's own integration) without a manual `-D` flag. If you ever see the
`HEAD0` error again despite this, an existing stale build directory's cache likely already has the
wrong `GIT_EXECUTABLE` baked in from before this fix — delete that build directory and reconfigure
fresh rather than trying to patch the existing cache.

## Crash handler & symbols (Group J hardening, 2026-09-04)

`app/src/crash_handler.cpp` hooks five failure classes, all printing a dbghelp-symbolized stack
to stderr: SEH unhandled exceptions (`SetUnhandledExceptionFilter`, walks the *faulting* context),
plus `std::terminate`, `SIGABRT`, pure-virtual calls, and CRT invalid-parameter (these walk their
own current context — crash site is a few frames down). Debug builds accept
`--crash-test av|abort|terminate` to exercise the hooks (flag compiled out in Release). Symbol
policy is deliberate, not defaulted: Debug gets `/Zi` from CMake's defaults; **Release keeps full
PDBs** (`/Zi` + `/DEBUG /OPT:REF /OPT:ICF` appended in the root `CMakeLists.txt`) so a Release
crash still symbolizes — zero runtime cost, the info lives in the `.pdb` beside the exe. Note the
handler needs that `.pdb` present next to the binary. `backward-cpp` (master pin, not the stale
v1.6 tag) is the researched fallback if source-snippet traces are ever wanted — it does NOT chain
an existing SEH filter (verified from its source), so it would replace, not coexist.

## Benchmarks (Group N)

`-DVOXEL_BUILD_BENCHMARKS=ON` (OFF by default) adds `benchmarks/voxel_benchmarks` — Google
Benchmark harnesses for chunk-map ops, octahedral encode/decode, and `extract_mesh` (the standing
regression baseline). Build it **Release, core-only** to avoid the full GPU dependency fetch:
`cmake -B C:\b\bench-rel -G Ninja -DCMAKE_BUILD_TYPE=Release -DVOXEL_BUILD_RENDERER=OFF
-DVOXEL_BUILD_BENCHMARKS=ON -DBUILD_TESTING=OFF`. Convention: save
`--benchmark_out=benchmarks/baselines/<date>-<change>.json` beside significant changes; judge
before/after with Benchmark's `tools/compare.py` (Mann-Whitney U-test), not single-run eyeballs.
MSVC discipline (researched + confirmed locally): pass non-const lvalues to `DoNotOptimize` (the
const-ref overload deprecation is a real /WX break), no LTCG on benchmark targets. The full
methodology note lives in `research/engine-hardening-log.md`.

## Dependency notes (see `cmake/Dependencies.cmake` for the full pins + inline reasoning)

- DiligentEngine is pinned to commit `aca2285` (master, 2026-08-16), **not** the coordinated tag
  `API256015` (2026-03-26) the brief originally reasoned toward — that tag's vendored SPIRV-Tools
  snapshot fails under this machine's MSVC (19.50) with C++20 warning C5232 elevated to an error
  by DiligentCore's own `/WX`. `aca2285` is past whatever upstream fix changed this and builds
  clean. If DiligentEngine is ever re-pinned, re-verify this still builds under real MSVC first.
- EnTT/GLM/GLFW/FastNoise2 are declared **before** DiligentEngine in `Dependencies.cmake` on
  purpose — this DiligentEngine commit turns out to also pull in EnTT internally, and CMake's
  FetchContent gives the *first* `Declare` call for a given name priority build-wide, so ours must
  come first or `_deps/entt-build` collides between two different EnTT sources ("The binary
  directory ... is already used to build a source directory").
- `UPDATE_DISCONNECTED TRUE` is set on the DiligentEngine package — without it, FetchContent's
  separate git "update" step (re-checking a pinned tag for upstream changes on every reconfigure)
  fails the same `HEAD^0` way described above, even after fixing `GIT_EXECUTABLE`. A pinned tag
  has nothing to check for anyway.

## Sanitizers (M1.1's ASan/TSan verification)

**ASan works, via MSVC's `/fsanitize=address`, but needs one extra runtime DLL not on PATH
outside a vcvars-sourced shell**: `clang_rt.asan_dynamic-x86_64.dll`, under
`VC\Tools\MSVC\<ver>\bin\Hostx64\x64\` in the Build Tools install. Either run the ASan binary from
within a `vcvars64.bat`-sourced session, or copy that DLL next to the binary — otherwise it fails
to launch with `clang_rt.asan_dynamic-x86_64.dll: cannot open shared object file`.

**TSan is not available on this machine, confirmed via all three realistic paths, not assumed:**
- MSVC (`cl.exe`) has no ThreadSanitizer support at all — it's a Clang/GCC-only sanitizer.
- llvm-mingw's `clang++` explicitly refuses it: `unsupported option '-fsanitize=thread' for target
  'x86_64-w64-windows-gnu'`.
- WSL2 Ubuntu is installed but has no C++ compiler, and `apt-get install g++` blocks on an
  interactive sudo password (`sudo: timed out`) that can't be supplied non-interactively.

If TSan coverage matters later: either install a compiler in WSL yourself (`sudo apt install g++`
or `clang`, entering the password interactively) and compile the platform-independent code there —
`engine/jobs`'s `ThreadPool` has zero Windows-specific dependencies, so this works cleanly once a
compiler exists — or get `clang-cl` added to the Build Tools install
(`Microsoft.VisualStudio.Component.VC.Llvm.Clang`), though Windows TSan support under clang-cl is
historically less mature than Linux's.

## FastNoise2 SIMD levels actually compiled in this build

**`FastSIMD::FeatureSet::SCALAR` is NOT compiled into this project's FastNoise2** — only
`SSE2`/`SSE41`/`AVX2`/`AVX512` are (confirmed in Phase 0's own build log: `FastSIMD: Created
dispatch library "FastSIMD_FastNoise" with Feature Sets (RELAXED): SSE2;SSE41;AVX2;AVX512` — no
`SCALAR` in that list). `FastNoise::New<T>(FastSIMD::FeatureSet::SCALAR)` silently returns
**null** for an uncompiled feature set rather than erroring — dereferencing it segfaults on the
very first call. If pinning to one explicit SIMD level for determinism (`world/generation`
already does this — see `M1.2 brief` §2.5), pin to `SSE2` instead: it's the actual lowest level
compiled in, and still a safe universal floor for this x86-64-only project (SSE2 is mandatory
baseline on every x86-64 CPU, unlike SSE41/AVX2/AVX512). `world/generation/src/heightmap_generator.cpp`
wraps `FastNoise::New<T>` in a helper that throws instead of returning null/segfaulting if this
ever regresses — check there before assuming a given `FeatureSet` value is safe to request.

## Dependency additions from the hardening pass (2026-09-04)

Beyond the original five: `concurrentqueue` v1.0.5 (ThreadPool's interior queue; DOWNLOAD_ONLY,
consumed as a PRIVATE include of `engine_jobs` — public header stays third-party-free), `Boost`
1.92.0 via the official CMake release archive with `BOOST_INCLUDE_LIBRARIES unordered` only
(backs `world/chunk/coord_containers.hpp`'s `CoordMap`/`CoordSet`; header-only usage), and —
gated behind `VOXEL_BUILD_BENCHMARKS` — Google Benchmark v1.9.5 + `unordered_dense` v4.9.2 (the
latter exists ONLY for the comparison harness; it lost the local benchmark and is not used by
the engine). All tags verified live via `git ls-remote`/release API on 2026-09-04; all
smoke-tested standalone under real MSVC before wiring in. CI needs no workflow change: the CPM
caches key on `hashFiles('cmake/Dependencies.cmake')`, so new pins invalidate and fetch
automatically.

## Terrain fixes & gameplay pass (2026-09-04, after the hardening pass)

Decision log: `research/terrain-fixes-log.md`. Fixed: the **ribbon bug — TWO stacked causes**:
(1) streaming applied the Chebyshev radius to Y (desired set is now full [-3,2]-band COLUMNS in
a horizontal-only radius; the pre-fix "196 chunks" was exactly 7×7×4 cropped layers), and (2)
**terrain winding inverted since Phase 1** — `FrontCounterClockwise` was left False against the
mesher's CCW output, so up-facing triangles were back-face culled everywhere and every prior
"verified" frame was silhouette slivers scraping past a 5% threshold. Fix in `pso_terrain.cpp`
(=True): verify fraction 14.3%→39.4%, continuous landmass + the ocean's first-ever render;
threshold raised to 25%. Never trust a winding derivation without reviewing a real capture from
both sides — `VOXEL_DUMP_FRAME=<path>` + `--verify-frame` writes a PPM for exactly that (more
env debug tools: `VOXEL_DUMP_DRAWS`, `VOXEL_NO_CULL`, `VOXEL_ONLY_CHUNK_Y=<n>`). Also fixed: the
**overlay fps/ms mismatch** (both now derive from one smoothed frame time — consistent by
construction), and load-stutter mitigations grounded in two research reports
(per-frame mesh upload budget `--upload-budget N` default 4, 0=unlimited for A/B; budgeted GPU
buffer RELEASE on unload — the godot_voxel-documented destroy-churn spike; nearest-first job
submission). New gameplay: **walk mode** (`G` toggles fly/walk, `--walk` starts in it; gravity +
analytic ground clamp from `HeightmapGenerator::height_at`, clamped to sea level — you stride on
water, no swimming yet) and **procedural trees** (deterministic jittered-grid placement masked by
height/slope, box trunk + octahedron canopy appended into the chunk's own compressed mesh; Wood/
Leaves materials 4/5; overlay "objects" count). `--verify-frame` writes a reviewable PPM when
env `VOXEL_DUMP_FRAME=<path>` is set. `--autofly --walk` asserts no ground fall-through per
frame. The 2s stats line now prints worst-frame ms and the loaded chunk-Y range.

## Micro-voxel pivot (2026-09-05) — operational deltas

Full record: `research/micro-voxel-pivot-log.md`; backlog Groups W–Y in `docs/goals.md`.
Machine-relevant deltas ONLY:

- **`voxel_app` defaults to `--renderer svo`** (the sparse-brick octree at 7.8 mm voxels near the
  camera, GPU ray-marched). The old chunk/mesh world is `--renderer mesh`. World-ready time on the
  svo path is ~0.6 s — the whole "loading takes forever" complaint no longer applies there.
- svo flags: `--voxel-log2 N` (finest voxel = 2^N m, default -7), `--region-log2 N` (root edge =
  2^N m, default 9), `--lod-radius M` (full resolution within M meters, default 4), `--no-trees`,
  `--no-shadows`, `--no-ao`, `--no-lod-march`, `--lod-quality Q`. `--pos/--yaw/--pitch`,
  `--verify-frame`, `VOXEL_DUMP_FRAME`, `--dump-every`, `--walk`, `--autofly` all work unchanged.
- `tools/svo_render` renders the same world on the CPU to a PNG (`--xz x,z` auto eye height,
  `--pos/--yaw/--pitch` like the app, `--root-log2`, `--voxel-log2`, `--size WxH`, `--verify`).
  It prints a per-level sampled/kept brick histogram and the camera column probe — the first tool
  to reach for when a GPU frame looks wrong (it found the MUTABLE-SRB bug in minutes).
- **HLSL for D3D12 (FXC) forbids writing a runtime-indexed vector component** (X3500) — Vulkan's
  compiler accepts it, so a shader can pass on `--mode vk` and fail on `--mode d3d12`. Use masked
  vector writes (`svo_march.psh.hlsl`'s `AxisMask`/`Comp` helpers). Test both backends.
- **Diligent MUTABLE SRB variables bind exactly once per SRB**; a second `Set` is silently ignored
  in Release. Resources replaced at runtime (the tree buffers) must be DYNAMIC variables.
- The 165 Hz panel caps fps readings at ~155–159 (FIFO_RELAXED); a "GPU headroom" question needs
  a Tracy GPU zone or a heavier pose (ground level at a hilltop measured 76 fps), not the fps line.

## Lin-look, collision & lag pass (2026-09-05, after the pivot) — operational deltas

Full record: `research/lin-look-log.md`; backlog Groups Z–AC (goals 164–176) in `docs/goals.md`.
Machine-relevant deltas ONLY:

- **`--debug-view NAME`** renders ONE shading term per frame:
  `lit|ao|normal|facenormal|level|steps|coverage|cubepx|smooth|lodcube|material|distance`
  (`tools/svo_render --view` takes the same names). Reach for it before staring at a composite —
  each view attributed one bug in this pass. TAA is off under a debug view; `--verify-frame`
  captures one without judging it.
- **Shaders load at runtime**: edit `render/diligent/shaders/*.hlsl` and relaunch, no rebuild.
  That is what made the water bisection (swap one `return` line, sample one pixel row with a
  five-line Python script) a four-run job.
- **`svo_render --lod-center x,y,z`** builds the LOD around a point other than the camera — the
  deterministic reproduction of "the camera moved away from the last build center". Any artifact
  that appears "after the rebuild finishes" starts here, on the CPU, not in the app.
- New app flags: `--no-taa`, `--smooth-pixels N` (ancestor span for the averaged normal, ~6 px),
  `--grain A` / `--no-grain` (per-cube brightness grain, default 0.10), `--ao-radius PX`,
  `--shadow-lod M`, `--svo-threads N` (build pool; default 3/4 of the hardware threads — a build on
  EVERY thread starved `present`, measured: 12 of 13 slow frames), `--svo-upload-mb N` (slice
  size, default 32; 8 made the stutter WORSE), `--noclip` (the old spectator — collision is on by
  default in fly AND walk).
- **Vulkan faults if the app's very first command is a timestamp query** (`vkCmdWriteTimestamp`
  inside the NVIDIA driver, with no post-processor warm-up ahead of it); the `gpu march+resolve`
  timer skips the first two frames. The overlay and the 2 s stats line print it — that number
  (3.2–6.3 ms everywhere measured) is how "the lag is not the rendering" was settled.
- **Slow-frame attributor**: on the svo path every frame over 20 ms logs
  `slow frame N: X ms = upload + camera + render + post + overlay + present` plus what was happening
  (uploading / tree swapped / building), and the exit summary counts them by cause. A "why did it
  hitch" question starts with a 900-frame `--autofly --walk` run and this log, not with a profiler.
- **`--verify-frame` reads 34% on both backends now**; the section above's 48% counted the moiré
  as contrast. Threshold unchanged (6%).
- **Tree layout v2**: internal nodes and brick leaves carry one attribute word after the header
  (int8 x3 average normal + uint8 coverage), so the child-slot arithmetic moved everywhere.
  `tree_layout.hpp`, `ray_trace.cpp` and `svo_march.psh.hlsl` change together; the 7,000-ray
  oracle is the check.
- `tools/svo_render` writes its PNGs stored, not compressed (2,765,798 bytes at 1280x720 = raw
  RGB + headers) — re-save through PIL (12.x is installed, Python 3.14) before committing one.
- **113/113 tests** (9 new in `world/collision`, 3 new in `world/svo`). Captures for the pass:
  `research/captures/lin_*.png` (named at the end of the research log).

## Phase status

**Phase 0 (repo scaffold + dependency fetch/build smoke test): DONE.** Clean configure+build
succeeds with all 5 dependencies — DiligentCore/Tools/FX build with D3D11, D3D12, OpenGL, and
Vulkan backends all linking — and our own stub `voxel_app`/`mesh_dump` executables build and run.

**Phase 1 M1.1–M1.7: DONE (2026-09-03).** See `Phase 1 brief` §8 and
`Phase 1 completion brief`'s per-group logs. `voxel_app` opens a window, streams Surface-Nets
terrain in/out around a flyable spectator camera (WASD + Space/Ctrl, RMB mouse-look, Shift boost,
Esc quits) on Vulkan AND D3D12, with a Tracy client + ImGui overlay + VK_EXT_memory_budget
diagnostics. 49/49 tests. Smoke flags: `--mode vk|d3d12`, `--frames N`, `--verify-frame`
(mechanical terrain-visible check via back-buffer readback), `--autofly` (bounded-memory streaming
check), `--radius`, `--seed`, `--validation`. Headless CI subset: configure with
`-DVOXEL_BUILD_RENDERER=OFF` (no Diligent/GLFW/Tracy fetch); static analysis:
`-DVOXEL_CLANG_TIDY=ON` (on this machine pass
`-DVOXEL_CLANG_TIDY_EXE="C:/Program Files/LLVM/bin/clang-tidy.exe"`). Known deferred: in-app
RenderDoc trigger (no vendored `renderdoc_app.h`; launch through RenderDoc UI instead),
`tools/mesh_dump` .obj export, first GitHub-runner execution of `.github/workflows/ci.yml`.

## Visual/gameplay/CI pass (2026-09-04, after the terrain-fixes pass) — what changed operationally

Full record: `research/visual-stage-log.md` (+ `docs/render-pipeline.md`, `docs/progress.md`).
Machine-relevant deltas ONLY (read those files for the why):

- **`--verify-frame` metric REPLACED** (twice — bloom then the gradient sky broke every
  reference-pixel scheme): it now counts LOCAL-CONTRAST pixels (neighbor delta >4/255); terrain
  measures 12–14%, sky-only 0.9%, threshold 6%. The old "25%" numbers in the section above are
  historical.
- **8 materials** now (Sand=6, Grass=7 appended after the decoration IDs); both frozen palette
  counts moved 6→8 together. Every solid used to be Stone — surface banding lives in
  `terrain_fill.cpp` (seam-exact 34×34 margin grid).
- New flags: `--pos x,y,z --yaw D --pitch D` (debug camera), `--dump-every N`, `--no-post`,
  `--no-bloom`, `--no-tonemap`, `--no-sky`; F2 = in-app screenshot; `VOXEL_DUMP_FRAME` writes
  PNG now (`.ppm` extension still honored). `tools/mesh_dump [cx cy cz] [seed] [out.obj]` exports
  real .obj.
- The 12B vertex's 4th byte is CONTEXT-DEPENDENT: baked AO on land, water-column depth on water,
  per-tree brightness jitter on trees — all documented at the pack sites and tested; widen to 16B
  rather than pack a fourth meaning (goals.md goal 110).
- PostProcessor must be constructed BEFORE TerrainRenderer (scene-target format flows into the
  terrain/sky PSOs), and DiligentFX Bloom needs its one warm-up PostFXContext::Execute — see
  post_process.cpp before touching either.
- **76/76 tests**; walk mode now SWIMS over deep water (buoyancy; ground_height no longer
  sea-clamps — the old "stride on water" note above is historical).
- CI is REAL and green (run 33941021916: cores ×3, ASan/UBSan, TSan w/ .tsan-suppressions,
  clang-tidy, Windows renderer + WARP smoke). The workflow MUST NOT get a `branches:` filter
  containing `C++-voxel` — `+` is a glob quantifier and kills every run at startup with zero jobs.
- `.clang-format` exists now (calibrated; tree-wide pass applied). clang-tidy green including new
  code.
- **RTK/Bash heredoc gotcha (this machine)**: the command hook collapses `\` → `\` inside
  heredocs — a `'\0'` written via bash became a literal NUL byte in the file (invisible in most
  displays, C2137 from MSVC). For escape-sensitive edits use the Edit tool, or build bytes from
  char codes (`bytes([0x5C, 0x30])`).
- One OPEN visual defect: floating sliver curtains at rare grazing angles — full repro + hunt
  state in `research/water-foliage-design.md`; next tool is RenderDoc (goals 73/105). Mesh data
  proven clean three ways; don't re-run the offline hunts, they're now permanent tests.
