# Code Review Bug Fixes

## TL;DR

> **Quick Summary**: Fix 15 bugs identified by 5 oracle code review agents across the entire `src/` directory, prioritized CRITICAL → HIGH → MEDIUM → LOW. Includes GLSL correctness bugs, infinite loops, NaN-producing easing functions, per-frame allocation waste, re-entrancy risks, shutdown ordering, and type standard violations.
> 
> **Deliverables**:
> - Fixed `mainmenu_bg.glsl` — array bounds check + uninitialized variables
> - Fixed `MenuHelper.cpp` + `MainMenuScreen.cpp` — infinite loop on all-locked menus
> - Fixed `Easing.hpp` — easeOutExpo NaN bug (`-2.0f` → `2.0f`)
> - Added `MainMenuScreen::onExit()` — shader handle cleanup
> - Added `string_view` overload to `Renderer::drawText/drawTextCentered`
> - Fixed `PausePopupScreen` — queued pop instead of re-entrant pop
> - Fixed `LoadingScreen` — transition guard prevents double-call
> - Fixed `App.hpp` — `int run()` → `i32 run()`
> - Fixed `JsonUtils` — try-catch on malformed JSON
> - Fixed `FontManager` — `LoadFontEx` validation + shutdown ordering
> - Fixed `LoadingTask` — empty queue guard
> - Fixed `CMakeLists.txt` — cross-platform shader null device
> 
> **Estimated Effort**: Medium
> **Parallel Execution**: YES — 3 waves
> **Critical Path**: Wave 1 → Wave 2 → Wave 3 → F1-F4

---

## Context

### Original Request
"now lets do an code review and making our code much better for all of them inside of SRC. find bugs, performances, issue, and like literally everything"

### Interview Summary
**Key Discussions**:
- User requested comprehensive code review of entire `src/` directory
- 5 oracle agents reviewed all files and found 35 issues total
- Deduplicated to 16 unique bugs (now 15 after dropping dead code without locations)
- Priority order: CRITICAL → HIGH → MEDIUM → LOW

**Research Findings**:
- **Bug #2 (infinite loop)** exists in TWO locations: `MainMenuScreen.cpp:278-280` AND `MenuHelper.cpp:89-91`
- **Bug #16 (easeOutExpo)** is misclassified as "simplify pow" — it's actually a NaN-correctness bug: `std::pow(-2.0f, -10.0f * t)` produces NaN for non-integer exponents
- **Bug #6 (PausePopupScreen re-entrancy)** is defensive programming in current architecture — `pop()` starts a transition, doesn't immediately destroy
- **Bug #4 (stale shader handle)** is defensive-only — `ShaderManager::shutdown()` runs after screens are destroyed in current architecture

### Metis Review
**Identified Gaps** (all addressed):
- MenuHelper has SAME infinite loop — added to Bug #2 scope
- easeOutExpo NaN is CRITICAL, not LOW — reclassified
- LoadingScreen `transitionToNext()` called from BOTH `onUpdate()` AND `onInput()` — guard must cover both
- JsonUtils `loadFromFile()` line 11 (`file >> data`) can also throw — needs try-catch
- FontManager needs `shutdown()` method with guard flag, called before `CloseWindow()`
- Use `IsFontReady()` (Raylib 5.5 API), not `IsFontValid()`
- Bug #14 (ScreenBlurEffect) — needs verification if already fixed from previous plan

---

## Work Objectives

### Core Objective
Fix all identified code review bugs across `src/` — correctness bugs, performance waste, type violations, and shutdown ordering issues.

### Concrete Deliverables
- `assets/shaders/mainmenu_bg.glsl` — bounds check + init vars
- `src/UI/screens/MainMenuScreen.hpp/.cpp` — onExit() + infinite loop fix
- `src/Utils/ui/MenuHelper.cpp` — infinite loop fix
- `src/AnimationController/animation/Easing.hpp` — NaN fix
- `src/Utils/render/Render.hpp/.cpp` — string_view overloads
- `src/UI/screens/PausePopupScreen.cpp` — queued pop
- `src/UI/screens/LoadingScreen.hpp/.cpp` — transition guard
- `src/Core/App.hpp` — int→i32
- `src/Core/App.cpp` — FontManager shutdown ordering
- `src/Utils/json/JsonUtils.cpp` — try-catch
- `src/Utils/font/FontUtils.hpp/.cpp` — LoadFontEx validation + shutdown method
- `src/Core/LoadingTask.hpp` — empty queue guard
- `src/CMakeLists.txt` — cross-platform null device

### Definition of Done
- [ ] `cmake --build build --config Release --target BiofuelGame` succeeds with 0 errors, 0 warnings
- [ ] Game launches and reaches MainMenuScreen
- [ ] Main menu background shader renders without visual artifacts
- [ ] Pressing Arrow keys on menu with locked items doesn't infinite loop
- [ ] `easeOutExpo(0.5f)` returns ~0.5 (not NaN)
- [ ] No per-frame `std::string` allocations from `drawText` with literals

### Must Have
- All 15 bugs fixed with correct behavior
- Game builds and runs without crashes
- Shutdown is crash-free (no UnloadFont after CloseWindow)

### Must NOT Have (Guardrails)
- NO refactoring of Renderer call sites when adding string_view overloads (overloads are additive)
- NO rewriting or "simplifying" easing functions beyond the easeOutExpo NaN fix
- NO adding FontManager into a full resource management system (just validation + shutdown ordering)
- NO modifying GLSL visual output for normal (non-edge-case) values
- NO changing ScreenManager pop() architecture — just queue the pop from PausePopupScreen
- NO including Bug #15 (dead code) without specific file:line locations
- NO adding new features — only bug fixes

---

## Verification Strategy (MANDATORY)

> **ZERO HUMAN INTERVENTION** — ALL verification is agent-executed. No exceptions.

### Test Decision
- **Infrastructure exists**: NO
- **Automated tests**: None
- **Framework**: none
- **Agent-Executed QA**: ALWAYS (mandatory for all tasks)

### QA Policy
Every task MUST include agent-executed QA scenarios.
Evidence saved to `.sisyphus/evidence/task-{N}-{scenario-slug}.{ext}`.

- **Build verification**: `cmake --build build --config Release --target BiofuelGame`
- **Code verification**: PowerShell grep for specific patterns
- **Shader verification**: Game launches, main menu background renders correctly

---

## Execution Strategy

### Parallel Execution Waves

