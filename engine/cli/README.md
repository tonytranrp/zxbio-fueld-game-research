# `engine/cli` — one declarative option layer for every executable

## What belongs here

The *mechanism* for turning a command line into a struct, and nothing else. This module knows
about strings, member pointers and diagnostics. It does not know what a renderer, a scenario or a
voxel is.

- `value.hpp` — the small vocabulary types: `ValueKind`, `Size`, `EnumEntry`, `Status`.
- `option.hpp` — `Option` (one declared row) and `bind<&Struct::member>()`, the type erasure.
- `parser.hpp` — `parse()`, and the three diagnostics.
- `help.hpp` — `--help`, generated from the table so it cannot drift from it.
- `response_file.hpp` — `@file` expansion, the mechanism `.scn` scenarios carry engine settings on.
- `detail/` — the conversions. Not part of the public API.

## What does not belong here

- Any option *table*. A table names the program's own struct, so it lives with that program
  (`app/src/app_options.hpp`, `tools/svo_render/src/render_options.hpp`, …). This module never
  gains a `#include` of one.
- Logging. `parse()` returns its diagnostic as a string; the caller decides how to show it. That
  is what lets a test assert on the exact message.

## The rules of this folder

1. **An option is data, not code.** A row is a `constexpr` aggregate: name, alias, kind, setter,
   help, default text, group. Adding an option is adding a row — never a branch in a parser.
2. **One `void*` + one function pointer per row.** The setter is
   `Status(*)(void* base, std::string_view token, const Option& self)`, produced by
   `bind<&Owner::member>()`. There is no `std::variant` of every option type in the program, and
   no template instantiated per option kind at the call site
   (`cpp-heavy-templates/references/templates-and-metaprogramming.md` §5).
3. **The table is compile-time; the parse is a plain runtime loop.** No expression templates.
   `static_assert(has_unique_names(kTable))` at every table's definition site.
4. **Every template parameter is constrained.** No bare `typename T` on the public API
   (`templates-and-metaprogramming.md` §1).
5. **Three distinct diagnostics, each naming the option**: unknown option, missing value,
   unparseable value. A test asserts all three.
6. **`--no-x` is a row property, not a parser special case.** A `ValueKind::Toggle` row answers to
   both `--x` and `--no-x`, and both reach the same target.
7. **Aliases are a row property too.** `--root-log2` is `--region-log2`'s `alias`; there is no
   second row and no second target to keep in sync.
