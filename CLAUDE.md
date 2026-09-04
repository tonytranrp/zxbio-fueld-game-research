# C++ Voxel Engine (branch: `C++-voxel`)

Phase-based build per [`PROJECT_BRIEF.md`](PROJECT_BRIEF.md) (John Lin-inspired voxel terrain
engine on DiligentEngine + EnTT + GLM + GLFW + FastNoise2, CMake + CPM.cmake). Read that file for
vision/architecture/phase roadmap — this file is operational build notes only, specific to this
machine, learned the hard way during Phase 0. Read it before running `cmake` on this project.

## Building — must read before running cmake

**Use the `CMakePresets.json` at the repo root** — this is now the primary, recommended way to
build, and it's what makes Visual Studio's/VS Code's own "Open Folder" CMake integration work
without any manual setup: opening this repo's folder in either one auto-detects the
`windows-debug`/`windows-release` presets and Just Works. From a command line (after sourcing a
real MSVC environment — see below), the equivalent is:

```
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug
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
already does this — see `M1_2_BRIEF.md` §2.5), pin to `SSE2` instead: it's the actual lowest level
compiled in, and still a safe universal floor for this x86-64-only project (SSE2 is mandatory
baseline on every x86-64 CPU, unlike SSE41/AVX2/AVX512). `world/generation/src/heightmap_generator.cpp`
wraps `FastNoise::New<T>` in a helper that throws instead of returning null/segfaulting if this
ever regresses — check there before assuming a given `FeatureSet` value is safe to request.

## Phase status

**Phase 0 (repo scaffold + dependency fetch/build smoke test): DONE.** Clean configure+build
succeeds with all 5 dependencies — DiligentCore/Tools/FX build with D3D11, D3D12, OpenGL, and
Vulkan backends all linking — and our own stub `voxel_app`/`mesh_dump` executables build and run.

**Phase 1 M1.1–M1.7: DONE (2026-09-03).** See `PHASE_1_BRIEF.md` §8 and
`PHASE_1_COMPLETION_BRIEF.md`'s per-group logs. `voxel_app` opens a window, streams Surface-Nets
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
