# ScreenManager System

## TL;DR

> **Quick Summary**: Build a ScreenManager singleton with a Screen abstract base class (full lifecycle), a push/pop screen stack with overlay support, and fade transitions. Move Main Menu rendering from App.cpp inline code to a dedicated MainMenuScreen class. Delete the orphaned Game.hpp/cpp.
> 
> **Deliverables**:
> - `src/UI/Screen.hpp` — Abstract Screen base class with virtual lifecycle methods
> - `src/UI/ScreenManager.hpp/.cpp` — Singleton screen stack manager
> - `src/UI/screens/MainMenuScreen.hpp/.cpp` — Main menu screen (extracted from App.cpp)
> - Refactored `src/Core/App.cpp` — delegates to ScreenManager, no inline menu rendering
> - Deleted `src/Core/Game.hpp` and `src/Core/Game.cpp`
> - Updated `src/Data/Data.hpp` — adds `Data::screens()` accessor
> - Updated `src/CMakeLists.txt` — adds `UI/screens/` include path
> 
> **Estimated Effort**: Medium
> **Parallel Execution**: YES — 3 waves
> **Critical Path**: Task 1 (Screen base) → Task 3 (ScreenManager) → Task 4 (App refactor) → Task 5 (Integration) → Task 6 (Fade) → F1-F4

---

## Context

### Original Request
User wants a ScreenManager inside the UI/ folder that uses events and utils for screen rendering. Main Menu rendering should move from App.cpp inline code to a dedicated screen class inside `UI/screens/`. Screens should have standard lifecycle methods and be triggered via OnRender-style iteration at a high level. The system should use events where appropriate.

### Interview Summary
**Key Discussions**:
- **Singleton pattern**: ScreenManager follows EventManager's singleton pattern, accessible via `Data::screens()`
- **Full lifecycle**: Screens have OnEnter, OnExit, OnPause, OnResume, OnUpdate, OnRender, OnInput
- **Stack with overlay**: PauseScreen can push on top of GameScreen, bottom screen still renders
- **Fade transitions**: Fade-to-black when switching screens
- **Game.hpp deletion**: User confirmed deletion — ScreenManager replaces its purpose entirely
- **No unit tests**: Agent QA only — run game, verify behavior

**Research Findings**:
- **KatanaEngine/Aspen pattern**: Virtual LoadContent/UnloadContent, OnEnter/OnExit/OnPause/OnResume, Update(dt), Render(), HandleInput()
- **SuperTuxKart**: Menu stack with pushMenu/popMenu/replaceTopMostScreen
- **Fade transitions**: Screen manages its own TransitionState (None/In/Out) with progress 0→1, ScreenManager handles timing
- **Passthrough flags**: Screens can allow background screens to draw/update when covered

### Metis Review
**Identified Gaps** (all addressed):
- **Game.hpp overlap**: DECIDED — delete Game.hpp/cpp, ScreenManager replaces it
- **OnRender as event**: RESOLVED — screens are called directly by ScreenManager in game loop, NOT via event dispatch. Events are for decoupled communication (input, resize), not the main render/update cycle
- **FPS counter placement**: RESOLVED — stays in App::render() as debug info, not screen content
- **Input routing**: RESOLVED — only top screen receives input by default
- **Fade transition mechanism**: RESOLVED — use existing `Renderer::drawRect()` with alpha Color, no Renderer modifications needed
- **CMake include paths**: RESOLVED — must add `${CMAKE_CURRENT_SOURCE_DIR}/UI/screens`
- **Namespace**: RESOLVED — `biofuel::ui` for ScreenManager/Screen, `biofuel::ui::screens` for concrete screens
- **beginFrame/endFrame**: RESOLVED — stays in App, not in individual screens

---

## Work Objectives

### Core Objective
Build a ScreenManager system that takes over all screen rendering from App.cpp, enabling future screens (GameplayScreen, PauseScreen) to be added without touching the main loop.

### Concrete Deliverables
- `src/UI/Screen.hpp` — Abstract base class
- `src/UI/ScreenManager.hpp` / `src/UI/ScreenManager.cpp` — Singleton manager
- `src/UI/screens/MainMenuScreen.hpp` / `src/UI/screens/MainMenuScreen.cpp` — Main menu
- Refactored `src/Core/App.cpp`
- Updated `src/Data/Data.hpp`
- Updated `src/CMakeLists.txt`
- Deleted `src/Core/Game.hpp` and `src/Core/Game.cpp`

### Definition of Done
- [ ] Game launches and shows identical Main Menu (same text, same colors, same positions)
- [ ] App::render() no longer contains inline menu draw calls — only beginFrame/endFrame + ScreenManager + FPS
- [ ] Game.hpp and Game.cpp are deleted from the repository
- [ ] ScreenManager is accessible via `Data::screens()`
- [ ] ScreenManager supports push, pop, replace, and clear
- [ ] Screen base class has onEnter/onExit/onPause/onResume/onUpdate/onRender/onInput
- [ ] Fade transition works (black overlay fades in/out when switching)
- [ ] Project compiles with zero errors

### Must Have
- Screen abstract base class with full virtual lifecycle
- ScreenManager singleton with push/pop/replace/clear stack operations
- MainMenuScreen rendering identical to current App::render() output
- Fade-to-black transitions between screens
- `Data::screens()` accessor following the `Data::events()` pattern
- Only top screen receives input by default
- beginFrame/endFrame called in App, not inside screens

### Must NOT Have (Guardrails)
- ❌ NO OnRender/OnUpdate as event types — screens are called directly, not via event dispatch
- ❌ NO modifications to `Utils/render/Renderer.hpp` or `Renderer.cpp` — use existing `drawRect` with alpha for fades
- ❌ NO UI widget system (buttons, panels) — MainMenuScreen uses raw `Renderer::drawText` calls
- ❌ NO other screens beyond MainMenuScreen (GameplayScreen, PauseScreen are future work)
- ❌ NO direct `Data::screens().push()` calls from inside Screen subclasses — screens request transitions, ScreenManager enforces
- ❌ NO Game.hpp/Game.cpp left orphaned — must be deleted
- ❌ NO sound/music system integration
- ❌ NO excessive comments, AI-slop documentation, or premature abstractions

