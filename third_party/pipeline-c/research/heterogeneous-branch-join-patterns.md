# Heterogeneous Branch/Join Patterns in C++20/23 — Research Report

**Date:** 2025-07-16  
**Scope:** Patterns for variant-based dispatch, heterogeneous fan-out/fan-in, type_list join stages, and variant construction relevant to the `pb::` pipeline builder DSL.  
**Status:** Research complete. No implementation code written.

---

## Question

What are the practical C++20/23 patterns for implementing **heterogeneous branch joins** — where branch cases produce *different* output types that must converge at a join stage — within a type-level pipeline builder DSL like `pb::`?

---

## Sources Consulted

| # | Source | Version / Location | Authority |
|---|--------|--------------------|-----------|
| S1 | `pb::` codebase: `pipeline_state.hpp`, `meta.hpp`, `sequential.hpp`, `concepts.hpp` | HEAD (local) | Primary — the target DSL |
| S2 | `pb::` codebase: `branch-join-roadmap.md`, `sequential-branch-execution-limitations.md` | HEAD (local) | Primary — documented roadmap & constraints |
| S3 | ISO C++20 Standard: `std::variant`, `std::visit`, `std::type_identity` | C++20 (N4861) | Definitive — language feature reference |
| S4 | ISO C++23 Standard: `std::visit<R>` explicit return type | C++23 (N4950) | Definitive — language feature reference |
| S5 | Taskflow: `tf::ConditionalTasking` | v3.7+ (official docs) | Primary — competing pipeline library pattern |
| S6 | P2300 (`std::execution`): `let_value`, `let_error`, `let_stopped` senders | R9 (2024-05) | Primary — standardization-track sender/receiver |
| S7 | Overload pattern: `template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };` | Common C++17+ idiom | Secondary — well-established community pattern |

---

## Findings

### 1. Variant-Based Dispatch Patterns (`std::variant`, `std::visit`)

**Core mechanism confirmed (S3, §20.7.3):** `std::variant<Ts...>` stores one of several alternatives. `std::visit` applies a callable to the active alternative at runtime. This is the fundamental mechanism for heterogeneous value convergence in C++.

**The overload pattern (S7) — confirmed best practice:**

```cpp
template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
// CTAD deduction guide (C++17):
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;
```

This pattern is the canonical C++ approach for `std::visit` dispatch. It aggregates multiple callables into a single visitor. It is used pervasively in production codebases and is the recommended approach in cppreference examples.

**`std::visit<R>` explicit return type (S4, C++23):** C++23 adds an explicit return-type form `std::visit<R>(visitor, variant)`. This eliminates the need for all visitor overloads to return the same type (the compiler uses `R` instead) and prevents implicit-conversion ambiguity. This is relevant for a join stage that must produce a unified output type from heterogeneous variant alternatives.

**Direct applicability to `pb::`:** A heterogeneous branch produces `std::variant<OutputA, OutputB, OutputC>`. The join stage receives this variant and uses `std::visit` (with either the overload pattern or explicit return type) to produce a unified `JoinOutput`.

**Confidence: Confirmed.** The overload pattern + `std::visit` is the standard C++ mechanism. C++23 `std::visit<R>` is a quality-of-life upgrade but not required.

---

### 2. Taskflow Conditional Tasking (Heterogeneous Fan-Out)

**Mechanism (S5):** Taskflow's `tf::ConditionalTasking` uses a `tf::Condition` object that returns an integer index. That index selects which successor task executes next:

```cpp
tf::Taskflow taskflow;
auto cond = taskflow.emplace([](){ return 0; }); // returns index
auto A = taskflow.emplace([&]{ process_foo(); });
auto B = taskflow.emplace([&]{ process_bar(); });
cond.precede(A, B); // cond → {A, B}, index selects
```

**Key design observation:** Taskflow's fan-out is *index-based*, not type-based. All branch paths are type-erased to `tf::Task` handles. The "heterogeneity" is in the runtime behavior of each task, not in the type system. There is no compile-time join that unifies heterogeneous output types — each task is `void`-returning or communicates via captured shared state.