```
Wave 1 (Start Immediately — quick correctness fixes, all independent):
├── Task 1: Fix GLSL bugs (OOB array + uninitialized vars) [quick]
├── Task 2: Fix infinite loop (MainMenuScreen + MenuHelper) [quick]
├── Task 3: Fix Easing easeOutExpo NaN bug [quick]
├── Task 4: Fix App.hpp int→i32 [quick]
├── Task 5: Fix LoadingTask OOB guard [quick]
└── Task 6: Fix LoadingScreen transition guard [quick]

Wave 2 (After Wave 1 — slightly more involved fixes):
├── Task 7: Add MainMenuScreen::onExit() for shader cleanup [quick]
├── Task 8: Add string_view overloads to Renderer [unspecified-high]
├── Task 9: Fix PausePopupScreen re-entrancy (queued pop) [quick]
├── Task 10: Fix JsonUtils exception handling [quick]
└── Task 11: Fix FontManager validation + shutdown ordering [unspecified-high]

Wave 3 (After Wave 2 — build system + verification):
├── Task 12: Fix CMake NUL device (cross-platform) [quick]
└── Task 13: Verify ScreenBlurEffect module refs (previous plan) [quick]

Wave FINAL (After ALL tasks — 4 parallel reviews):
├── Task F1: Plan compliance audit (oracle)
├── Task F2: Code quality review (unspecified-high)
├── Task F3: Real manual QA (unspecified-high + playwright)
└── Task F4: Scope fidelity check (deep)
→ Present results → Get explicit user okay

Critical Path: Wave 1 (all) → Wave 2 (T8 string_view, T11 FontManager) → Wave 3 → FINAL
Parallel Speedup: ~60% faster than sequential
Max Concurrent: 6 (Wave 1)
```

### Dependency Matrix

| Task | Depends On | Blocks | Wave |
|------|-----------|--------|------|
| 1 | - | F1-F4 | 1 |
| 2 | - | F1-F4 | 1 |
| 3 | - | F1-F4 | 1 |
| 4 | - | F1-F4 | 1 |
| 5 | - | F1-F4 | 1 |
| 6 | - | F1-F4 | 1 |
| 7 | - | F1-F4 | 2 |
| 8 | - | F1-F4 | 2 |
| 9 | - | F1-F4 | 2 |
| 10 | - | F1-F4 | 2 |
| 11 | - | F1-F4 | 2 |
| 12 | - | F1-F4 | 3 |
| 13 | - | F1-F4 | 3 |
| F1 | 1-13 | - | FINAL |
| F2 | 1-13 | - | FINAL |
| F3 | 1-13 | - | FINAL |
| F4 | 1-13 | - | FINAL |

### Agent Dispatch Summary

- **Wave 1**: **6** — T1-T6 → `quick`
- **Wave 2**: **5** — T7 → `quick`, T8 → `unspecified-high`, T9 → `quick`, T10 → `quick`, T11 → `unspecified-high`
- **Wave 3**: **2** — T12-T13 → `quick`
- **FINAL**: **4** — F1 → `oracle`, F2 → `unspecified-high`, F3 → `unspecified-high`, F4 → `deep`

---

## TODOs

- [x] 1. Fix GLSL bugs (OOB array + uninitialized variables)

  **What to do**:
  - In `assets/shaders/mainmenu_bg.glsl` line 77: Change `vec3 lightAcc;` to `vec3 lightAcc = vec3(0.0);` to initialize the variable
  - In `assets/shaders/mainmenu_bg.glsl` line 125: Change `int(rand)` to `int(clamp(rand, 0.0, 6.0))` to prevent array out-of-bounds access
  - Review the shader for any other uninitialized variables or unchecked array accesses
  - Build and verify shader compiles without warnings and renders correctly

  **Must NOT do**:
  - Do NOT change the shader's visual output for normal (non-edge-case) values
  - Do NOT restructure the shader or refactor unrelated code
  - Do NOT change array size or color values

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Two targeted fixes in a single GLSL file
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1
  - **Blocks**: F1-F4
  - **Blocked By**: None

  **References**:

  **Pattern References** (existing code to follow):
  - `assets/shaders/mainmenu_bg.glsl:52-60` — `colArr[7]` array definition (7 elements, index 0-6)
  - `assets/shaders/mainmenu_bg.glsl:77` — `vec3 lightAcc;` uninitialized variable
  - `assets/shaders/mainmenu_bg.glsl:125` — `colArr[int(rand)]` unchecked array index

  **WHY Each Reference Matters**:
  - Line 52-60: Array size is 7, valid indices 0-6. `int(rand)` can produce -1 (if rand < 0) or > 6 (if rand > 7), causing OOB read. `clamp` bounds it to valid range.
  - Line 77: Uninitialized `lightAcc` can contain garbage values on some GPU drivers, causing visual artifacts or black pixels.
  - Line 125: The unchecked `int(rand)` is the direct site of the OOB access.

  **Acceptance Criteria**:

  - [ ] `Select-String -Path "assets\shaders\mainmenu_bg.glsl" -Pattern "lightAcc = vec3\(0"` returns 1 match
  - [ ] `Select-String -Path "assets\shaders\mainmenu_bg.glsl" -Pattern "clamp\(rand, 0\.0, 6\.0\)" returns 1 match
  - [ ] Build succeeds: `cmake --build build --config Release --target BiofuelGame`

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: GLSL fixes compile and render correctly
    Tool: Bash (cmake)
    Preconditions: Shader source file has been modified
    Steps:
      1. Run `cmake --build build --config Release --target BiofuelGame`
      2. Verify exit code is 0
      3. Launch game executable
      4. Wait 2 seconds for main menu to render
      5. Verify shader background renders (not black, not artifacted)
    Expected Result: Build succeeds, shader renders correctly with visible voxel blocks
    Failure Indicators: Build fails with GLSL compilation error, or background is black/artifacted
    Evidence: .sisyphus/evidence/task-1-glsl-fix.txt

  Scenario: Array bounds check prevents OOB
    Tool: Bash (grep)
    Preconditions: Shader file has been modified
    Steps:
      1. `Select-String -Path "assets\shaders\mainmenu_bg.glsl" -Pattern "int\(rand\)" -Exclude "clamp"` — expect 0 matches (all int(rand) are wrapped in clamp)
      2. `Select-String -Path "assets\shaders\mainmenu_bg.glsl" -Pattern "clamp\(rand"` — expect 1+ match
    Expected Result: All array accesses use clamped index, no bare int(rand)
    Failure Indicators: Any bare int(rand) remaining
    Evidence: .sisyphus/evidence/task-1-bounds-check.txt
  ```

  **Commit**: YES (groups with Wave 1)
  - Message: `fix(glsl): add bounds check and initialize lightAcc in mainmenu_bg shader`
  - Files: `assets/shaders/mainmenu_bg.glsl`
  - Pre-commit: `cmake --build build --config Release --target BiofuelGame`