---

## Verification Strategy

> **ZERO HUMAN INTERVENTION** — ALL verification is agent-executed. No exceptions.

### Test Decision
- **Infrastructure exists**: NO
- **Automated tests**: None (agent QA only)
- **Framework**: none

### QA Policy
Every task MUST include agent-executed QA scenarios.
Evidence saved to `.sisyphus/evidence/task-{N}-{scenario-slug}.{ext}`.

- **Desktop Game**: Use Playwright (playwright skill) to launch the executable, take screenshots, verify visual output
- **Build Verification**: Use Bash (cmake) — build, check for errors/warnings
- **Code Verification**: Use Grep — verify no inline draw calls remain in App.cpp, verify file existence

---

## Execution Strategy

### Parallel Execution Waves

```
Wave 1 (Start Immediately — foundation + scaffolding):
├── Task 1: Screen abstract base class [quick]
├── Task 2: Delete Game.hpp/cpp + update CMake includes [quick]
└── Task 3: ScreenManager singleton [unspecified-high]

Wave 2 (After Wave 1 — core implementation):
├── Task 4: Refactor App.cpp to delegate to ScreenManager [deep]
├── Task 5: MainMenuScreen implementation [unspecified-high]
└── Task 6: Update Data.hpp with Data::screens() accessor [quick]

Wave 3 (After Wave 2 — polish + transitions):
└── Task 7: Fade transition system [deep]

Wave FINAL (After ALL tasks — 4 parallel reviews):
├── F1: Plan compliance audit (oracle)
├── F2: Code quality review (unspecified-high)
├── F3: Real manual QA (unspecified-high + playwright)
└── F4: Scope fidelity check (deep)
→ Present results → Get explicit user okay
```

### Dependency Matrix

| Task | Depends On | Blocks | Wave |
|------|-----------|--------|------|
| 1 | — | 3, 4, 5, 7 | 1 |
| 2 | — | 4 | 1 |
| 3 | 1 | 4, 6 | 1 |
| 4 | 1, 2, 3 | 7 | 2 |
| 5 | 1, 3 | 7 | 2 |
| 6 | 3 | 7 | 2 |
| 7 | 4, 5, 6 | F1-F4 | 3 |

### Agent Dispatch Summary

- **Wave 1**: 3 tasks — T1 → `quick`, T2 → `quick`, T3 → `unspecified-high`
- **Wave 2**: 3 tasks — T4 → `deep`, T5 → `unspecified-high`, T6 → `quick`
- **Wave 3**: 1 task — T7 → `deep`
- **FINAL**: 4 tasks — F1 → `oracle`, F2 → `unspecified-high`, F3 → `unspecified-high`, F4 → `deep`

---

## TODOs

- [x] 1. Screen Abstract Base Class

  **What to do**:
  - Create `src/UI/Screen.hpp` defining the abstract `Screen` base class
  - Full lifecycle virtual methods: `onEnter()`, `onExit()`, `onPause()`, `onResume()`
  - Core loop virtual methods: `onUpdate(float dt)` (pure virtual), `onRender()` (pure virtual), `onInput()` (virtual, empty default)
  - Transition state: `enum class TransitionState { None, TransitionIn, TransitionOut }` — member `m_transitionState`, `m_transitionProgress` (0.0–1.0), `m_transitionDuration` (configurable, default 0.5s)
  - Passthrough flags: `bool m_passthroughRender = false`, `bool m_passthroughUpdate = false`, `bool m_passthroughInput = false` — control whether background screens continue when this screen is on top
  - Helper: `float getTransitionAlpha() const` — returns alpha based on transition state (In: 0→1, Out: 1→0, None: 1.0)
  - Helper: `bool needsRemoval() const` — true when transition out is complete (progress >= 1.0)
  - Helper: `bool isTransitioning() const` — true when state is TransitionIn or TransitionOut
  - Store a `ScreenManager* m_manager` pointer, set via `setManager(ScreenManager*)` — screens can request transitions but not force them
  - Namespace: `biofuel::ui`
  - Non-copyable, non-movable
  - Virtual destructor

  **Must NOT do**:
  - Do NOT add LoadContent/UnloadContent — not needed yet, no asset system
  - Do NOT add OnRender/OnUpdate as event structs — these are direct method calls
  - Do NOT add any implementation beyond the base class definition

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Single header file, well-defined interface, no complex logic
  - **Skills**: []
  - **Skills Evaluated but Omitted**:
    - `playwright`: Not needed — no UI to verify yet

  **Parallelization**:
  - **Can Run In Parallel**: YES (with Tasks 2, 3)
  - **Parallel Group**: Wave 1
  - **Blocks**: Tasks 3, 4, 5, 7
  - **Blocked By**: None

  **References**:

  **Pattern References** (existing code to follow):
  - `src/Core/App.hpp:12-50` — Application class structure — follow same non-copyable pattern, same Config struct style, same method naming (init/shutdown vs onEnter/onExit)
  - `src/Data/event/EventManager.hpp:12-36` — Singleton pattern with `init()`/`shutdown()`/private constructor — Screen follows similar lifecycle but as abstract base, not singleton
  - `src/Core/Types.hpp:6-21` — Type aliases — use `f32` for float values (transition progress, duration)

  **API/Type References** (contracts to implement against):
  - `src/Utils/render/Render.hpp:12-25` — Renderer API — Screen::onRender() will call these methods (drawText, drawRect, beginFrame/endFrame are called by App/ScreenManager, not individual screens)
  - `src/Data/Data.hpp:19-28` — Data accessor pattern — `Data::screens()` will return ScreenManager reference, similar to `Data::events()` returning EventManager reference

  **WHY Each Reference Matters**:
  - `App.hpp`: Ensures Screen follows the same code style and design patterns as the rest of the codebase
  - `EventManager.hpp`: The singleton pattern is the blueprint for ScreenManager — Screen class should feel like it belongs in the same system
  - `Types.hpp`: Using `f32` instead of `float` maintains type consistency across the codebase
  - `Render.hpp`: Screens will call Renderer methods — the base class doesn't call them directly but implementers need to know the API
  - `Data.hpp`: The accessor pattern is the integration point — understand how `Data::events()` works to mirror it for `Data::screens()`

  **Acceptance Criteria**:

  - [ ] File exists: `src/UI/Screen.hpp`
  - [ ] Class `Screen` in namespace `biofuel::ui`
  - [ ] All lifecycle methods declared as virtual: onEnter, onExit, onPause, onResume, onUpdate (pure virtual), onRender (pure virtual), onInput
  - [ ] TransitionState enum defined with None, TransitionIn, TransitionOut
  - [ ] Passthrough flags: m_passthroughRender, m_passthroughUpdate, m_passthroughInput
  - [ ] getTransitionAlpha(), needsRemoval(), isTransitioning() methods declared
  - [ ] ScreenManager* m_manager member with setManager()
  - [ ] Virtual destructor, non-copyable, non-movable

  **QA Scenarios (MANDATORY):**

  ```
  Scenario: Screen.hpp compiles and has correct structure
    Tool: Bash (grep)
    Preconditions: File exists at src/UI/Screen.hpp
    Steps:
      1. grep "virtual void onRender" src/UI/Screen.hpp  → should find pure virtual declaration
      2. grep "virtual void onUpdate" src/UI/Screen.hpp  → should find pure virtual declaration
      3. grep "TransitionState" src/UI/Screen.hpp  → should find enum class
      4. grep "m_passthroughRender" src/UI/Screen.hpp  → should find member
      5. grep "getTransitionAlpha" src/UI/Screen.hpp  → should find method
      6. grep "namespace biofuel::ui" src/UI/Screen.hpp  → should find namespace
    Expected Result: All 6 grep commands return at least 1 match each
    Failure Indicators: Any grep returns empty
    Evidence: .sisyphus/evidence/task-1-screen-structure.txt
  ```

  **Commit**: YES (groups with Tasks 2, 3)
  - Message: `feat(ui): add Screen base class, ScreenManager singleton, remove Game stub`
  - Files: `src/UI/Screen.hpp`

