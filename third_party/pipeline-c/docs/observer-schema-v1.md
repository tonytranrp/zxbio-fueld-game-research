# pb.observer.v1 — observer ABI and verbose event-line schema

**Status:** stable. Custom observer implementations and downstream log-
parsing tools may rely on the contracts described on this page across
all v1.x releases of Pipeline-c++.

This page formalizes two related but independent contracts:

1. **`pb.observer.v1`** — the C++ ABI of `pb::runtime::observer` (vtable
   shape, method signatures, default behaviors).
2. **`pb.observer.verbose.v1`** — the text-line schema produced by
   `pb::verbose_observer` when attached via `pb::with_verbose_diagnostics`
   or installed manually.

The two schemas bump independently. A non-ABI-breaking change to a
verbose log-line still bumps `pb.observer.verbose.v1`; an ABI-breaking
change to the observer vtable bumps `pb.observer.v1` but does not
necessarily affect the verbose line schema.

## `pb.observer.v1` — ABI contract

Identifier: `pb::runtime::observer_schema_version == "pb.observer.v1"`.

### Vtable layout (locked)

The `pb::runtime::observer` struct exposes seven virtual methods in
**this order**. Re-ordering, removing, or renaming any of them is an
ABI break and bumps to `pb.observer.v2`.

```cpp
struct observer {
  virtual ~observer() = default;

  virtual void on_stage_start    (const stage_id&)                       = 0;
  virtual void on_stage_success  (const stage_id&)                       = 0;
  virtual void on_stage_failure  (const stage_id&, const error&)         = 0;
  virtual void on_stage_exception(const stage_id&, const error&)         = 0;
  virtual void on_case_selected  (const stage_id& branch_id,
                                  std::size_t     case_index,
                                  const stage_id& predicate_id)          = 0;
  virtual void on_case_skipped   (const stage_id& branch_id,
                                  std::size_t     case_index,
                                  const stage_id& predicate_id)          = 0;
  virtual void on_case_failed    (const stage_id& branch_id,
                                  std::size_t     case_index,
                                  const stage_id& case_stage_id,
                                  const error&    diagnostic)            = 0;
};
```

> The signatures above use `= 0` for clarity — in the actual header
> they are non-pure with default `{}` bodies so derived observers only
> override what they need. The vtable position and signature are what
> the v1 contract pins; the default-body convenience is also part of v1.

### Stability invariants (v1.x)

1. The seven virtual methods MUST keep their exact names, parameter
   types, return types, and vtable order.
2. The default no-op bodies MUST remain — a v2 cannot silently make a
   method pure and break existing derived classes.
3. The destructor MUST remain virtual and stay the first vtable slot.
4. `stage_id`, `error`, and `error_category` referenced by the
   signatures are themselves part of the v1 ABI surface — changes to
   their layout require a v2 bump of this schema.

### Permitted v1.x changes

- Adding a new virtual method with a default no-op body at the END of
  the vtable. Derived observers that don't override it pay no cost.
  Existing observers compiled against earlier v1.x headers continue
  to link because their vtable layouts match for the earlier slots.
- Adding new non-virtual helpers on `observer`.

### Forbidden v1.x changes

- Renaming a method.
- Removing a method.
- Changing any parameter or return type.
- Making a method pure (`= 0`).
- Reordering virtual methods (changes the vtable).
- Inserting a new virtual method in the middle of the vtable.

A change that touches any of these MUST be implemented as
`pb.observer.v2`, shipping the v1 observer alongside the new one.

## `pb.observer.verbose.v1` — event line schema

Identifier: `pb::runtime::verbose_observer_schema_version == "pb.observer.verbose.v1"`.

`pb::verbose_observer` produces newline-terminated text records, one
per call. Every line begins with the prefix `[pb.verbose]` followed by
an event kind and field-value pairs. The schema MUST be parseable line
by line: log-tailing tools can grep for `[pb.verbose] stage_failure` or
`[pb.verbose] case_failed` without parsing free-form text.

### Locked line formats