**For `pb::` contrast:** `pb::` tracks types through the entire pipeline. Taskflow's approach of erasing branch output types via shared mutable state (captured by reference) contradicts `pb::`'s core value proposition: compile-time type safety across stage boundaries. The index-based dispatch pattern, however, is analogous to what `pb::` already does (first-match-wins predicate evaluation → index → selected branch stage).

**Confidence: Confirmed.** Taskflow's conditional tasking is relevant as a dispatch pattern (predicate → index → route) but its type-erased approach to fan-in is not suitable for `pb::`.

---

### 3. `stdexec` / P2300 `let_value` and `let_error` Senders

**Mechanism (S6):** P2300 sender/receiver defines `let_value`, `let_error`, and `let_stopped` as sender adaptors that *dynamically choose the next sender* based on the output value or error of a predecessor:

```cpp
// let_value: takes a sender S and a function F(S's-value-type) → new-sender
auto s = ex::let_value(ex::just(42), [](int v) {
    if (v > 0) return ex::just(std::to_string(v));
    else       return ex::just(0.0f);            // different type!
});
// Result sender produces std::variant<std::string, float>
```

**Critical finding:** `let_value` and `let_error` produce a **variant of all possible output types** from the callback branches. This is the `stdexec` way of handling heterogeneous fan-in: the callback returns different sender types, and the framework automatically unifies them into `std::variant<...>`.

The P2300 specification (R9) formalizes this via `variant-sender` concepts and `value_types_of_t` metafunctions that compute the variant.

**For `pb::` applicability:** This is exactly the pattern needed for heterogeneous branch join. The key insight is:

1. Each branch case produces its own output type `OutputType_i`.
2. The branch node's collective output becomes `std::variant<OutputType_1, OutputType_2, ...>`.
3. The join stage receives this variant and produces a unified `JoinOutput`.

The `let_value` pattern demonstrates that *automatic variant construction from heterogeneous branches* is a validated, standardization-track approach.

**Confidence: Confirmed.** P2300's `let_value` provides the strongest precedent for automatic heterogeneous-to-variant unification in C++ pipeline designs.

---

### 4. `type_list`-Based Join Stages

**Current `pb::` context (S1, S2):** The existing `pb::` codebase already has the foundational infrastructure:

- `pb::meta::type_list<Ts...>` — a compile-time type container (S1: `meta.hpp`)
- `branch_node<Cases...>::output_types` — already computes `type_list<OutputA, OutputB, ...>` (S1: `pipeline_state.hpp` line ~248: `using output_types = meta::type_list<typename branch_case_output<Cases>::output_type...>;`)
- `pb::branch_outputs<Cases...>::output_types` — same type list (S1: `pipeline_state.hpp` line ~260)

**The roadblock (S2):** The current homogeneous-only constraint is enforced in `branch_output_validation_impl`:

```cpp
// pipeline_state.hpp, ~line 105
static_assert(branch_outputs_match_output<Output, Cases...>::value,
              "Branch output validation mismatch: every branch output_type must match requested output_type");
```

And in `selected_branch_node`:

```cpp
// pipeline_state.hpp, ~line 230
using output_type = typename validation::output_type; // single type
```

The branch node's `output_type` is a *single* type (the common homogeneous output). For heterogeneous support, this must become the variant.

**Design for type_list-based join input:**

A heterogeneous join stage declares its `input_type` as `meta::type_list<OutputA, OutputB, OutputC>` (or equivalently `std::variant<OutputA, OutputB, OutputC>`). Two design choices:

| Approach | Input Type | Join Stage Signature | Pros | Cons |
|----------|-----------|---------------------|------|------|
| **A: type_list input** | `meta::type_list<A, B, C>` | `operator()(std::variant<A,B,C>)` or overload set | Matches existing `pb::` meta vocabulary; explicit about type set | Requires join stage to explicitly list variants; more verbose |
| **B: variant input** | `std::variant<A, B, C>` | `operator()(std::variant<A,B,C>)` | Natural C++; simple `std::visit` | Less explicit about the type set at the meta level |

