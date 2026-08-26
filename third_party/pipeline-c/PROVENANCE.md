# Pipeline-c-

This is the project owner's own library (<https://github.com/tonytranrp/Pipeline-c->),
vendored here as source so it can be customized in-tree for this game specifically, instead of
being fetched as an external CPM dependency on every from-scratch configure.

- Checked out at commit `d8bc48f9f4b2c52424dd08e5e82f13052b3bbeb6` -- the same commit the old
  `CPMAddPackage` URL in the root `CMakeLists.txt` used to pin.
- License: MIT (see `LICENSE` in this directory), copyright 2026 tonytranrp.
- Consumed via `add_subdirectory("third_party/pipeline-c")` in the root `CMakeLists.txt`, which
  exposes the same `pb::core` / `pb::runtime` targets the old CPM setup did -- no other CMake
  code needed to change.

## Updating / customizing

Edit the files under this directory directly; they are normal, tracked, editable source in this
repo now, not a pristine upstream mirror. There is no expectation of staying in sync with the
upstream `Pipeline-c-` repo going forward -- if useful upstream changes land there later, pull
them in manually (e.g. `git diff` against a fresh clone of upstream at whatever commit you want)
rather than re-vendoring wholesale, since that would clobber any local customization.