- [x] 2. Delete Game.hpp/cpp + Update CMake Includes

  **What to do**:
  - Delete `src/Core/Game.hpp` — orphaned file, never used by App or any other file
  - Delete `src/Core/Game.cpp` — orphaned file, stub implementations only
  - Verify no other file references Game.hpp (use grep/search)
  - Add `${CMAKE_CURRENT_SOURCE_DIR}/UI/screens` to `src/CMakeLists.txt` include paths (needed for future MainMenuScreen includes)
  - Verify the project still compiles after deletion

  **Must NOT do**:
  - Do NOT delete any other files (Types.hpp stays, App.hpp stays)
  - Do NOT modify any other include paths in CMakeLists.txt

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: File deletion + one-line CMake edit, straightforward
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES (with Tasks 1, 3)
  - **Parallel Group**: Wave 1
  - **Blocks**: Task 4
  - **Blocked By**: None

  **References**:

  **Pattern References**:
  - `src/CMakeLists.txt:9-26` — Current include paths — add `UI/screens` in the same style after the existing `UI` line

  **API/Type References**:
  - `src/Core/Game.hpp` — The file being deleted — verify it contains only GameState enum and Game class
  - `src/Core/Game.cpp` — The file being deleted — verify it contains only stub implementations

  **Test References**:
  - Use `grep -r "Game.hpp" src/` to verify no other file includes Game.hpp before deleting

  **WHY Each Reference Matters**:
  - `CMakeLists.txt`: Must add the new include path in the exact same format as existing ones
  - `Game.hpp/cpp`: Must verify the file is truly orphaned before deleting — if anything references it, deletion would break the build

  **Acceptance Criteria**:

  - [ ] `src/Core/Game.hpp` does not exist
  - [ ] `src/Core/Game.cpp` does not exist
  - [ ] `grep -r "Game.hpp" src/` returns zero matches
  - [ ] `src/CMakeLists.txt` contains `${CMAKE_CURRENT_SOURCE_DIR}/UI/screens` in include paths
  - [ ] `cmake --build build --config Release` succeeds with zero errors

  **QA Scenarios (MANDATORY):**

  ```
  Scenario: Game files deleted, no references remain
    Tool: Bash
    Preconditions: Game.hpp and Game.cpp existed before
    Steps:
      1. Test-Path src/Core/Game.hpp → should return False
      2. Test-Path src/Core/Game.cpp → should return False
      3. grep -r "Game" src/ --include="*.hpp" --include="*.cpp" | grep -v "BiofuelGame" → should return empty
    Expected Result: Both files gone, no code references Game class
    Failure Indicators: File still exists, or grep finds a reference
    Evidence: .sisyphus/evidence/task-2-game-deleted.txt

  Scenario: CMake includes UI/screens path
    Tool: Bash (grep)
    Preconditions: src/CMakeLists.txt exists
    Steps:
      1. grep "UI/screens" src/CMakeLists.txt → should find the include path
    Expected Result: Include path present
    Failure Indicators: No match found
    Evidence: .sisyphus/evidence/task-2-cmake-includes.txt
  ```

  **Commit**: YES (groups with Tasks 1, 3)
  - Message: `feat(ui): add Screen base class, ScreenManager singleton, remove Game stub`
  - Files: (deleted) `src/Core/Game.hpp`, `src/Core/Game.cpp`, `src/CMakeLists.txt`