- [x] 2. Fix infinite loop in menu navigation (MainMenuScreen + MenuHelper)

  **What to do**:
  - In `src/UI/screens/MainMenuScreen.cpp` line 278-280: Add a cycle counter to the `do { ... } while (s_items[m_selected].locked)` loop that breaks after `s_items.size()` iterations:
    ```cpp
    i32 cycleCount = 0;
    do {
        m_selected = (m_selected + dir + itemCount) % itemCount;
        ++cycleCount;
    } while (s_items[m_selected].locked && cycleCount < itemCount);
    ```
  - In `src/Utils/ui/MenuHelper.cpp` line 89-91: Add the SAME cycle counter to the `do { ... } while (items[selectedIndex].locked)` loop:
    ```cpp
    i32 cycleCount = 0;
    do {
        selectedIndex = (selectedIndex + dir + itemCount) % itemCount;
        ++cycleCount;
    } while (items[selectedIndex].locked && cycleCount < itemCount);
    ```
  - The cycleCount guard ensures that if ALL items are locked, the loop exits after trying each item once, rather than looping forever

  **Must NOT do**:
  - Do NOT change the menu navigation behavior for normal (non-all-locked) cases
  - Do NOT add logging or error messages to the navigation function
  - Do NOT refactor the menu system or change the do-while pattern

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Two targeted additions of cycle counters in identical patterns
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1
  - **Blocks**: F1-F4
  - **Blocked By**: None

  **References**:

  **Pattern References** (existing code to follow):
  - `src/UI/screens/MainMenuScreen.cpp:278-280` — `do { m_selected = ... } while (s_items[m_selected].locked)` — the infinite loop site
  - `src/Utils/ui/MenuHelper.cpp:89-91` — `do { selectedIndex = ... } while (items[selectedIndex].locked)` — SAME infinite loop pattern

  **WHY Each Reference Matters**:
  - Both files have the identical bug. Fixing only MainMenuScreen leaves PausePopupScreen (which uses MenuHelper) with the same crash.
  - The cycle counter pattern is minimal and preserves existing behavior for normal cases.

  **Acceptance Criteria**:

  - [ ] `Select-String -Path "src\UI\screens\MainMenuScreen.cpp" -Pattern "cycleCount"` returns 1+ matches
  - [ ] `Select-String -Path "src\Utils\ui\MenuHelper.cpp" -Pattern "cycleCount"` returns 1+ matches
  - [ ] Build succeeds: `cmake --build build --config Release --target BiofuelGame`

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Both navigation loops have cycle guard
    Tool: Bash (grep)
    Preconditions: Both files modified
    Steps:
      1. `Select-String -Path "src\UI\screens\MainMenuScreen.cpp" -Pattern "cycleCount <"` — expect 1 match
      2. `Select-String -Path "src\Utils\ui\MenuHelper.cpp" -Pattern "cycleCount <"` — expect 1 match
      3. `cmake --build build --config Release --target BiofuelGame` — expect success
    Expected Result: Both loops have cycle guards, build succeeds
    Failure Indicators: Missing cycle counter in either file, build fails
    Evidence: .sisyphus/evidence/task-2-infinite-loop.txt

  Scenario: Menu navigation still works for normal (unlocked) items
    Tool: Bash
    Preconditions: Game built
    Steps:
      1. Launch game
      2. Press DOWN arrow — menu selection should move to next item
      3. Press UP arrow — menu selection should move to previous item
      4. Press ENTER — should activate selected item
    Expected Result: Menu navigation works identically to before the fix
    Failure Indicators: Menu doesn't respond to arrow keys, or selection doesn't change
    Evidence: .sisyphus/evidence/task-2-menu-nav.txt
  ```

  **Commit**: YES (groups with Wave 1)
  - Message: `fix(ui): prevent infinite loop when all menu items are locked`
  - Files: `src/UI/screens/MainMenuScreen.cpp`, `src/Utils/ui/MenuHelper.cpp`
  - Pre-commit: `cmake --build build --config Release --target BiofuelGame`

- [x] 3. Fix Easing easeOutExpo NaN bug

  **What to do**:
  - In `src/AnimationController/animation/Easing.hpp` line 81: Change `std::pow(-2.0f, -10.0f * t)` to `std::pow(2.0f, -10.0f * t)`
  - The current formula `1.0f - std::pow(-2.0f, -10.0f * t)` produces NaN for all `t ∈ (0, 1)` because raising a negative number to a fractional power is undefined in real numbers
  - The standard easeOutExpo formula uses positive base: `1.0f - std::pow(2.0f, -10.0f * t)`
  - This is a **correctness bug**, not a simplification — animations using `easeOutExpo` (e.g., slide animations in PausePopupScreen) currently produce NaN position values

  **Must NOT do**:
  - Do NOT rewrite or "simplify" any other easing functions — they are mathematically correct
  - Do NOT change the function signature, namespace, or `[[nodiscard]]`/`noexcept` attributes
  - Do NOT add comments explaining the fix beyond what's necessary

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Single-character fix (negative sign removal)
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1
  - **Blocks**: F1-F4
  - **Blocked By**: None

  **References**:

  **Pattern References**:
  - `src/AnimationController/animation/Easing.hpp:81` — `return t == 1.0f ? 1.0f : 1.0f - std::pow(-2.0f, -10.0f * t);` — the bug
  - `src/AnimationController/animation/Easing.hpp:78` — `std::pow(2.0f, 10.0f * (t - 1.0f))` — easeInExpo uses POSITIVE base correctly
  - `src/AnimationController/animation/Easing.hpp:87-88` — easeInOutExpo also uses positive `2.0f` base

  **WHY Each Reference Matters**:
  - Line 81 is the ONLY line with the negative base bug — all other easing functions use positive bases correctly
  - Lines 78 and 87-88 show the correct pattern (positive `2.0f` base) to match

  **Acceptance Criteria**:

  - [ ] `Select-String -Path "src\AnimationController\animation\Easing.hpp" -Pattern "pow\(-2"` returns 0 matches
  - [ ] `Select-String -Path "src\AnimationController\animation\Easing.hpp" -Pattern "pow\(2\.0f, -10\.0f \* t\)" returns 1 match (the corrected line)
  - [ ] Build succeeds: `cmake --build build --config Release --target BiofuelGame`

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: easeOutExpo no longer produces NaN
    Tool: Bash (grep + build)
    Preconditions: File modified
    Steps:
      1. `Select-String -Path "src\AnimationController\animation\Easing.hpp" -Pattern "pow\(-2"` — expect 0 matches
      2. `Select-String -Path "src\AnimationController\animation\Easing.hpp" -Pattern "easeOutExpo"` — expect 1 match (line 80)
      3. Verify the line reads `std::pow(2.0f, -10.0f * t)` (positive base)
      4. `cmake --build build --config Release --target BiofuelGame` — expect success
    Expected Result: Negative base removed, positive base confirmed, build succeeds
    Failure Indicators: Any `pow(-2` remains, or build fails
    Evidence: .sisyphus/evidence/task-3-easing-nan.txt
  ```

  **Commit**: YES (groups with Wave 1)
  - Message: `fix(easing): correct easeOutExpo NaN bug — change negative base to positive`
  - Files: `src/AnimationController/animation/Easing.hpp`
  - Pre-commit: `cmake --build build --config Release --target BiofuelGame`

- [x] 4. Fix App.hpp int→i32 type standard

  **What to do**:
  - In `src/Core/App.hpp` line 34: Change `[[nodiscard]] int run();` to `[[nodiscard]] i32 run();`
  - In `src/Core/App.cpp`: Change the `run()` method signature to match (`int Application::run()` → `i32 Application::run()`)
  - Verify the return statement in `run()` returns an `i32` value (likely 0 for success)
  - Build and verify

  **Must NOT do**:
  - Do NOT change any other method signatures or types
  - Do NOT change the return value or logic

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Simple type alias change in two files
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1
  - **Blocks**: F1-F4
  - **Blocked By**: None

  **References**:
  - `src/Core/App.hpp:34` — `[[nodiscard]] int run();` needs i32
  - `src/Core/App.cpp` — `int Application::run()` definition needs i32
  - `src/README.md` — Coding standard: "Always use project types from Core/Types.hpp"

  **Acceptance Criteria**:
  - [ ] `Select-String -Path "src\Core\App.hpp" -Pattern "\bint run\b"` returns 0 matches
  - [ ] Build succeeds

  **QA Scenarios**:
  ```
  Scenario: App::run uses project type i32
    Tool: Bash (grep)
    Steps:
      1. `Select-String -Path "src\Core\App.hpp" -Pattern "i32 run"` — expect 1 match
      2. `Select-String -Path "src\Core\App.cpp" -Pattern "i32 Application::run"` — expect 1 match
      3. `cmake --build build --config Release --target BiofuelGame` — expect success
    Expected Result: run() declaration and definition use i32
    Evidence: .sisyphus/evidence/task-4-type-fix.txt
  ```

  **Commit**: YES (groups with Wave 1)
  - Message: `refactor(types): change App::run() return type from int to i32`
  - Files: `src/Core/App.hpp`, `src/Core/App.cpp`
  - Pre-commit: `cmake --build build --config Release --target BiofuelGame`

- [x] 5. Fix LoadingTask OOB guard on empty queue

  **What to do**:
  - In `src/Core/LoadingTask.hpp` `processNext()` method: Add an early return if `m_tasks.empty()` at the start, BEFORE accessing `m_tasks[m_currentIndex]`
  - The current code checks `isDone()` which checks `m_currentIndex >= m_tasks.size()`, but calling `processNext()` on an empty queue where `m_currentIndex` is 0 would access `m_tasks[0]` when size is 0
  - Add: `if (m_tasks.empty()) { return; }` at the very start of `processNext()`, before the `isDone()` check

  **Must NOT do**:
  - Do NOT change the `isDone()` logic
  - Do NOT refactor the LoadingTaskQueue class structure
  - Do NOT add logging or error handling

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Single guard line addition
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1
  - **Blocks**: F1-F4
  - **Blocked By**: None

  **References**:
  - `src/Core/LoadingTask.hpp:30-40` — `processNext()` method with potential OOB on empty queue

  **Acceptance Criteria**:
  - [ ] `Select-String -Path "src\Core\LoadingTask.hpp" -Pattern "m_tasks\.empty\(\)"` returns 1 match
  - [ ] Build succeeds

  **QA Scenarios**:
  ```
  Scenario: Empty queue guard in processNext
    Tool: Bash (grep + build)
    Steps:
      1. `Select-String -Path "src\Core\LoadingTask.hpp" -Pattern "m_tasks.empty()"` — expect 1 match
      2. `cmake --build build --config Release --target BiofuelGame` — expect success
    Expected Result: processNext() has empty queue guard
    Evidence: .sisyphus/evidence/task-5-oob-guard.txt
  ```

  **Commit**: YES (groups with Wave 1)
  - Message: `fix(core): add empty-queue guard to LoadingTask::processNext()`
  - Files: `src/Core/LoadingTask.hpp`
  - Pre-commit: `cmake --build build --config Release --target BiofuelGame`

- [x] 6. Fix LoadingScreen transition guard (prevent double-call)

  **What to do**:
  - In `src/UI/screens/LoadingScreen.hpp`: Add a `bool m_transitioned = false;` member variable
  - In `src/UI/screens/LoadingScreen.cpp`: In the `onUpdate()` method where `transitionToNext()` is called, wrap it in a guard:
    ```cpp
    if (m_tasksDone && m_displayProgress >= 1.0f && m_elapsed >= MIN_DISPLAY_SECONDS && !m_transitioned) {
        m_transitioned = true;
        transitionToNext();
    }
    ```
  - Also in `onInput()` if it calls `transitionToNext()` for key-skip: add the same `!m_transitioned` guard so the transition only fires once regardless of which call site triggers it
  - Build and verify

  **Must NOT do**:
  - Do NOT change the transition logic itself
  - Do NOT remove the existing conditions, just add the guard
  - Do NOT reset `m_transitioned` (it's a one-shot flag during the screen's lifetime)

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: One bool member + two guard conditions
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1
  - **Blocks**: F1-F4
  - **Blocked By**: None

  **References**:
  - `src/UI/screens/LoadingScreen.hpp` — Add `bool m_transitioned = false;` member
  - `src/UI/screens/LoadingScreen.cpp:92-95` — `transitionToNext()` called every frame without guard
  - `src/UI/screens/LoadingScreen.cpp` — `onInput()` may also call `transitionToNext()`

  **Acceptance Criteria**:
  - [ ] `Select-String -Path "src\UI\screens\LoadingScreen.hpp" -Pattern "m_transitioned"` returns 1 match
  - [ ] `Select-String -Path "src\UI\screens\LoadingScreen.cpp" -Pattern "m_transitioned"` returns 2+ matches (both guard sites)
  - [ ] Build succeeds

  **QA Scenarios**:
  ```
  Scenario: Transition fires only once
    Tool: Bash (grep + build)
    Steps:
      1. `Select-String -Path "src\UI\screens\LoadingScreen.hpp" -Pattern "m_transitioned"` — expect 1 match
      2. `Select-String -Path "src\UI\screens\LoadingScreen.cpp" -Pattern "m_transitioned"` — expect 2+ matches
      3. `cmake --build build --config Release --target BiofuelGame` — expect success
    Expected Result: Transition guard in both header and implementation
    Evidence: .sisyphus/evidence/task-6-transition-guard.txt
  ```

  **Commit**: YES (groups with Wave 1)
  - Message: `fix(ui): prevent LoadingScreen transitionToNext double-call with guard flag`
  - Files: `src/UI/screens/LoadingScreen.hpp`, `src/UI/screens/LoadingScreen.cpp`
  - Pre-commit: `cmake --build build --config Release --target BiofuelGame`

- [x] 7. Add MainMenuScreen::onExit() for shader cleanup

  **What to do**:
  - In `src/UI/screens/MainMenuScreen.hpp`: Add `void onExit() override;` declaration in the public section
  - In `src/UI/screens/MainMenuScreen.cpp`: Implement `onExit()` that:
    1. Sets `m_bgShaderReady = false;`
    2. Resets `m_bgResLoc = -1;` and `m_bgTimeLoc = -1;`
    3. Does NOT call `UnloadShader()` — ShaderManager owns the shader lifecycle
  - This is defensive programming: when MainMenuScreen exits (e.g., during gameplay screen transition), it releases its references. The actual shader is managed by ShaderManager.
  - Build and verify

  **Must NOT do**:
  - Do NOT call `UnloadShader()` — ShaderManager owns all shader lifecycles
  - Do NOT add any other resource cleanup beyond resetting handle fields
  - Do NOT reset `m_bgTime` or `m_selected` (those are re-set in `onEnter()` anyway)

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Small method addition with 3 assignment lines
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 2
  - **Blocks**: F1-F4
  - **Blocked By**: None

  **References**:
  - `src/UI/screens/MainMenuScreen.hpp:18-21` — Existing lifecycle overrides (onEnter, onUpdate, onRender, onInput) — add onExit here
  - `src/UI/screens/MainMenuScreen.hpp:78-81` — `m_bgShader`, `m_bgResLoc`, `m_bgTimeLoc`, `m_bgShaderReady` — the fields to reset
  - `src/UI/Screen.hpp` — Base class with virtual `onExit()` (empty default)

  **Acceptance Criteria**:
  - [ ] `Select-String -Path "src\UI\screens\MainMenuScreen.hpp" -Pattern "onExit"` returns 1 match
  - [ ] `Select-String -Path "src\UI\screens\MainMenuScreen.cpp" -Pattern "onExit"` returns 1+ matches
  - [ ] Build succeeds

  **QA Scenarios**:
  ```
  Scenario: onExit() resets shader handle fields
    Tool: Bash (grep + build)
    Steps:
      1. `Select-String -Path "src\UI\screens\MainMenuScreen.hpp" -Pattern "void onExit"` — expect 1 match
      2. `Select-String -Path "src\UI\screens\MainMenuScreen.cpp" -Pattern "m_bgShaderReady = false"` — expect 1 match (in onExit)
      3. `cmake --build build --config Release --target BiofuelGame` — expect success
    Expected Result: onExit() declared and implemented, resets shader handle
    Evidence: .sisyphus/evidence/task-7-onexit.txt
  ```

  **Commit**: YES (groups with Wave 2)
  - Message: `fix(ui): add MainMenuScreen::onExit() to reset shader handle on exit`
  - Files: `src/UI/screens/MainMenuScreen.hpp`, `src/UI/screens/MainMenuScreen.cpp`
  - Pre-commit: `cmake --build build --config Release --target BiofuelGame`

- [x] 8. Add string_view overloads to Renderer

  **What to do**:
  - In `src/Utils/render/Render.hpp`: Add `std::string_view` overloads for `drawText` and `drawTextCentered`:
    ```cpp
    static void drawText(std::string_view text, i32 x, i32 y, i32 fontSize, Color color);
    static void drawTextCentered(std::string_view text, i32 centerX, i32 y, i32 fontSize, Color color);
    ```
  - In `src/Utils/render/Render.cpp`: Implement the `string_view` overloads by forwarding to `DrawText(text.data(), x, y, fontSize, color)` — no `std::string` construction needed
  - Keep the existing `const std::string&` overloads (they remain for backward compatibility)
  - The `string_view` overloads avoid heap allocation when called with string literals or `std::string_view` constants
  - Build and verify

  **Must NOT do**:
  - Do NOT remove the existing `const std::string&` overloads — backward compatibility
  - Do NOT refactor call sites across the codebase to use string_view — this is additive only
  - Do NOT add any other Renderer methods

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: Adding overloads requires careful include management and ensuring both signatures work correctly together
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 2
  - **Blocks**: F1-F4
  - **Blocked By**: None

  **References**:
  - `src/Utils/render/Render.hpp:19` — `static void drawText(const std::string& text, i32 x, i32 y, i32 fontSize, Color color);` — add string_view overload
  - `src/Utils/render/Render.hpp:25` (or similar) — `drawTextCentered` — add string_view overload
  - `src/Utils/render/Render.cpp:14` — Current implementation forwards to `::DrawText(text.c_str(), ...)`
  - `src/README.md` — Coding standard: "std::string_view for String Literals"

  **Acceptance Criteria**:
  - [ ] `Select-String -Path "src\Utils\render\Render.hpp" -Pattern "string_view"` returns 2+ matches (drawText and drawTextCentered)
  - [ ] `Select-String -Path "src\Utils\render\Render.hpp" -Pattern "const std::string&"` returns 2 matches (overloads preserved)
  - [ ] Build succeeds
  - [ ] Existing call sites compile without changes

  **QA Scenarios**:
  ```
  Scenario: string_view overloads exist alongside string overloads
    Tool: Bash (grep + build)
    Steps:
      1. `Select-String -Path "src\Utils\render\Render.hpp" -Pattern "string_view text"` — expect 2 matches
      2. `Select-String -Path "src\Utils\render\Render.hpp" -Pattern "const std::string& text"` — expect 2 matches (preserved)
      3. `cmake --build build --config Release --target BiofuelGame` — expect success (no existing call sites broken)
    Expected Result: Both overload sets exist, all call sites compile
    Evidence: .sisyphus/evidence/task-8-stringview.txt
  ```

  **Commit**: YES (groups with Wave 2)
  - Message: `perf(render): add string_view overloads for drawText and drawTextCentered`
  - Files: `src/Utils/render/Render.hpp`, `src/Utils/render/Render.cpp`
  - Pre-commit: `cmake --build build --config Release --target BiofuelGame`

- [x] 9. Fix PausePopupScreen re-entrancy (queued pop)

  **What to do**:
  - In `src/UI/screens/PausePopupScreen.cpp` lines 228-236: Instead of directly calling `sm->pop()` inside the animation `onComplete` callback, queue the pop for the next frame:
    ```cpp
    slideAnim->onComplete([this](animation::Animation<f32>*) {
        if (auto* sm = manager()) {
            // Queue pop instead of immediate call to avoid re-entrancy
            sm->queuePop();
            if (m_quitting) {
                sm->requestQuit();
            }
        }
    });
    ```
  - If `ScreenManager` doesn't have a `queuePop()` method, add it. It should store the pop request and execute it in the next `update()` call, after the animation callback chain has fully resolved.
  - Alternatively (simpler approach): Set a `m_wantsPop = true` flag in the callback, then in `onUpdate()` check the flag and call `sm->pop()` there. This avoids re-entrancy because `onUpdate()` is called from the main loop, not from inside an animation callback.

  **Must NOT do**:
  - Do NOT change the animation system architecture
  - Do NOT remove the animation callback — just defer the pop
  - Do NOT add complex state machine for pop queuing

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Either a single method rename or a flag + conditional pop
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 2
  - **Blocks**: F1-F4
  - **Blocked By**: None

  **References**:
  - `src/UI/screens/PausePopupScreen.cpp:228-236` — `onComplete` callback that calls `sm->pop()` synchronously
  - `src/UI/ScreenManager.hpp` — Check if `queuePop()` method exists; if not, the simpler flag approach is preferred
  - `src/UI/Screen.hpp` — Base class with `m_manager` pointer for accessing ScreenManager

  **Acceptance Criteria**:
  - [ ] `Select-String -Path "src\UI\screens\PausePopupScreen.cpp" -Pattern "sm->pop\(\)"` returns 0 matches (only in onComplete callback)
  - [ ] Build succeeds
  - [ ] Game launches, ESC opens pause popup, ESC closes it without crash

  **QA Scenarios**:
  ```
  Scenario: Pop is deferred, not called during callback
    Tool: Bash (grep + build + run)
    Steps:
      1. `Select-String -Path "src\UI\screens\PausePopupScreen.cpp" -Pattern "->pop()"` — expect 0 matches inside callbacks (or 1 match from deferred approach)
      2. `cmake --build build --config Release --target BiofuelGame` — expect success
      3. Launch game, press ESC to open pause popup, press ESC again to close
      4. Verify no crash or undefined behavior
    Expected Result: Pop is deferred, no re-entrancy, game works correctly
    Failure Indicators: Crash on popup close, double-free, or assertion failure
    Evidence: .sisyphus/evidence/task-9-queued-pop.txt
  ```

  **Commit**: YES (groups with Wave 2)
  - Message: `fix(ui): defer PausePopupScreen pop() to avoid re-entrancy in animation callback`
  - Files: `src/UI/screens/PausePopupScreen.cpp`, `src/UI/screens/PausePopupScreen.hpp` (if adding flag)
  - Pre-commit: `cmake --build build --config Release --target BiofuelGame`

- [x] 10. Fix JsonUtils exception handling

  **What to do**:
  - In `src/Utils/json/JsonUtils.cpp`: Wrap the `file >> data` operation (line 11) in a try-catch block:
    ```cpp
    Json JsonUtils::loadFromFile(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            return Json{};
        }
        Json data;
        try {
            file >> data;
        } catch (const nlohmann::json::parse_error&) {
            return Json{};
        }
        return data;
    }
    ```
  - This ensures that even if the file contains malformed JSON, the function returns an empty object instead of crashing
  - The `parseString()` method already passes `false` for `allow_exceptions` (line 23), so it's safe, but add a comment documenting that it returns a discarded value on failure

  **Must NOT do**:
  - Do NOT change the function signatures or return types
  - Do NOT add logging (spdlog or otherwise) — keep it simple
  - Do NOT change `parseString()` behavior — it already handles exceptions via `allow_exceptions=false`

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Adding try-catch to one function, adding comment to another
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 2
  - **Blocks**: F1-F4
  - **Blocked By**: None

  **References**:
  - `src/Utils/json/JsonUtils.cpp:5-13` — `loadFromFile()` with unhandled `file >> data` parse error
  - `src/Utils/json/JsonUtils.cpp:22-24` — `parseString()` already safe (allow_exceptions=false)
  - `src/Utils/json/README.md` — Documents: "loadFromFile() never throws"

  **Acceptance Criteria**:
  - [ ] `Select-String -Path "src\Utils\json\JsonUtils.cpp" -Pattern "parse_error"` returns 1 match
  - [ ] Build succeeds

  **QA Scenarios**:
  ```
  Scenario: loadFromFile catches parse errors
    Tool: Bash (grep + build)
    Steps:
      1. `Select-String -Path "src\Utils\json\JsonUtils.cpp" -Pattern "catch.*parse_error"` — expect 1 match
      2. `cmake --build build --config Release --target BiofuelGame` — expect success
    Expected Result: loadFromFile wraps parsing in try-catch
    Evidence: .sisyphus/evidence/task-10-jsonutils.txt
  ```

  **Commit**: YES (groups with Wave 2)
  - Message: `fix(json): add try-catch for parse_error in JsonUtils::loadFromFile`
  - Files: `src/Utils/json/JsonUtils.cpp`
  - Pre-commit: `cmake --build build --config Release --target BiofuelGame`

- [x] 11. Fix FontManager validation + shutdown ordering

  **What to do**:
  - In `src/Utils/font/FontUtils.hpp`: Add a public `shutdown()` method to `FontManager`:
    ```cpp
    void shutdown(); // Unloads all fonts and marks manager as shut down
    ```
  - Add a private `bool m_shutDown = false;` member flag
  - In `src/Utils/font/FontUtils.cpp`:
    1. Implement `shutdown()`: calls `unloadAll()`, then sets `m_shutDown = true`
    2. In the destructor `~FontManager()`: only call `unloadAll()` if `!m_shutDown` (guard against double-unload)
    3. In `load()`: after `LoadFontEx()`, add validation check using Raylib's `IsFontReady()`:
       ```cpp
       Font font = LoadFontEx(path.c_str(), baseSize, nullptr, 0);
       if (!IsFontReady(font)) {
           spdlog::warn("FontManager: Failed to load font '{}' from '{}'", name, path);
           return;
       }
       ```
  - In `src/Core/App.cpp`: In `shutdown()`, add `Data::fonts().shutdown()` call **BEFORE** `CloseWindow()` (or in the appropriate shutdown sequence position, after `Data::screens().shutdown()` and before Raylib cleanup)
  - Build and verify

  **Must NOT do**:
  - Do NOT turn FontManager into a full resource management system
  - Do NOT add font caching or reference counting
  - Do NOT change the `load()` method signature
  - Do NOT call `shutdown()` from the destructor — only from `App::shutdown()`

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: Touches FontManager (singleton), App lifecycle, and Raylib API validation — multiple concerns
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 2
  - **Blocks**: F1-F4
  - **Blocked By**: None

  **References**:
  - `src/Utils/font/FontUtils.hpp` — `FontManager` class with `instance()`, `load()`, `get()`, `has()`, `unloadAll()` methods
  - `src/Utils/font/FontUtils.cpp:5-7` — Destructor calls `unloadAll()` after Raylib shutdown
  - `src/Utils/font/FontUtils.cpp` — `load()` method calls `LoadFontEx()` without validation
  - `src/Core/App.cpp` — `shutdown()` method — add `Data::fonts().shutdown()` before `CloseWindow()`
  - Raylib API: `IsFontReady(Font)` — returns true if font loaded successfully

  **Acceptance Criteria**:
  - [ ] `Select-String -Path "src\Utils\font\FontUtils.hpp" -Pattern "shutdown"` returns 1 match
  - [ ] `Select-String -Path "src\Utils\font\FontUtils.hpp" -Pattern "m_shutDown"` returns 1 match
  - [ ] `Select-String -Path "src\Utils\font\FontUtils.cpp" -Pattern "IsFontReady"` returns 1 match
  - [ ] `Select-String -Path "src\Core\App.cpp" -Pattern "fonts\(\)\.shutdown"` returns 1 match
  - [ ] Build succeeds
  - [ ] Game shuts down without crash

  **QA Scenarios**:
  ```
  Scenario: FontManager has shutdown method and validation
    Tool: Bash (grep + build + run)
    Steps:
      1. `Select-String -Path "src\Utils\font\FontUtils.hpp" -Pattern "void shutdown"` — expect 1 match
      2. `Select-String -Path "src\Utils\font\FontUtils.cpp" -Pattern "IsFontReady"` — expect 1 match
      3. `Select-String -Path "src\Core\App.cpp" -Pattern "fonts\(\).shutdown"` — expect 1 match
      4. `cmake --build build --config Release --target BiofuelGame` — expect success
      5. Launch game, then close window — verify no crash
    Expected Result: shutdown() method exists, font validation added, shutdown ordering correct
    Failure Indicators: Build fails, crash on shutdown, font loading fails silently
    Evidence: .sisyphus/evidence/task-11-fontmanager.txt
  ```

  **Commit**: YES (groups with Wave 2)
  - Message: `fix(font): add FontManager shutdown ordering + LoadFontEx validation`
  - Files: `src/Utils/font/FontUtils.hpp`, `src/Utils/font/FontUtils.cpp`, `src/Core/App.cpp`
  - Pre-commit: `cmake --build build --config Release --target BiofuelGame`

- [x] 12. Fix CMake NUL device (cross-platform shader validation)

  **What to do**:
  - In `src/CMakeLists.txt`: Find the `glslc -o NUL` command in the shader embedding custom command
  - Replace `NUL` (Windows-only null device) with `${CMAKE_NULL_DEVICE}` which CMake provides since version 3.25, OR use a platform-conditional that outputs to the appropriate null device:
    ```cmake
    # Option A: Use CMAKE_NULL_DEVICE (CMake 3.25+)
    cmake_minimum_required(VERSION 3.25)
    # ... in custom command:
    COMMAND ${GLSLC_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_SOURCE} -o ${CMAKE_NULL_DEVICE}

    # Option B: Platform conditional (for older CMake)
    if(WIN32)
      set(NULL_DEVICE "NUL")
    else()
      set(NULL_DEVICE "/dev/null")
    endif()
    # ... in custom command:
    COMMAND ${GLSLC_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_SOURCE} -o ${NULL_DEVICE}
    ```
  - If the project's `cmake_minimum_required` is below 3.25, use Option B
  - Build and verify shader compilation still works on Windows

  **Must NOT do**:
  - Do NOT change the shader embedding pipeline logic
  - Do NOT modify `cmake/EmbedShaders.cmake`
  - Do NOT remove the `glslc` validation step entirely

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Single CMake variable replacement
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 3
  - **Blocks**: F1-F4
  - **Blocked By**: None

  **References**:
  - `src/CMakeLists.txt` — Contains `glslc -o NUL` or similar Windows-only null device reference
  - `cmake/EmbedShaders.cmake` — The shader embedding script (may reference NUL too)

  **Acceptance Criteria**:
  - [ ] `Select-String -Path "src\CMakeLists.txt" -Pattern "NUL"` returns 0 matches (after fix)
  - [ ] `Select-String -Path "src\CMakeLists.txt" -Pattern "NULL_DEVICE|CMAKE_NULL_DEVICE|/dev/null"` returns 1+ matches
  - [ ] Build succeeds on Windows: `cmake --build build --config Release --target BiofuelGame`

  **QA Scenarios**:
  ```
  Scenario: CMake uses cross-platform null device
    Tool: Bash (grep + build)
    Steps:
      1. `Select-String -Path "src\CMakeLists.txt" -Pattern "NUL"` — expect 0 matches (no Windows-only NUL)
      2. `Select-String -Path "src\CMakeLists.txt" -Pattern "NULL_DEVICE|CMAKE_NULL_DEVICE|/dev/null"` — expect 1+ matches
      3. `cmake --build build --config Release --target BiofuelGame` — expect success
    Expected Result: No Windows-only NUL device, build succeeds
    Evidence: .sisyphus/evidence/task-12-cmake-null.txt
  ```

  **Commit**: YES (groups with Wave 3)
  - Message: `fix(build): use cross-platform null device in CMake shader validation`
  - Files: `src/CMakeLists.txt`
  - Pre-commit: `cmake --build build --config Release --target BiofuelGame`

- [x] 13. Verify ScreenBlurEffect module refs (previous plan)

  **What to do**:
  - Check if `src/AnimationController/screen/ScreenBlurEffect.cpp` already uses `BlurHModule::NAME` and `BlurVModule::NAME` instead of hardcoded `"blur_h"` / `"blur_v"` strings
  - If the previous plan (`code-cleanliness-shader-reorg`) was executed and completed, these references should already be updated
  - If NOT yet updated, update them now:
    - Replace `shaderMgr.get("blur_h")` with `shaderMgr.get(BlurHModule::NAME.data())` (or equivalent)
    - Replace `shaderMgr.get("blur_v")` with `shaderMgr.get(BlurVModule::NAME.data())`
    - Replace uniform name strings with module constants if they exist
  - If already updated, just verify and mark as done

  **Must NOT do**:
  - Do NOT create new module files — those were done in the previous plan
  - Do NOT change the GLSL source content
  - Do NOT modify ShaderManager API

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Verification + possible minor string replacements
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 3
  - **Blocks**: F1-F4
  - **Blocked By**: None

  **References**:
  - `src/AnimationController/screen/ScreenBlurEffect.cpp` — File to verify/update
  - `src/Utils/render/Shader/BlurHModule.hpp` — Module with `NAME = "blur_h"` (if exists)
  - `src/Utils/render/Shader/BlurVModule.hpp` — Module with `NAME = "blur_v"` (if exists)
  - `.sisyphus/plans/code-cleanliness-shader-reorg.md` — Previous plan that addressed this

  **Acceptance Criteria**:
  - [ ] `ScreenBlurEffect.cpp` uses module references instead of hardcoded `"blur_h"` / `"blur_v"` strings, OR the module files don't exist yet (previous plan not executed)
  - [ ] Build succeeds

  **QA Scenarios**:
  ```
  Scenario: ScreenBlurEffect uses module references or previous plan not yet executed
    Tool: Bash (grep + build)
    Steps:
      1. Check if `src\Utils\render\Shader\BlurHModule.hpp` exists
      2. If modules exist: `Select-String -Path "src\AnimationController\screen\ScreenBlurEffect.cpp" -Pattern '"blur_h"|"blur_v"'` — expect 0 raw string matches
      3. If modules don't exist: Leave as-is (previous plan not yet executed)
      4. `cmake --build build --config Release --target BiofuelGame` — expect success
    Expected Result: Module references if previous plan was executed, or unchanged if not
    Evidence: .sisyphus/evidence/task-13-blurmodule.txt
  ```

  **Commit**: YES (groups with Wave 3, if changes needed)
  - Message: `refactor(shader): update ScreenBlurEffect to use module references`
  - Files: `src/AnimationController/screen/ScreenBlurEffect.cpp` (if changes needed)
  - Pre-commit: `cmake --build build --config Release --target BiofuelGame`

---

## Final Verification Wave (MANDATORY — after ALL implementation tasks)

> 4 review agents run in PARALLEL. ALL must APPROVE. Present consolidated results to user and get explicit "okay" before completing.

- [ ] F1. **Plan Compliance Audit** — `oracle`
  Read the plan end-to-end. For each "Must Have": verify fix exists (grep for corrected pattern). For each "Must NOT Have": search codebase for forbidden patterns — reject with file:line if found. Check evidence files in .sisyphus/evidence/. Compare deliverables against plan.
  Output: `Must Have [N/N] | Must NOT Have [N/N] | Tasks [N/N] | VERDICT: APPROVE/REJECT`

- [ ] F2. **Code Quality Review** — `unspecified-high`
  Run `cmake --build build --config Release`. Review all changed files for: `as any`/`@ts-ignore` equivalents, empty catches, commented-out code, unused imports, AI slop (excessive comments, over-abstraction, generic names). Check namespace consistency.
  Output: `Build [PASS/FAIL] | Files [N clean/N issues] | VERDICT`

- [ ] F3. **Real Manual QA** — `unspecified-high` (+ playwright skill)
  Launch the built executable. Verify Main Menu renders correctly (shader background visible, no artifacts). Verify menu navigation works (arrow keys, enter). Press ESC to open pause popup, ESC again to close. Verify no crash on shutdown.
  Output: `Scenarios [N/N pass] | Visual [MATCH/MISMATCH] | VERDICT`

- [ ] F4. **Scope Fidelity Check** — `deep`
  For each task: read "What to do", read actual diff. Verify 1:1 — everything in spec was fixed, nothing beyond spec was added. Check "Must NOT do" compliance. Detect cross-task contamination. Flag unaccounted changes.
  Output: `Tasks [N/N compliant] | Contamination [CLEAN/N issues] | Unaccounted [CLEAN/N files] | VERDICT`

---

## Commit Strategy

- **Wave 1**: `fix(bugs): GLSL bounds check, infinite loop guard, easing NaN, type fixes, OOB guards` — mainmenu_bg.glsl, MainMenuScreen.cpp, MenuHelper.cpp, Easing.hpp, App.hpp, LoadingTask.hpp, LoadingScreen.cpp
- **Wave 2**: `fix(bugs): shader cleanup, string_view overloads, re-entrancy, exception handling, font validation` — MainMenuScreen.hpp/.cpp, Render.hpp/.cpp, PausePopupScreen.cpp, JsonUtils.cpp, FontUtils.hpp/.cpp, App.cpp
- **Wave 3**: `fix(build): cross-platform shader null device` — CMakeLists.txt

---

## Success Criteria

### Verification Commands
```bash
cmake --build build --config Release --target BiofuelGame  # Expected: 0 errors, 0 warnings
```

### Final Checklist
- [ ] All "Must Have" present
- [ ] All "Must NOT Have" absent
- [ ] Game launches and runs correctly
- [ ] Shutdown is crash-free