**Recommendation: Approach B (variant input) with automatic type_list-to-variant conversion.** The branch node's `output_types` type_list is converted to `std::variant<Outputs...>` via a `meta::to_variant_t` alias. The join stage declares `input_type = std::variant<A, B, C>` and validation checks that this matches the computed variant from the branch.

**Confidence: Likely.** Approach B is cleaner and aligns with both P2300 precedent and standard C++ practice. The type_list remains the internal representation, with variant as the user-facing contract.

---

### 5. Compile-Time Conversion from Homogeneous List to Variant

**Meta-algorithm (S1, S3):**

The conversion from `meta::type_list<A, B, C>` to `std::variant<A, B, C>` is a straightforward meta-algorithm:

```cpp
// In pb::meta namespace:
template <class List>
struct to_variant;

template <class... Ts>
struct to_variant<type_list<Ts...>> {
    using type = std::variant<Ts...>;
};

template <class List>
using to_variant_t = typename to_variant<List>::type;
```

This is a `O(1)` template instantiation (simple pack expansion). No recursion needed.

**Where it hooks into `pb::` (S1):**

The `branch_node<Cases...>` already computes `output_types` as `meta::type_list<...>`. Adding a `variant_output` alias:

```cpp
template <class... Cases>
struct branch_node {
    // ... existing members ...
    using output_types = meta::type_list<typename branch_case_output<Cases>::output_type...>;
    using variant_output = meta::to_variant_t<output_types>;  // NEW
};
```

The `selected_branch_node` then uses `variant_output` as its `output_type` when cases are heterogeneous.

**Detection of homogeneity vs. heterogeneity:**

A compile-time check determines whether branch outputs are already homogeneous (can skip variant wrapping):

```cpp
template <class... Ts>
struct are_all_same : std::false_type {};

template <class T>
struct are_all_same<T> : std::true_type {};

template <class T, class... Rest>
struct are_all_same<T, T, Rest...> : are_all_same<T, Rest...> {};
// fallback: are_all_same<T, U, Rest...> : std::false_type {}
```

If `are_all_same<Outputs...>::value` is true, `output_type` stays as the single type (backward-compatible with current homogeneous behavior). If false, `output_type` becomes `std::variant<Outputs...>`.

**Confidence: Confirmed.** This is a standard pack-expansion metaprogramming pattern. The meta-algorithm is trivial. The design decision is about when to apply it.

---

### 6. Designing a Join Stage for Multiple Input Types

**Pattern A: Variant-accepting join with `std::visit` + overloaded**

```cpp
struct Finalize {
    using input_type  = std::variant<ProcessedInvoice, ProcessedReport, ProcessedMemo>;
    using output_type = int;

    static constexpr auto stage_name() noexcept { return "finalize"; }

    int operator()(const input_type& var) const {
        return std::visit(overloaded{
            [](const ProcessedInvoice& doc) { return doc.amount * 100; },
            [](const ProcessedReport& doc)  { return doc.pages * 10; },
            [](const ProcessedMemo& doc)    { return doc.words / 10; },
        }, var);
    }
};
```

**Pattern B: Join stage as a type_list consumer (matching internal DSL convention)**

```cpp
struct Finalize {
    // input_type: the join stage declares it accepts each branch output type
    using input_type  = pb::meta::type_list<ProcessedInvoice, ProcessedReport, ProcessedMemo>;
    using output_type = int;

    // The framework synthesizes: operator()(std::variant<ProcessedInvoice, ProcessedReport, ProcessedMemo>)
    // Requires the join stage to define handlers for each type:
    static constexpr auto handle(ProcessedInvoice doc) { return doc.amount * 100; }
    static constexpr auto handle(ProcessedReport doc)  { return doc.pages * 10; }
    static constexpr auto handle(ProcessedMemo doc)    { return doc.words / 10; }
};
```

Pattern B is more verbose but gives `pb::` the ability to validate at compile time that the join stage handles every branch output type. Pattern A is simpler but relies on the user getting the overload set right (and the compiler catching missing overloads at `std::visit` instantiation time).