| Event kind | Line template |
| --- | --- |
| `stage_start`     | `[pb.verbose] stage_start stage=<key>\n` |
| `stage_success`   | `[pb.verbose] stage_success stage=<key>\n` |
| `stage_failure`   | `[pb.verbose] stage_failure stage=<key> category=<cat>[ message=<msg>]\n` |
| `stage_exception` | `[pb.verbose] stage_exception stage=<key> category=<cat>[ message=<msg>]\n` |
| `case_selected`   | `[pb.verbose] case_selected branch=<key> case=<idx> predicate=<key>\n` |
| `case_skipped`    | `[pb.verbose] case_skipped branch=<key> case=<idx> predicate=<key>\n` |
| `case_failed`     | `[pb.verbose] case_failed branch=<key> case=<idx> stage=<key> category=<cat>[ message=<msg>]\n` |
| `fan_in_started` | `[pb.verbose] fan_in_started branch=<key> cases=<n>\n` |
| `fan_in_case_scheduled` | `[pb.verbose] fan_in_case_scheduled branch=<key> case=<idx> stage=<key>\n` |
| `fan_in_case_completed` | `[pb.verbose] fan_in_case_completed branch=<key> case=<idx> stage=<key> success=<true|false>\n` |
| `fan_in_completed` | `[pb.verbose] fan_in_completed branch=<key> selected=<n> completed=<n> failed=<n>\n` |

Rules:

- `<key>` is `stage_id.key` if non-empty, else `stage_id.name`. Empty
  keys + empty names produce empty values (the field is still
  emitted).
- `<idx>` is the case index as a decimal integer.
- `<n>` is a decimal count. For fan-in completion records, `selected` is
  the number of predicate-selected cases, `completed` is the number of
  case stages that completed successfully, and `failed` is the number of
  predicate or stage failures recorded for that fan-in branch.
- `<cat>` is the `error_category` enumerator name produced by
  `pb::runtime::category_name(...)` — currently one of
  `stage_failure`, `expected_error`, `exception`, `contract_violation`.
- The `message=<msg>` field is emitted ONLY when the error carries a
  non-empty message. It is the LAST field on its line.
- Field order on each line is fixed and forms part of the v1
  contract. A reorder is a v2 bump.

### Stability invariants (verbose v1.x)

1. The `[pb.verbose]` prefix MUST appear as the first 12 characters of
   every emitted line.
2. The space-separated `event_kind` MUST be the second token on every
   line, exactly matching the event-kind names listed in the table.
3. Existing `<field>=<value>` keys MUST keep their spelling and
   position on the line.
4. The newline terminator is `'\n'` — not `\r\n`, even on Windows
   builds.

### Permitted verbose v1.x changes

- Adding a new event kind (with a new locked line template
  documented here) for a newly-added observer hook.
- Adding a new OPTIONAL field at the END of an existing line. Tools
  MUST tolerate trailing fields they don't recognize.

### Forbidden verbose v1.x changes

- Renaming a field key.
- Removing a field.
- Reordering fields within a line.
- Changing the prefix or the newline terminator.
- Splitting one event across multiple lines.

## Conformance tests

- `tests/runtime/observer_schema_v1_regression.cpp` — pins every
  documented event line format with a byte-equal regression.
- `tests/compile_pass/schema_v1_cross_version_regression.cpp` — pins
  the `observer_schema_version` and `verbose_observer_schema_version`
  identifiers.

A change that breaks either test is a schema break. Either the change
is wrong, or the major must be bumped.

## Migration to v2 (future)

The v2 migration procedure mirrors the export-schema procedure
documented in [docs/export-schema-v1.md](export-schema-v1.md):

1. Add the new observer type alongside `observer` (e.g. `observer_v2`).
2. Keep `observer` and all v1 conformance tests as a regression net.
3. Document the diff under `observer-schema-v2.md`.
4. Update factory functions (`with_verbose_diagnostics`, etc.) to
   advertise both versions, with v1 staying the default until the
   library itself does a major release.

## See also

- [include/pb/runtime/observer.hpp](../include/pb/runtime/observer.hpp) — the v1 ABI
- [include/pb/runtime/error_policy.hpp](../include/pb/runtime/error_policy.hpp) — `pb::verbose_observer` source
- [docs/export-schema-v1.md](export-schema-v1.md) — sibling schema with the same stability promise
- [docs/observer-hooks-roadmap.md](observer-hooks-roadmap.md) — broader observer roadmap
