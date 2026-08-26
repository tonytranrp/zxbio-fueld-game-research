# raylib 5.5 prebuilt Windows binaries

Vendored from raylib's own official GitHub release (not built by this project) so a default
Windows dev build never has to configure/compile raylib from source — see the "Build
performance" note in the root `CMakeLists.txt` for why.

- Source: <https://github.com/raysan5/raylib/releases/download/5.5/raylib-5.5_win64_msvc16.zip>
- SHA256 of that zip: `8d046084d12353183e701ef4c9d276c21fcd3243c2a368091fabfb2769b8507c`
- Toolset: MSVC16 (VS2019, ABI-compatible with newer MSVC toolsets via Microsoft's VC14x binary
  compatibility guarantee, as long as the consumer also links the dynamic CRT -- this repo does,
  see `CMAKE_MSVC_RUNTIME_LIBRARY` in the root `CMakeLists.txt`)
- License: zlib (see `LICENSE` in this directory) -- same as every other raylib distribution
  channel; already reflected in `THIRD-PARTY-NOTICES.md`.

Only the files this project actually uses are kept (not the full release archive):
`include/{raylib,raymath,rlgl}.h`, `lib/raylib.dll` + `lib/raylibdll.lib` (the shared-library
variant, matching this project's existing `BUILD_SHARED_LIBS ON` choice for raylib -- not the
static `lib/raylib.lib` that also ships in the release zip).

## Updating to a newer raylib version

1. Download the new version's `raylib-<version>_win64_msvc16.zip` from
   <https://github.com/raysan5/raylib/releases>.
2. Replace the 6 files in `include/` and `lib/` above with the new release's copies, and this
   file's version/URL/hash.
3. Bump the version pin in the non-Windows CPM fallback in the root `CMakeLists.txt` to match,
   so Windows and other platforms stay on the same raylib version.