**Recommendation: Pattern A for the MVP, Pattern B as an opt-in enhancement.** Pattern A is immediately usable with existing C++17+ and doesn't require framework synthesis of `operator()`. The compiler already errors if `std::visit` doesn't cover all alternatives (with a reasonably clear message). Pattern B can be added later as a `pb::join_handler<...>` wrapper that provides `handle` method validation.

**Confidence: Confirmed.** Both patterns are valid. Pattern A is lower-friction for the first heterogeneous release.

---

### 7. Additional Relevant C++23/26 Features

**C++23 `std::visit<R>` (S4):** Explicit return type avoids the common-visitor-return-type requirement. This is a minor improvement — the overload pattern already handles this well.

**C++23 explicit object parameters (`this auto&& self`):** Could enable join stages that are templated on the variant alternative without the overload pattern, but the overload pattern is more explicit and readable.

**C++26 pack indexing (`Ts...[I]`):** Simplifies certain meta-algorithms that index into type lists, but `pb::meta::at_t` already handles this. Pack indexing would reduce compile times marginally.

**C++26 `std::execution` (P2300):** Once compiler support stabilizes, the `let_value`/`let_error` senders provide a production-grade async branch/join model that `pb::` can lower to.

**Confidence: Confirmed.** C++23 `std::visit<R>` is the most immediately useful new feature. C++26 pack indexing and `std::execution` are future-enabling.

---

## Comparison: Homogeneous vs. Heterogeneous Branch/Join

| Dimension | Homogeneous (Current) | Heterogeneous (Proposed) |
|-----------|----------------------|--------------------------|
| Branch output | Single type `T` | `std::variant<A, B, C>` |
| Validation | `branch_outputs_match_output` static_assert | Variant type equality check |
| Join input | `T` (direct match) | `std::variant<A, B, C>` |
| Join execution | Direct call `join_stage(input)` | `std::visit(overloaded{...}, variant_input)` |
| DSL syntax | `::branch<CaseA, CaseB>::join<Join>::to<Out>` | Same DSL — no syntax change needed |
| Compile time | O(1) type check | O(1) variant check + O(N) overload resolution |
| Runtime | Direct dispatch | One indirect `std::visit` dispatch (~function pointer overhead) |
| Backward compat | — | Fully backward-compatible via `are_all_same` detection |

---

## Recommendation

### Architecture for Heterogeneous Branch/Join in `pb::`

**Step 1 — Add `meta::to_variant`:**

```cpp
// meta.hpp addition
template <class List> struct to_variant;
template <class... Ts> struct to_variant<type_list<Ts...>> {
    using type = std::variant<Ts...>;
};
template <class List> using to_variant_t = typename to_variant<List>::type;
```

**Step 2 — Add `are_all_same` to `meta`:**

```cpp
template <class... Ts> struct are_all_same : std::false_type {};
template <class T> struct are_all_same<T> : std::true_type {};
template <class T, class... Rest> struct are_all_same<T, T, Rest...> : are_all_same<T, Rest...> {};
```

**Step 3 — Modify `selected_branch_node` to compute `output_type` conditionally:**

```cpp
template <class... Cases>
struct selected_branch_node {
    using output_types = meta::type_list<typename branch_case_output<Cases>::output_type...>;
    
    // If all outputs are the same type, use it directly (homogeneous, backward compatible).
    // Otherwise, wrap in std::variant (heterogeneous).
    using output_type = std::conditional_t<
        meta::are_all_same<typename branch_case_output<Cases>::output_type...>::value,
        meta::front_t<output_types>,
        meta::to_variant_t<output_types>
    >;
    // ...
};
```

**Step 4 — Remove the homogeneous-only static_assert in `branch_output_validation_impl`:**

Replace the all-same check with a check that the requested output type matches the computed `output_type` (which may be a variant):

```cpp
template <class Output, class... Cases>
struct branch_output_validation_impl<branch_outputs<Cases...>, Output, true> {
    using computed_output = std::conditional_t<
        meta::are_all_same<typename branch_case_output<Cases>::output_type...>::value,
        meta::front_t<meta::type_list<typename branch_case_output<Cases>::output_type...>>,
        meta::to_variant_t<meta::type_list<typename branch_case_output<Cases>::output_type...>>
    >;
    static_assert(std::same_as<Output, computed_output>,
        "Branch output type mismatch: requested output must match the computed branch output type "
        "(single type for homogeneous branches, std::variant<...> for heterogeneous branches)");
    // ...
};
```