- [x] 3. ScreenManager Singleton

  **What to do**:
  - Create `src/UI/ScreenManager.hpp` and `src/UI/ScreenManager.cpp`
  - Singleton pattern following `EventManager` exactly: `static ScreenManager& instance()`, private constructor/destructor, non-copyable/non-movable
  - Lifecycle: `init()`, `shutdown()` — called from App, same as EventManager
  - Stack operations:
    - `push(std::unique_ptr<Screen> screen)` — push screen on top of stack, call its `onEnter()`, set its `setManager(this)`
    - `pop()` — call top screen's `onExit()`, remove from stack, call new top's `onResume()` if any
    - `replace(std::unique_ptr<Screen> screen)` — pop top + push new in one operation (calls old onExit, new onEnter)
    - `clear()` — remove all screens, calling onExit on each
  - Per-frame delegation:
    - `update(float dt)` — iterate stack top-to-bottom. For each screen: if top, call `onUpdate(dt)`. If not top and `m_passthroughUpdate`, call `onUpdate(dt)`. Also update transition progress for all transitioning screens.
    - `render()` — iterate stack bottom-to-top. For each screen: if top, call `onRender()`. If not top and `m_passthroughRender`, call `onRender()`. Draw fade overlay for transitioning screens.
    - `handleInput()` — call only top screen's `onInput()`. If top has `m_passthroughInput`, also call next screen's `onInput()`.
  - Transition handling:
    - When `push()` or `replace()` is called, set new screen's `m_transitionState = TransitionState::TransitionIn`, `m_transitionProgress = 0.0f`
    - In `update(dt)`, advance transition progress for transitioning screens
    - When transition-in completes (progress >= 1.0), set state to `TransitionState::None`
    - When screen needs to exit, set `m_transitionState = TransitionState::TransitionOut`, `m_transitionProgress = 0.0f`
    - When transition-out completes, mark screen for removal via `needsRemoval()`
  - Accessors: `[[nodiscard]] Screen* currentScreen()`, `[[nodiscard]] bool isEmpty()`, `[[nodiscard]] size_t stackSize()`
  - Namespace: `biofuel::ui`

  **Must NOT do**:
  - Do NOT call `Renderer::beginFrame()` or `Renderer::endFrame()` — that stays in App
  - Do NOT emit events for screen transitions — use direct method calls
  - Do NOT add screens other than through push/replace — no "register by name" factory
  - Do NOT add a `ScreenFactory` or screen registry system — over-engineering for now

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: Substantial logic (stack management, transition timing, passthrough iteration) — more than a quick task but not deeply algorithmic
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES (with Tasks 1, 2 — but needs Task 1's Screen.hpp to compile)
  - **Parallel Group**: Wave 1 (depends on Task 1 for Screen.hpp, but can write .cpp/.hpp in parallel since Screen.hpp interface is known from the plan)
  - **Blocks**: Tasks 4, 6, 7
  - **Blocked By**: Task 1 (Screen base class must exist for ScreenManager to compile)

  **References**:

  **Pattern References** (existing code to follow):
  - `src/Data/event/EventManager.hpp:12-36` — EXACT singleton pattern to copy: `static instance()`, private constructor, `init()`/`shutdown()`, non-copyable/non-movable
  - `src/Data/event/EventManager.cpp:1-39` — Singleton implementation pattern: Meyers singleton for `instance()`, lazy init in `dispatcher()`, clear + reset in `shutdown()`

  **API/Type References**:
  - `src/UI/Screen.hpp` (Task 1) — Screen interface that ScreenManager manages — all lifecycle methods, TransitionState enum, passthrough flags, getTransitionAlpha(), needsRemoval()
  - `src/Utils/render/Render.hpp:14-19` — Renderer::drawRect() will be used for fade overlay in render()

  **External References**:
  - KatanaEngine ScreenManager pattern: push/pop stack with transition support

  **WHY Each Reference Matters**:
  - `EventManager`: ScreenManager must feel identical to use — same singleton pattern, same Data accessor, same init/shutdown lifecycle. Copying this pattern ensures consistency.
  - `Screen.hpp`: This is the contract ScreenManager implements against — every method ScreenManager calls must match Screen's virtual interface exactly.
  - `Render.hpp`: The fade overlay will use `Renderer::drawRect(0, 0, screenW, screenH, Color{0,0,0,alpha})` — must understand the drawRect signature.

  **Acceptance Criteria**:

  - [ ] Files exist: `src/UI/ScreenManager.hpp`, `src/UI/ScreenManager.cpp`
  - [ ] Class `ScreenManager` in namespace `biofuel::ui`
  - [ ] Singleton with `static ScreenManager& instance()`, private constructor
  - [ ] `init()`, `shutdown()` lifecycle methods
  - [ ] `push()`, `pop()`, `replace()`, `clear()` stack operations
  - [ ] `update(float dt)`, `render()`, `handleInput()` per-frame delegation
  - [ ] Transition state management in update()
  - [ ] `currentScreen()`, `isEmpty()`, `stackSize()` accessors
  - [ ] Compiles: `cmake --build build --config Release` succeeds

  **QA Scenarios (MANDATORY):**

  ```
  Scenario: ScreenManager.hpp has correct singleton pattern
    Tool: Bash (grep)
    Preconditions: File exists at src/UI/ScreenManager.hpp
    Steps:
      1. grep "static ScreenManager& instance()" src/UI/ScreenManager.hpp → should match
      2. grep "void push" src/UI/ScreenManager.hpp → should match
      3. grep "void pop" src/UI/ScreenManager.hpp → should match
      4. grep "void replace" src/UI/ScreenManager.hpp → should match
      5. grep "void update" src/UI/ScreenManager.hpp → should match
      6. grep "void render" src/UI/ScreenManager.hpp → should match
      7. grep "void handleInput" src/UI/ScreenManager.hpp → should match
      8. grep "ScreenManager(const ScreenManager&) = delete" src/UI/ScreenManager.hpp → should match
    Expected Result: All 8 grep commands return matches
    Failure Indicators: Any grep returns empty
    Evidence: .sisyphus/evidence/task-3-screenmanager-structure.txt

  Scenario: ScreenManager.cpp compiles and links
    Tool: Bash (cmake)
    Preconditions: Task 1 (Screen.hpp) is complete
    Steps:
      1. cmake --build build --config Release 2>&1
    Expected Result: Build succeeds with 0 errors
    Failure Indicators: Any error mentioning ScreenManager or Screen
    Evidence: .sisyphus/evidence/task-3-build-success.txt
  ```

  **Commit**: YES (groups with Tasks 1, 2)
  - Message: `feat(ui): add Screen base class, ScreenManager singleton, remove Game stub`
  - Files: `src/UI/ScreenManager.hpp`, `src/UI/ScreenManager.cpp`

- [x] 4. Refactor App.cpp to Delegate to ScreenManager

  **What to do**:
  - Modify `src/Core/App.hpp`:
    - Add `#include "UI/ScreenManager.hpp"` (or forward declare)
    - Remove `void render()` from private — keep it, but its body changes
    - No new members needed — ScreenManager is a singleton accessed via `Data::screens()`
  - Modify `src/Core/App.cpp`:
    - In `init()`: After `Data::events().init()`, add `Data::screens().init()`
    - In `shutdown()`: Before `Data::events().shutdown()`, add `Data::screens().shutdown()`
    - In `update()`: Replace empty body with `Data::screens().update(deltaTime)`
    - In `processInput()`: Replace empty body with `Data::screens().handleInput()`
    - In `render()`: Replace ALL inline menu drawing with:
      ```cpp
      utils::render::Renderer::beginFrame(BLACK);
      Data::screens().render();
      // FPS counter (debug info, stays here)
      const int screenH = utils::render::Renderer::screenHeight();
      utils::render::Renderer::drawText(
          TextFormat("Window: %dx%d | FPS: %d",
              utils::render::Renderer::screenWidth(), screenH, GetFPS()),
          20, screenH - 30, 16, DARKGRAY);
      utils::render::Renderer::endFrame();
      ```
  - The `render()` method now does: beginFrame → ScreenManager.render() → FPS counter → endFrame
  - That's it. No more `drawText("Biofuel Game - Fuel Farm", ...)` etc. in App.cpp

  **Must NOT do**:
  - Do NOT move beginFrame/endFrame into ScreenManager or Screen classes
  - Do NOT remove the FPS counter — it stays in App::render()
  - Do NOT add any Screen push logic to App::init() yet — that comes in Task 5 (MainMenuScreen)
  - Do NOT modify App.hpp's Config struct or constructor

  **Recommended Agent Profile**:
  - **Category**: `deep`
    - Reason: Refactoring the main application loop — must understand the existing flow and replace it correctly without breaking anything
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO — depends on Tasks 1, 2, 3
  - **Parallel Group**: Wave 2
  - **Blocks**: Task 7
  - **Blocked By**: Tasks 1, 2, 3

  **References**:

  **Pattern References**:
  - `src/Core/App.cpp:95-138` — CURRENT render() method with inline menu drawing — ALL of lines 111-135 (the menu text drawing) must be replaced with `Data::screens().render()`
  - `src/Core/App.cpp:52-53` — Current `Data::events().init()` call — add `Data::screens().init()` right after it, same pattern
  - `src/Core/App.cpp:65` — Current `Data::events().shutdown()` call — add `Data::screens().shutdown()` right before it, same pattern

  **API/Type References**:
  - `src/UI/ScreenManager.hpp` (Task 3) — ScreenManager API: init(), shutdown(), update(dt), render(), handleInput()
  - `src/Data/Data.hpp:19-28` — Data accessor pattern — will add `Data::screens()` in Task 6

  **WHY Each Reference Matters**:
  - `App.cpp:95-138`: This is the exact code being refactored. The executor must understand which lines to keep (beginFrame, endFrame, FPS counter) and which to delete (all menu text drawing).
  - `App.cpp:52-53,65`: The pattern for adding lifecycle calls — add `Data::screens().init()` in the same position as `Data::events().init()`.
  - `ScreenManager.hpp`: The API contract — must call the exact method signatures defined there.

  **Acceptance Criteria**:

  - [ ] `App::init()` calls `Data::screens().init()` after `Data::events().init()`
  - [ ] `App::shutdown()` calls `Data::screens().shutdown()` before `Data::events().shutdown()`
  - [ ] `App::update()` calls `Data::screens().update(deltaTime)`
  - [ ] `App::processInput()` calls `Data::screens().handleInput()`
  - [ ] `App::render()` contains ONLY: beginFrame(BLACK), Data::screens().render(), FPS counter, endFrame()
  - [ ] No inline menu text drawing remains in App::render() — grep for `drawText("Biofuel` returns zero matches
  - [ ] Compiles: `cmake --build build --config Release` succeeds

  **QA Scenarios (MANDATORY):**

  ```
  Scenario: App::render() no longer has inline menu drawing
    Tool: Bash (grep)
    Preconditions: App.cpp has been modified
    Steps:
      1. grep "drawText.*Biofuel Game" src/Core/App.cpp → should return empty
      2. grep "drawText.*Controls" src/Core/App.cpp → should return empty
      3. grep "Data::screens().render()" src/Core/App.cpp → should return 1 match
      4. grep "Data::screens().init()" src/Core/App.cpp → should return 1 match
      5. grep "Data::screens().shutdown()" src/Core/App.cpp → should return 1 match
    Expected Result: No inline menu drawing, ScreenManager calls present
    Failure Indicators: Any drawText with menu content, or missing ScreenManager calls
    Evidence: .sisyphus/evidence/task-4-app-refactored.txt

  Scenario: Build succeeds after refactor
    Tool: Bash (cmake)
    Preconditions: All Wave 1 tasks complete
    Steps:
      1. cmake --build build --config Release 2>&1
    Expected Result: Build succeeds (may have linking issues until Task 5 adds MainMenuScreen — that's OK, capture the error)
    Failure Indicators: Compilation errors in App.cpp itself (not missing symbol from MainMenuScreen)
    Evidence: .sisyphus/evidence/task-4-build.txt
  ```

  **Commit**: YES (groups with Tasks 5, 6)
  - Message: `refactor(app): delegate rendering to ScreenManager and MainMenuScreen`
  - Files: `src/Core/App.hpp`, `src/Core/App.cpp`

- [x] 5. MainMenuScreen Implementation

  **What to do**:
  - Create `src/UI/screens/MainMenuScreen.hpp` and `src/UI/screens/MainMenuScreen.cpp`
  - `MainMenuScreen` inherits from `biofuel::ui::Screen`
  - `onRender()` — renders the EXACT same content that was previously in App::render():
    ```cpp
    void MainMenuScreen::onRender() {
        using namespace utils::render;
        // Title
        Renderer::drawText("Biofuel Game - Fuel Farm", 20, 20, 30, RAYWHITE);
        // Subtitle
        Renderer::drawText("2D Pixel-Art Biofuel Management Sim with 2D/3D Model Swap", 20, 60, 18, GRAY);
        // Info block
        const int infoY = 120;
        const int lineHeight = 24;
        Renderer::drawText("Controls:", 20, infoY, 20, RED);
        Renderer::drawText("- ESC to exit", 20, infoY + lineHeight, 18, LIGHTGRAY);
        Renderer::drawText("- Resize window freely (min 1280x720)", 20, infoY + lineHeight * 2, 18, LIGHTGRAY);
        Renderer::drawText("- This is a placeholder window for the biofuel farming game.", 20, infoY + lineHeight * 3, 18, LIGHTGRAY);
    }
    ```
  - `onUpdate(float dt)` — empty for now (no menu animation yet)
  - `onInput()` — handle ESC key to request window close (can emit `WindowCloseRequestedEvent` or call Raylib's `WindowShouldClose()` check is in App's main loop already, so this can be empty for now)
  - `onEnter()` — no-op (no assets to load yet)
  - `onExit()` — no-op (no assets to unload yet)
  - Namespace: `biofuel::ui::screens`
  - Push the MainMenuScreen in App::init() after ScreenManager init:
    ```cpp
    Data::screens().init();
    Data::screens().push(std::make_unique<screens::MainMenuScreen>());
    ```

  **Must NOT do**:
  - Do NOT add button UI components — just raw text like before
  - Do NOT add menu animation or fancy effects — identical to current output
  - Do NOT add background music or sound
  - Do NOT call beginFrame/endFrame — that's App's job

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: Must exactly replicate existing visual output — no deviation allowed. Requires careful attention to detail.
  - **Skills**: [`playwright`]
    - `playwright`: Needed to visually verify the Main Menu looks identical after refactor

  **Parallelization**:
  - **Can Run In Parallel**: YES (with Tasks 4, 6 — but needs Task 3 for ScreenManager to push onto)
  - **Parallel Group**: Wave 2
  - **Blocks**: Task 7
  - **Blocked By**: Tasks 1, 3

  **References**:

  **Pattern References** (existing code to follow — CRITICAL EXACT COPY):
  - `src/Core/App.cpp:111-127` — EXACT rendering code to copy into MainMenuScreen::onRender(). Every drawText call, every position, every color, every font size must match character-for-character.

  **API/Type References**:
  - `src/UI/Screen.hpp` (Task 1) — Abstract base class MainMenuScreen inherits from — must implement pure virtual `onUpdate()` and `onRender()`
  - `src/Utils/render/Render.hpp:14-19` — Renderer API — same drawText calls as before

  **WHY Each Reference Matters**:
  - `App.cpp:111-127`: This is the reference output. The MainMenuScreen must produce IDENTICAL visual output. Copy the code verbatim, then delete it from App.cpp (Task 4 handles the deletion).
  - `Screen.hpp`: Must implement all pure virtual methods, can leave other virtuals with default empty implementation.
  - `Render.hpp`: Same drawText API — no changes to how rendering works, just where the calls live.

  **Acceptance Criteria**:

  - [ ] Files exist: `src/UI/screens/MainMenuScreen.hpp`, `src/UI/screens/MainMenuScreen.cpp`
  - [ ] Class `MainMenuScreen` in namespace `biofuel::ui::screens`, inherits `Screen`
  - [ ] `onRender()` contains all 6 drawText calls from the original App::render()
  - [ ] `onUpdate()` implemented (empty body is OK)
  - [ ] `onInput()` implemented (empty body is OK)
  - [ ] App::init() pushes MainMenuScreen after ScreenManager init
  - [ ] Game launches and shows identical Main Menu text, colors, positions
  - [ ] Compiles: `cmake --build build --config Release` succeeds

  **QA Scenarios (MANDATORY):**

  ```
  Scenario: MainMenuScreen renders identical output
    Tool: Playwright (playwright skill)
    Preconditions: Game built at build/bin/Release/BiofuelGame.exe
    Steps:
      1. Launch BiofuelGame.exe
      2. Wait 2 seconds for window to appear
      3. Take screenshot of the entire window
      4. Verify "Biofuel Game - Fuel Farm" text is visible (RAYWHITE on BLACK)
      5. Verify "Controls:" text is visible (RED on BLACK)
      6. Verify FPS counter at bottom (DARKGRAY on BLACK)
      7. Close window
    Expected Result: Screenshot shows black background with white title text and red "Controls:" header, same as before refactor
    Failure Indicators: White/blank background, missing text, different colors, different positions
    Evidence: .sisyphus/evidence/task-5-mainmenu-screenshot.png

  Scenario: Window resizing still works
    Tool: Playwright (playwright skill)
    Preconditions: Game running
    Steps:
      1. Launch BiofuelGame.exe
      2. Resize window to 1600x900
      3. Verify FPS counter updates to show new dimensions "Window: 1600x900"
      4. Resize window to attempt 800x600 (below minimum)
      5. Verify window does NOT go below 1280x720
    Expected Result: Window resizes above 1280x720 freely, cannot shrink below minimum
    Failure Indicators: Window shrinks below 1280x720, or FPS counter doesn't update
    Evidence: .sisyphus/evidence/task-5-resize-test.png
  ```

  **Commit**: YES (groups with Tasks 4, 6)
  - Message: `refactor(app): delegate rendering to ScreenManager and MainMenuScreen`
  - Files: `src/UI/screens/MainMenuScreen.hpp`, `src/UI/screens/MainMenuScreen.cpp`

- [x] 6. Update Data.hpp with Data::screens() Accessor

  **What to do**:
  - Modify `src/Data/Data.hpp`:
    - Add `#include "UI/ScreenManager.hpp"` (or forward declare + include in .cpp — but Data.hpp is header-only like current pattern)
    - Add `Data::screens()` static method following the exact pattern of `Data::events()`:
      ```cpp
      [[nodiscard]] static ui::ScreenManager& screens() {
          return ui::ScreenManager::instance();
      }
      ```
  - This is the integration point — everywhere in the codebase that needs ScreenManager uses `Data::screens()`

  **Must NOT do**:
  - Do NOT remove existing `Data::eventBus()` or `Data::events()` methods
  - Do NOT change the namespace or naming convention — follow existing pattern exactly
  - Do NOT add any other accessors

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Adding one include and one method — 3 lines of code
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES (with Tasks 4, 5 — but needs Task 3 for ScreenManager to exist)
  - **Parallel Group**: Wave 2
  - **Blocks**: Task 7
  - **Blocked By**: Task 3

  **References**:

  **Pattern References** (CRITICAL — copy exactly):
  - `src/Data/Data.hpp:19-28` — EXACT pattern to follow: static methods returning singleton references. `Data::events()` returns `EventManager&`, `Data::screens()` returns `ScreenManager&`

  **API/Type References**:
  - `src/UI/ScreenManager.hpp` (Task 3) — `ScreenManager::instance()` is the method to call

  **WHY Each Reference Matters**:
  - `Data.hpp`: This is the blueprint. The new accessor must be identical in style — same `[[nodiscard]]`, same static method, same singleton access pattern.

  **Acceptance Criteria**:

  - [ ] `Data.hpp` includes `UI/ScreenManager.hpp`
  - [ ] `Data::screens()` method exists and returns `ui::ScreenManager&`
  - [ ] `Data::eventBus()` and `Data::events()` still exist unchanged
  - [ ] Compiles: `cmake --build build --config Release` succeeds

  **QA Scenarios (MANDATORY):**

  ```
  Scenario: Data.hpp has screens accessor
    Tool: Bash (grep)
    Preconditions: Data.hpp has been modified
    Steps:
      1. grep "static ui::ScreenManager& screens()" src/Data/Data.hpp → should match
      2. grep "ScreenManager" src/Data/Data.hpp → should match (include + method)
      3. grep "eventBus" src/Data/Data.hpp → should still match (not removed)
      4. grep "events" src/Data/Data.hpp → should still match (not removed)
    Expected Result: New accessor present, old accessors intact
    Failure Indicators: Missing screens() method, or old methods removed
    Evidence: .sisyphus/evidence/task-6-data-accessor.txt
  ```

  **Commit**: YES (groups with Tasks 4, 5)
  - Message: `refactor(app): delegate rendering to ScreenManager and MainMenuScreen`
  - Files: `src/Data/Data.hpp`

- [x] 7. Fade Transition System

  **What to do**:
  - Enhance `ScreenManager::update()` and `ScreenManager::render()` to handle fade transitions
  - **Transition-In flow** (when a screen is pushed):
    1. ScreenManager sets new screen's `m_transitionState = TransitionState::TransitionIn`, `m_transitionProgress = 0.0f`
    2. Each frame in `update(dt)`: advance `m_transitionProgress += dt / m_transitionDuration`
    3. In `render()`: draw a black overlay over the screen with alpha = `(1.0 - getTransitionAlpha())` — so it starts fully black (alpha=1.0) and fades to transparent (alpha=0.0)
    4. When progress >= 1.0: set `m_transitionState = TransitionState::None`
  - **Transition-Out flow** (when a screen is popped or replaced):
    1. ScreenManager sets departing screen's `m_transitionState = TransitionState::TransitionOut`, `m_transitionProgress = 0.0f`
    2. Each frame: advance progress same as above
    3. In `render()`: draw black overlay with alpha = `getTransitionAlpha()` — starts transparent, fades to black
    4. When progress >= 1.0: screen's `needsRemoval()` returns true, ScreenManager removes it
  - **Render the fade overlay** using existing Renderer:
    ```cpp
    // In ScreenManager::render(), after drawing all screens:
    if (auto* top = currentScreen(); top && top->isTransitioning()) {
        const float alpha = top->getTransitionAlpha();
        // Invert for overlay: TransitionIn means overlay fades OUT (1→0), TransitionOut means overlay fades IN (0→1)
        const float overlayAlpha = (top->getTransitionState() == Screen::TransitionState::TransitionIn) 
            ? (1.0f - alpha) : alpha;
        const Color fadeColor = { 0, 0, 0, static_cast<unsigned char>(overlayAlpha * 255.0f) };
        Renderer::drawRect(0, 0, Renderer::screenWidth(), Renderer::screenHeight(), fadeColor);
    }
    ```
  - **Queue behavior**: If a transition is already in progress when a new push/pop/replace is requested, queue the request and execute it when the current transition completes. This prevents broken visual states.
  - Default transition duration: `0.5f` seconds (half a second). Configurable per-screen via `m_transitionDuration`.
  - Add `ScreenManager::requestPush()`, `requestPop()`, `requestReplace()` — these queue transition-aware operations. The existing `push()`/`pop()`/`replace()` become the immediate (no-transition) versions, used internally.
  - Alternatively: simplify — just use push/pop/replace directly and always trigger transition. No queuing for now. If a transition is in progress, the new request is ignored (with a log warning via spdlog). This is simpler and avoids edge cases.

  **Must NOT do**:
  - Do NOT add new methods to `Renderer` — use existing `drawRect()` with alpha Color
  - Do NOT use GLSL shaders for the fade — a simple alpha rect is sufficient
  - Do NOT add transition easing curves yet — linear is fine for now
  - Do NOT add sound effects for transitions

  **Recommended Agent Profile**:
  - **Category**: `deep`
    - Reason: Transition state machine with timing, alpha blending, and edge case handling — requires careful implementation
  - **Skills**: []
  - **Skills Evaluated but Omitted**:
    - `playwright`: Used in Task 5 QA — not needed for implementation itself

  **Parallelization**:
  - **Can Run In Parallel**: NO — depends on Tasks 4, 5, 6
  - **Parallel Group**: Wave 3
  - **Blocks**: F1-F4
  - **Blocked By**: Tasks 4, 5, 6

  **References**:

  **Pattern References**:
  - `src/UI/Screen.hpp` (Task 1) — `TransitionState` enum, `getTransitionAlpha()`, `isTransitioning()`, `needsRemoval()`, `m_transitionProgress`, `m_transitionDuration`
  - `src/UI/ScreenManager.hpp` (Task 3) — `update()` and `render()` methods where transition logic is added

  **API/Type References**:
  - `src/Utils/render/Render.hpp:19` — `Renderer::drawRect(int x, int y, int width, int height, Color color)` — used for fade overlay. Raylib's `Color` struct has `a` field (0-255) for alpha.

  **External References**:
  - KatanaEngine transition pattern: Screen manages own TransitionState, ScreenManager handles timing

  **WHY Each Reference Matters**:
  - `Screen.hpp`: The transition state machine is defined there — ScreenManager must respect and advance it
  - `ScreenManager.hpp`: The update/render methods are where transition logic integrates
  - `Render.hpp`: Must use the exact drawRect signature — no modifications to Renderer allowed

  **Acceptance Criteria**:

  - [ ] `ScreenManager::update()` advances transition progress for transitioning screens
  - [ ] `ScreenManager::render()` draws black overlay with correct alpha during transitions
  - [ ] Transition-In: screen fades from black to visible (overlay alpha 1→0)
  - [ ] Transition-Out: screen fades from visible to black (overlay alpha 0→1)
  - [ ] `needsRemoval()` is checked after transition-out completes — screen is removed from stack
  - [ ] If transition in progress and new request comes, it's ignored (with spdlog warning)
  - [ ] Default transition duration is 0.5 seconds
  - [ ] Compiles: `cmake --build build --config Release` succeeds
  - [ ] Game launches — MainMenuScreen appears with a brief fade-in from black

  **QA Scenarios (MANDATORY):**

  ```
  Scenario: MainMenuScreen fades in on launch
    Tool: Playwright (playwright skill)
    Preconditions: Game built, fade transitions implemented
    Steps:
      1. Launch BiofuelGame.exe
      2. Immediately take screenshot (frame 1) — should show mostly black (fade just starting)
      3. Wait 1 second
      4. Take screenshot — should show full Main Menu visible (fade complete)
    Expected Result: Brief fade-in from black, then menu fully visible
    Failure Indicators: Menu appears instantly (no fade), or stays black (fade never completes), or flickers
    Evidence: .sisyphus/evidence/task-7-fade-in-early.png, .sisyphus/evidence/task-7-fade-in-complete.png

  Scenario: Game still renders correctly after fade completes
    Tool: Playwright (playwright skill)
    Preconditions: Fade transition implemented
    Steps:
      1. Launch BiofuelGame.exe
      2. Wait 2 seconds (fade-in complete)
      3. Take screenshot
      4. Verify "Biofuel Game - Fuel Farm" text visible at (20, 20)
      5. Verify "Controls:" text visible
      6. Verify FPS counter at bottom
    Expected Result: Identical visual output to pre-transition implementation
    Failure Indicators: Missing text, wrong colors, visual artifacts
    Evidence: .sisyphus/evidence/task-7-final-menu.png
  ```

  **Commit**: YES
  - Message: `feat(ui): add fade transition system for screen switches`
  - Files: `src/UI/ScreenManager.hpp`, `src/UI/ScreenManager.cpp`, `src/UI/Screen.hpp`

---

## Final Verification Wave

- [x] F1. **Plan Compliance Audit** — `oracle`
  Read the plan end-to-end. For each "Must Have": verify implementation exists (read file, grep codebase). For each "Must NOT Have": search codebase for forbidden patterns — reject with file:line if found. Check evidence files exist in .sisyphus/evidence/. Compare deliverables against plan.
  Output: `Must Have [N/N] | Must NOT Have [N/N] | Tasks [N/N] | VERDICT: APPROVE/REJECT`

- [x] F2. **Code Quality Review** — `unspecified-high`
  Run `cmake --build build --config Release`. Review all changed files for: `as any`/`@ts-ignore` equivalents, empty catches, commented-out code, unused imports, AI slop (excessive comments, over-abstraction, generic names). Check namespace consistency.
  Output: `Build [PASS/FAIL] | Files [N clean/N issues] | VERDICT`

- [x] F3. **Real Manual QA** — `unspecified-high` (+ playwright skill)
  Launch the built executable. Verify Main Menu renders identically to before: same title text, same colors, same positions, black background. Verify FPS counter visible at bottom. Take screenshots as evidence. Test window resizing still works.
  Output: `Scenarios [N/N pass] | Visual [MATCH/MISMATCH] | VERDICT`

- [x] F4. **Scope Fidelity Check** — `deep`
  For each task: read "What to do", read actual diff. Verify 1:1 — everything in spec was built, nothing beyond spec was built. Check "Must NOT do" compliance. Detect cross-task contamination. Flag unaccounted changes.
  Output: `Tasks [N/N compliant] | Contamination [CLEAN/N issues] | Unaccounted [CLEAN/N files] | VERDICT`

---

## Commit Strategy

- **Task 1+2+3**: `feat(ui): add Screen base class, ScreenManager singleton, remove Game stub` — Screen.hpp, ScreenManager.hpp/.cpp, delete Game.hpp/.cpp, CMakeLists.txt
- **Task 4+5+6**: `refactor(app): delegate rendering to ScreenManager and MainMenuScreen` — App.cpp, App.hpp, MainMenuScreen.hpp/.cpp, Data.hpp
- **Task 7**: `feat(ui): add fade transition system for screen switches` — ScreenManager.cpp, Screen.hpp

---

## Success Criteria

### Verification Commands
```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release  # Expected: Configuring done
cmake --build build --config Release --parallel   # Expected: BiofuelGame.exe built, 0 errors
```

### Final Checklist
- [ ] All "Must Have" present
- [ ] All "Must NOT Have" absent
- [ ] Game launches with identical Main Menu output
- [ ] App::render() contains no inline menu draw calls
- [ ] Game.hpp and Game.cpp deleted
- [ ] Fade transitions work between screen switches
