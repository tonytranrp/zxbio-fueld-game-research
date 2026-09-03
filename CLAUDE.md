# C++ Voxel Engine (branch: `C++-voxel`)

Phase-based build per [`PROJECT_BRIEF.md`](PROJECT_BRIEF.md) (John Lin-inspired voxel terrain
engine on DiligentEngine + EnTT + GLM + GLFW + FastNoise2, CMake + CPM.cmake). Read that file for
vision/architecture/phase roadmap — this file is operational build notes only, specific to this
machine, learned the hard way during Phase 0. Read it before running `cmake` on this project.

## Building — must read before running cmake

**Build to a short out-of-repo directory, not `build/` inside this repo.** This repo's own path
is long enough that DiligentCore's `ThirdParty/glslang` build (whose object paths mirror the full
absolute CPM cache source path) exceeds Windows' 260-char MAX_PATH when nested under
`<repo>\build\_deps\...`, and `cl.exe` fails with `fatal error C1083: Cannot open compiler
generated file: '': Invalid argument`. Use `C:\b` (verified working):

```
cmake -B C:\b -G Ninja -DGIT_EXECUTABLE="C:/Program Files/Git/cmd/git.exe"
cmake --build C:\b
```

**Must run from a real MSVC environment**, not a plain shell — `cl`/`link` aren't on PATH by
default. Source the VS toolchain first (adjust the version folder below if it changes):

```
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
```

This machine's Visual Studio is branded **"18" / 2026** internally (folder
`...\Microsoft Visual Studio\18\BuildTools\`), not `2022` — `vswhere.exe` does **not** detect it
(its registry/COM-based discovery is broken on this machine; confirmed working via direct
filesystem search + `vcvars64.bat` instead). Don't trust `vswhere -all` returning empty as proof
MSVC is absent — check for `cl.exe` directly:
`C:\Program Files (x86)\Microsoft Visual Studio\**\Hostx64\x64\cl.exe`.

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
FetchContent's internal `git rev-parse HEAD^0` into `HEAD0` (`fatal: ambiguous argument 'HEAD0'`).
Always pass `-DGIT_EXECUTABLE="C:/Program Files/Git/cmd/git.exe"` explicitly when configuring.

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

## Phase status

**Phase 0 (repo scaffold + dependency fetch/build smoke test): DONE.** Clean configure+build
succeeds with all 5 dependencies — DiligentCore/Tools/FX build with D3D11, D3D12, OpenGL, and
Vulkan backends all linking — and our own stub `voxel_app`/`mesh_dump` executables build and run.
See `PROJECT_BRIEF.md` §11 for Phase 1's definition of done before starting it.