**Step 5 — The join stage naturally receives `std::variant<...>` as input:**

The existing `Connectable` constraint and `append_join` machinery already handle arbitrary input types — the join stage simply declares `input_type = std::variant<ProcessedInvoice, ProcessedReport, ProcessedMemo>` and the framework validates that this matches the branch node's `output_type`.

**Step 6 — Runtime execution path:**

The branch node now returns `result<std::variant<...>>`. The sequential runtime's `invoke_stage` already passes the output of one stage as input to the next — the join stage receives the variant and calls `std::visit` internally. No changes to the runtime engine are needed beyond the type-level adjustment.

### DSL syntax remains unchanged:

```cpp
// Homogeneous (still works as before):
using Pipe = pb::from<Document>
    ::branch<InvoiceCase, ReportCase, MemoCase>
    ::join<Finalize>     // Finalize::input_type = ProcessedDoc (single type)
    ::to<int>;

// Heterogeneous (new capability):
using Pipe = pb::from<Document>
    ::branch<InvoiceCase, ReportCase, MemoCase>
    ::join<Finalize>     // Finalize::input_type = std::variant<Invoice, Report, Memo>
    ::to<int>;
```

---

## Uncertainties and Gaps

1. **Move-only branch inputs (S2):** The current implementation requires copy-constructible input because both the predicate and selected case inspect the same input. For heterogeneous branches, this remains a limitation. The resolution path is to move the input into the selected branch case (after predicate evaluation determines the case) rather than copying. This is orthogonal to the heterogeneous output question but interacts with it if branch cases need to consume the input by move.

2. **Parallel/async backends:** The variant-based join approach assumes sequential execution (branch → variant → join). For parallel backends, a different join mechanism may be needed (e.g., `when_all` that collects results from concurrent branches). The P2300 `when_all` + `let_value` pattern provides the precedent for this.

3. **Branch exhaustiveness at compile time:** The current implementation does not check that predicates cover all possible inputs. For heterogeneous branches, this becomes more important because the variant alternatives represent all possible output paths. An exhaustiveness lint (e.g., "did you forget a case for `Cancelled`?") would be valuable.

4. **Variant size and ABI impact:** `std::variant<A, B, C>` is `max(sizeof(A), sizeof(B), sizeof(C)) + alignment_overhead` bytes. For large output types, this could be significant. Alternative: type-erased join via `std::any` or a custom type-erased wrapper — but this sacrifices type safety, which is `pb::`'s core value.

5. **`std::visit` compile-time cost:** Each `std::visit` instantiation generates a vtable-like dispatch. For pipelines with many heterogeneous branches, compile times could increase. Mitigation: explicit template instantiation for the visitor, or the `pb::join_handler<...>` pattern (Pattern B above) that generates the visit at library level.

---

## Summary

The path to heterogeneous branch/join in `pb::` is straightforward and well-supported by C++20/23:

1. **`std::variant` + `std::visit` with the overload pattern** is the canonical C++ mechanism for type-safe heterogeneous dispatch. P2300's `let_value` validates this approach at the standardization level.

2. **The `pb::` codebase already has the necessary infrastructure:** `meta::type_list`, `branch_node::output_types`, and the runtime's stage-to-stage value passing. The only missing piece is the variant construction from the output type list.

3. **The DSL syntax does not need to change.** The branch node's `output_type` becomes `std::variant<...>` when outputs are heterogeneous, and the join stage receives that variant naturally.

4. **Backward compatibility is preserved** via `are_all_same` detection: homogeneous branches continue to use single-type output and avoid variant overhead.

5. **The Taskflow comparison** confirms that index-based dispatch (already implemented in `pb::`) combined with variant-based fan-in (proposed here) is a pragmatic architecture used by production pipeline systems, even if Taskflow itself uses type-erased shared state instead of variants.
