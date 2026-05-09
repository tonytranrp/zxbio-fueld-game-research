# Code Cleanliness & Shader Module Reorganization

## TL;DR

> **Quick Summary**: Fix all coding standard violations across src/ (raw types → project types, missing [[nodiscard]]/noexcept/constexpr, designated initializers) and reorganize the shader system into a Module-based architecture under `src/Utils/render/Shader/` with per-shader classes, constexpr configs, and a comprehensive README.
> 
> **Deliverables**:
> - All event structs using `i32`/`f32` instead of `int`/`float`
> - All singletons have `[[nodiscard]]` on `instance()`
> - All Renderer/ShaderManager helpers have `noexcept`
> - `App::Config` uses `i32` and designated initializers
> - `AnimationEvents.hpp` uses `std::string_view` instead of `const char*`
> - Shader Module system: `ShaderModule.hpp` base config + `BlurHModule.hpp` + `BlurVModule.hpp`
> - `App::init()` uses module-based shader registration
> - `ScreenBlurEffect` updated to use module references
> - `Shader/README.md` with creation guide
> - Duplicated ShaderManager load preamble extracted to private helper
> 
> **Estimated Effort**: Medium
> **Parallel Execution**: YES - 3 waves
> **Critical Path**: Wave 1 (type fixes, foundation) → Wave 2 (shader modules) → Wave 3 (integration + verification)

---

## Context

### Original Request
"now lets create an plan for code reviews and then for the cleaness of coding standards as ussing the most template and types as possible, compile times are the most important for perfomance, find everything about our code base about cleaness for us and then making it much more useefull, then for one thing i would like you to chnage is thatin our render folder @src\Utils\render/ for our EmbededShader, i would like you to make an Sub folder inside of the render called Shader and inside of there there should be Modules that connects over to Shadermanager and overall everything for initlization of that shader code, and inside the sub folder each shader is going to have its own class of Modules default and an good readme of how to create new Shader modules and how to use them and initlize them correctly"

### Interview Summary
**Key Discussions**:
- Shader Module pattern: Hybrid — Module base class + constexpr config structs (user chose this)
- Shader scale: 16+ shaders planned (user chose "large")
- Compile-time priority: Both constexpr + minimal includes (user chose this)
- Each shader gets its own .hpp file under Shader/ subfolder
- ShaderManager connects to Module classes for initialization

**Research Findings**:
- 20 specific coding standard violations identified across the codebase
- 3 event files completely bypassing project type system (raw int/float)
- 5 singleton instance() methods missing [[nodiscard]]
- ShaderManager has ~10 lines duplicated between load() and loadFromMemory()
- ScreenBlurEffect calls DrawTextureRec directly (acceptable in shader context)
- No automated test framework exists — all verification is manual

### Metis Review
**Identified Gaps** (addressed):
- Shader Module base class MUST NOT use virtual dispatch (hot render path) → Convention-only approach, no inheritance
- Module headers MUST NOT include ShaderManager.hpp or raylib.h (compile-time priority) → Only Core/Types.hpp and string_view
- C++20 designated initializers with Raylib Color must be verified on MSVC → Added verification task
- DrawTextureRec bypass is out of scope for this workstream → Documented as known exception
- App::Config in main.cpp uses positional init instead of designated initializers → Added to fix list
- App::run() missing [[nodiscard]] → Added to fix list
- Need to update CMakeLists.txt for new Shader/ directory → Added to task list
- No automated tests exist → All verification is manual build + run

---

## Work Objectives

### Core Objective
Fix all identified coding standard violations across the codebase and create a clean Module-based shader architecture that scales to 16+ shaders with minimal compile-time overhead.

### Concrete Deliverables
- `src/Data/event/mouse/MouseEvents.hpp` — uses `f32`/`i32` instead of raw types
- `src/Data/event/input/InputEvents.hpp` — uses `i32` instead of `int`
- `src/Data/event/screen/ScreenEvents.hpp` — uses `i32` instead of `int`
- `src/Data/event/animation/AnimationEvents.hpp` — uses `std::string_view` instead of `const char*`
- `src/Core/App.hpp` — Config uses `i32` and designated initializers
- `src/Utils/render/Render.hpp/.cpp` — `noexcept` on all methods, `[[nodiscard]]` on const methods
- `src/Utils/render/ShaderManager.hpp/.cpp` — `noexcept` on helpers, deduplicated load preamble
- `src/AnimationController/screen/ScreenBlurEffect.hpp` — BlurConfig uses designated Color init
- `src/UI/screens/PausePopupScreen.hpp` — BLUR_CONFIG uses designated Color init
- `src/Utils/render/Shader/ShaderModule.hpp` — Base config struct + module convention
- `src/Utils/render/Shader/BlurHModule.hpp` — Horizontal blur shader module
- `src/Utils/render/Shader/BlurVModule.hpp` — Vertical blur shader module
- `src/Utils/render/Shader/README.md` — How to create, use, and initialize shader modules
- Update `src/CMakeLists.txt` — Add Shader/ to include paths
- All singleton `instance()` methods — have `[[nodiscard]]`

### Definition of Done
- [ ] Build succeeds with zero warnings: `cmake --build build --config Release`
- [ ] Game launches and reaches MainMenuScreen
- [ ] Press ESC → PausePopupScreen with blur backdrop (visual verification)
- [ ] Press ESC → PausePopupScreen dismisses cleanly
- [ ] No raw `int`/`float` in event struct headers (grep verification)
- [ ] All singleton `instance()` methods have `[[nodiscard]]`
- [ ] Shader Module files exist in `src/Utils/render/Shader/`
- [ ] `App::init()` uses module-based shader names

### Must Have
- All 20 coding standard violations fixed
- Shader Module system with convention-only approach (no virtual dispatch)
- Module headers only include Core/Types.hpp and string_view
- README with shader module creation guide
- Pixel-identical blur effect output after reorganization

### Must NOT Have (Guardrails)
- NO virtual methods in Shader Module system (hot render path)
- NO ShaderManager.hpp or raylib.h includes in module headers (compile time)
- NO new Renderer methods (drawTextureRec bypass is a known exception)
- NO ShaderManager API redesign beyond extracting duplicated helper
- NO render pipeline abstractions, framebuffer objects, or pipeline state in modules
- NO FontManager parameter type changes (const std::string& is correct for map keys)
- NO changing const char* → string_view in contexts that need ownership

---

## Verification Strategy (MANDATORY)

> **ZERO HUMAN INTERVENTION** — ALL verification is agent-executed. No exceptions.

### Test Decision
- **Infrastructure exists**: NO
- **Automated tests**: None
- **Framework**: None (no test directory)
- **Agent-Executed QA**: ALWAYS (mandatory for all tasks)

### QA Policy
Every task MUST include agent-executed QA scenarios.
Evidence saved to `.sisyphus/evidence/task-{N}-{scenario-slug}.{ext}`.

- **Build verification**: `cmake --build build --config Release --target BiofuelGame`
- **Launch verification**: Run game, check for MainMenuScreen
- **Blur verification**: Run game with `BIOFUEL_DEV_STARTUP_PAUSE_POPUP=ON`, verify blur effect
- **Code verification**: PowerShell grep for specific patterns

---

## Execution Strategy

### Parallel Execution Waves

```
Wave 1 (Start Immediately - type fixes + noexcept/nodiscard):
├── Task 1: Fix event struct types (mouse, input, screen, animation) [quick]
├── Task 2: Fix App.hpp Config + main.cpp designated initializers [quick]
├── Task 3: Add [[nodiscard]] to all singleton instance() methods [quick]
├── Task 4: Add noexcept to Renderer and ShaderManager helpers [quick]
├── Task 5: Fix local constexpr → static constexpr in screen .cpp files [quick]
└── Task 6: Deduplicate ShaderManager load preamble [quick]

Wave 2 (After Wave 1 - shader module system):
├── Task 7: Create ShaderModule.hpp base config + convention [unspecified-high]
├── Task 8: Create BlurHModule.hpp [quick]
├── Task 9: Create BlurVModule.hpp [quick]
└── Task 10: Migrate EmbeddedShaders.hpp → module-embedded sources [unspecified-high]

Wave 3 (After Wave 2 - integration + verification):
├── Task 11: Update App.cpp to module-based shader init [quick]
├── Task 12: Update ScreenBlurEffect to use module references [quick]
├── Task 13: Update CMakeLists.txt for Shader/ directory [quick]
├── Task 14: Write Shader/README.md documentation [writing]
├── Task 15: Update Utils/render/README.md for new structure [writing]
└── Task 16: Fix BlurConfig designated initializers + verify Color struct [quick]

Wave FINAL (After ALL tasks — 4 parallel reviews):
├── Task F1: Plan compliance audit (oracle)
├── Task F2: Code quality review (unspecified-high)
├── Task F3: Real manual QA (unspecified-high + playwright)
└── Task F4: Scope fidelity check (deep)
→ Present results → Get explicit user okay

Critical Path: Wave 1 (all) → Wave 2 (T7 before T8/T9, T10 after T8/T9) → Wave 3 (T11/T12 after T7-T10) → FINAL
Parallel Speedup: ~50% faster than sequential
Max Concurrent: 6 (Wave 1)
```

### Dependency Matrix

| Task | Depends On | Blocks | Wave |
|------|-----------|--------|------|
| 1 | - | 16 | 1 |
| 2 | - | 16 | 1 |
| 3 | - | - | 1 |
| 4 | - | - | 1 |
| 5 | - | - | 1 |
| 6 | - | 11 | 1 |
| 7 | - | 8, 9, 10 | 2 |
| 8 | 7 | 10 | 2 |
| 9 | 7 | 10 | 2 |
| 10 | 8, 9 | 11 | 2 |
| 11 | 6, 10 | - | 3 |
| 12 | 10 | - | 3 |
| 13 | 7 | - | 3 |
| 14 | 7, 8, 9 | - | 3 |
| 15 | 7 | - | 3 |
| 16 | 1, 2 | - | 3 |
| F1 | all | - | FINAL |
| F2 | all | - | FINAL |
| F3 | all | - | FINAL |
| F4 | all | - | FINAL |

### Agent Dispatch Summary

- **Wave 1**: **6** — T1-T5 → `quick`, T6 → `quick`
- **Wave 2**: **4** — T7 → `unspecified-high`, T8 → `quick`, T9 → `quick`, T10 → `unspecified-high`
- **Wave 3**: **6** — T11-T13 → `quick`, T14-T15 → `writing`, T16 → `quick`
- **FINAL**: **4** — F1 → `oracle`, F2 → `unspecified-high`, F3 → `unspecified-high`, F4 → `deep`

---

## TODOs

- [x] 1. Fix event struct types (mouse, input, screen, animation)

  **What to do**:
  - In `MouseEvents.hpp`: Change all `float` fields to `f32`, change `int button` to `i32`, add `#include "Core/Types.hpp"`
  - In `InputEvents.hpp`: Change all `int key` fields to `i32`, add `#include "Core/Types.hpp"`
  - In `ScreenEvents.hpp`: Change all `int` fields to `i32`, add `#include "Core/Types.hpp"`
  - In `AnimationEvents.hpp`: Change `const char* screenName` to `std::string_view screenName`, verify `#include "Core/Types.hpp"` exists and add `#include <string_view>` if not present
  - In `InputSystem.cpp`: Add explicit casts at Raylib boundaries where `GetKeyPressed()` returns `int` and `Vector2` fields are `float` — use `static_cast<i32>()` and `static_cast<f32>()` where needed
  - Build and verify no compile errors

  **Must NOT do**:
  - Do NOT change Raylib function signatures or types
  - Do NOT change `FontManager::load()` parameter types
  - Do NOT add constructors to event structs (they use aggregate initialization)
  - Do NOT change event struct field names

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Mechanical type substitutions across 4-5 files, well-defined changes
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (with Tasks 2-6)
  - **Blocks**: Task 16 (BlurConfig fix needs events done first for consistency)
  - **Blocked By**: None

  **References**:
  - `src/Data/event/mouse/MouseEvents.hpp` — Raw `float`/`int` fields need project types
  - `src/Data/event/input/InputEvents.hpp` — Raw `int key` fields
  - `src/Data/event/screen/ScreenEvents.hpp` — Raw `int width/height` fields
  - `src/Data/event/animation/AnimationEvents.hpp` — `const char*` → `std::string_view`
  - `src/Core/Types.hpp` — Project type aliases (`i32`, `f32`, `u8`)
  - `src/Data/README.md` — Event coding standards (aggregate init, project types)

  **Why Each Reference Matters**:
  - MouseEvents/InputEvents/ScreenEvents: These are the files being changed
  - AnimationEvents: Already has Types.hpp but needs string_view
  - Core/Types.hpp: Reference for type alias names
  - Data/README.md: Documents event coding standards including "use project types"

  **Acceptance Criteria**:

  - [ ] `Select-String -Path "src\Data\event\**\*.hpp" -Pattern "\bint\b|\bfloat\b"` returns 0 matches (excluding comments and GLSL)
  - [ ] Build succeeds: `cmake --build build --config Release --target BiofuelGame`

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Event types use project aliases
    Tool: Bash (PowerShell)
    Preconditions: Build is clean
    Steps:
      1. Run `Select-String -Path "src\Data\event\mouse\MouseEvents.hpp" -Pattern "\bfloat\b|\bint\b"` — expect 0 matches
      2. Run `Select-String -Path "src\Data\event\input\InputEvents.hpp" -Pattern "\bint\b"` — expect 0 matches
      3. Run `Select-String -Path "src\Data\event\screen\ScreenEvents.hpp" -Pattern "\bint\b"` — expect 0 matches
      4. Run `Select-String -Path "src\Data\event\animation\AnimationEvents.hpp" -Pattern "const char\*"` — expect 0 matches
      5. Run `Select-String -Path "src\Data\event\**\*.hpp" -Pattern "#include.*Core/Types.hpp"` — expect 4 matches (one per file)
    Expected Result: All event struct fields use project types, all files include Core/Types.hpp
    Failure Indicators: Any match on raw int/float/const char* in event headers
    Evidence: .sisyphus/evidence/task-1-event-types.txt

  Scenario: Build succeeds after event type changes
    Tool: Bash
    Preconditions: Event files updated
    Steps:
      1. Run `cmake --build build --config Release --target BiofuelGame`
      2. Check exit code is 0
    Expected Result: Build completes with no errors
    Failure Indicators: Compilation errors about type mismatches in event trigger/handler sites
    Evidence: .sisyphus/evidence/task-1-build.txt
  ```

  **Commit**: YES (groups with Wave 1)
  - Message: `refactor(event-types): replace raw int/float with project types i32/f32 in event structs`
  - Files: `src/Data/event/mouse/MouseEvents.hpp`, `src/Data/event/input/InputEvents.hpp`, `src/Data/event/screen/ScreenEvents.hpp`, `src/Data/event/animation/AnimationEvents.hpp`, `src/Systems/Input/InputSystem.cpp`
  - Pre-commit: `cmake --build build --config Release --target BiofuelGame`

- [x] 2. Fix App::Config types and main.cpp designated initializers

  **What to do**:
  - In `src/Core/App.hpp`: Change `Config` struct fields from `int width/height/targetFps` to `i32 width/height/targetFps`
  - In `src/main.cpp`: Change positional member-by-member initialization to designated initializers:
    ```cpp
    biofuel::Application::Config config{
        .title = "Biofuel Game - Fuel Farm",
        .width = 1280,
        .height = 720,
        .targetFps = 60,
        .fullscreen = false,
        .resizable = true,
    };
    ```
  - Add `[[nodiscard]]` to `Application::run()` method declaration

  **Must NOT do**:
  - Do NOT change `Config.title` from `std::string` to `std::string_view` (title needs ownership)
  - Do NOT change the Config values (width, height, fps) — only the types
  - Do NOT add methods to Config

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Two files, simple type changes and style fix
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1
  - **Blocks**: Task 16
  - **Blocked By**: None

  **References**:
  - `src/Core/App.hpp` — Config struct with raw `int` fields
  - `src/main.cpp` — Positional Config initialization
  - `src/README.md` — Coding standard: "Designated initializers for clarity", "Always use project types"

  **Acceptance Criteria**:

  - [ ] `Select-String -Path "src\Core\App.hpp" -Pattern "\bint\b"` returns 0 matches in Config struct
  - [ ] `Select-String -Path "src\main.cpp" -Pattern "config\." - AND - NOT -Pattern "\."` shows designated initializer pattern
  - [ ] Build succeeds

  **QA Scenarios**:

  ```
  Scenario: Config uses project types and designated initializers
    Tool: Bash (PowerShell)
    Steps:
      1. Run `Select-String -Path "src\Core\App.hpp" -Pattern "int width"` — expect 0 matches
      2. Run `Select-String -Path "src\Core\App.hpp" -Pattern "i32 width"` — expect 1 match
      3. Run `Select-String -Path "src\main.cpp" -Pattern "\.title = "` — expect 1 match (designated init)
      4. Run `Select-String -Path "src\main.cpp" -Pattern "config\.width = "` — expect 0 matches (no positional)
    Expected Result: Config struct uses i32, main.cpp uses designated initializers
    Failure Indicators: Any raw `int` in Config, any positional member assignment
    Evidence: .sisyphus/evidence/task-2-config.txt

  Scenario: Build succeeds after Config changes
    Tool: Bash
    Steps:
      1. `cmake --build build --config Release --target BiofuelGame`
    Expected Result: Exit code 0
    Evidence: .sisyphus/evidence/task-2-build.txt
  ```

  **Commit**: YES (groups with Wave 1)
  - Message: `refactor(config): use i32 types and designated initializers in Application::Config`
  - Files: `src/Core/App.hpp`, `src/main.cpp`
  - Pre-commit: `cmake --build build --config Release --target BiofuelGame`

- [x] 3. Add [[nodiscard]] to all singleton instance() methods

  **What to do**:
  - In `EventManager.hpp`: Add `[[nodiscard]]` to `instance()` and `dispatcher()`
  - In `FontManager.hpp` (or `FontUtils.hpp`): Add `[[nodiscard]]` to `instance()`, `get()`, `has()`
  - In `ScreenManager.hpp`: Add `[[nodiscard]]` to `instance()`
  - In `AnimationManager.hpp`: Add `[[nodiscard]]` to `instance()`
  - In `ShaderManager.hpp`: Add `[[nodiscard]]` to `instance()`
  - Also add `[[nodiscard]]` to `Application::run()` in `App.hpp`

  **Must NOT do**:
  - Do NOT add `[[nodiscard]]` to void-returning methods
  - Do NOT change any method signatures or logic

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Simple attribute addition across 5-6 files
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1
  - **Blocks**: None
  - **Blocked By**: None

  **References**:
  - `src/Data/event/EventManager.hpp` — `instance()` method
  - `src/Utils/font/FontUtils.hpp` — `instance()`, `get()`, `has()` methods
  - `src/UI/ScreenManager.hpp` — `instance()` method
  - `src/AnimationController/AnimationManager.hpp` — `instance()` method
  - `src/Utils/render/ShaderManager.hpp` — `instance()` method (already has it on `get()` and `has()`)
  - `src/Core/App.hpp` — `run()` method
  - `src/README.md` — Coding standard: "[[nodiscard]] on All Value-Returning Functions"

  **Acceptance Criteria**:

  - [ ] All singleton `instance()` methods have `[[nodiscard]]`
  - [ ] `Application::run()` has `[[nodiscard]]`
  - [ ] Build succeeds

  **QA Scenarios**:

  ```
  Scenario: All singleton instance() methods have [[nodiscard]]
    Tool: Bash (PowerShell)
    Steps:
      1. For each file (EventManager, FontUtils, ScreenManager, AnimationManager, ShaderManager), run:
         `Select-String -Path "<file>" -Pattern "instance\(\)"`
         Verify the line BEFORE each match contains [[nodiscard]]
      2. `Select-String -Path "src\Core\App.hpp" -Pattern "int run\(\)"`
         Verify [[nodiscard]] is present
    Expected Result: All value-returning singleton methods have [[nodiscard]]
    Failure Indicators: Any instance() or run() without [[nodiscard]]
    Evidence: .sisyphus/evidence/task-3-nodiscard.txt

  Scenario: Build succeeds
    Tool: Bash
    Steps:
      1. `cmake --build build --config Release --target BiofuelGame`
    Expected Result: Exit code 0
    Evidence: .sisyphus/evidence/task-3-build.txt
  ```

  **Commit**: YES (groups with Wave 1)
  - Message: `refactor(nodiscard): add [[nodiscard]] to all singleton instance() and value-returning methods`
  - Files: EventManager.hpp, FontUtils.hpp, ScreenManager.hpp, AnimationManager.hpp, ShaderManager.hpp, App.hpp
  - Pre-commit: `cmake --build build --config Release --target BiofuelGame`

- [x] 4. Add noexcept to Renderer and ShaderManager helpers

  **What to do**:
  - In `Render.hpp`: Add `noexcept` to `screenWidth()` and `screenHeight()` declarations
  - In `Render.cpp`: Add `noexcept` to `screenWidth()` and `screenHeight()` definitions
  - In `ShaderManager.hpp`: Verify `setValue()` and `setValueTexture()` already have `noexcept` (they should). If not, add it.
  - In `ShaderManager.hpp`: Add `noexcept` to `getLocation()` if missing
  - Build and verify

  **Must NOT do**:
  - Do NOT add `noexcept` to methods that call Raylib functions that could theoretically allocate (like `LoadShader`, `LoadRenderTexture`)
  - Do NOT add `noexcept` to draw methods (they modify GPU state)
  - Do NOT change any method logic

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Simple attribute addition, well-defined
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1
  - **Blocks**: None
  - **Blocked By**: None

  **References**:
  - `src/Utils/render/Render.hpp` — `screenWidth()` and `screenHeight()` missing `noexcept`
  - `src/Utils/render/ShaderManager.hpp` — Static helpers to verify
  - `src/README.md` — Coding standard: "noexcept on All Accessors"

  **Acceptance Criteria**:
  - [ ] `screenWidth()` and `screenHeight()` have `noexcept` in both declaration and definition
  - [ ] Build succeeds

  **QA Scenarios**:

  ```
  Scenario: noexcept on accessor methods
    Tool: Bash (PowerShell)
    Steps:
      1. `Select-String -Path "src\Utils\render\Render.hpp" -Pattern "noexcept"` — expect matches on screenWidth and screenHeight lines
      2. `Select-String -Path "src\Utils\render\Render.cpp" -Pattern "noexcept"` — expect matches on screenWidth and screenHeight definitions
    Expected Result: Both declaration and definition have noexcept
    Evidence: .sisyphus/evidence/task-4-noexcept.txt

  Scenario: Build succeeds
    Tool: Bash
    Steps:
      1. `cmake --build build --config Release --target BiofuelGame`
    Expected Result: Exit code 0
    Evidence: .sisyphus/evidence/task-4-build.txt
  ```

  **Commit**: YES (groups with Wave 1)
  - Message: `refactor(noexcept): add noexcept to Renderer accessors and verify ShaderManager helpers`
  - Files: `src/Utils/render/Render.hpp`, `src/Utils/render/Render.cpp`
  - Pre-commit: `cmake --build build --config Release --target BiofuelGame`

- [x] 5. Fix local constexpr → static constexpr in screen .cpp files

  **What to do**:
  - In `src/UI/screens/PausePopupScreen.cpp`: Change `constexpr std::string_view title = "PAUSED"` to `static constexpr std::string_view title = "PAUSED"` (and similarly for `hint`)
  - In `src/UI/screens/MainMenuScreen.cpp`: Change all local `constexpr std::string_view` to `static constexpr std::string_view`
  - Build and verify

  **Must NOT do**:
  - Do NOT change class-level `static constexpr` members (those are already correct)
  - Do NOT change function-local `constexpr` that aren't `std::string_view`

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Simple keyword addition in 2 files
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1
  - **Blocks**: None
  - **Blocked By**: None

  **References**:
  - `src/UI/screens/PausePopupScreen.cpp` — Local constexpr string_views
  - `src/UI/screens/MainMenuScreen.cpp` — Local constexpr string_views
  - `src/README.md` — Coding standard: "constexpr for Compile-Time Constants"

  **Acceptance Criteria**:
  - [ ] No local `constexpr std::string_view` without `static` in screen .cpp files
  - [ ] Build succeeds

  **QA Scenarios**:

  ```
  Scenario: All function-local constexpr string_views are static
    Tool: Bash (PowerShell)
    Steps:
      1. `Select-String -Path "src\UI\screens\PausePopupScreen.cpp" -Pattern "(?<!static\s)constexpr std::string_view"` — expect 0 matches
      2. `Select-String -Path "src\UI\screens\MainMenuScreen.cpp" -Pattern "(?<!static\s)constexpr std::string_view"` — expect 0 matches
    Expected Result: All std::string_view constexpr are also static
    Evidence: .sisyphus/evidence/task-5-static-constexpr.txt

  Scenario: Build succeeds
    Tool: Bash
    Steps:
      1. `cmake --build build --config Release --target BiofuelGame`
    Expected Result: Exit code 0
    Evidence: .sisyphus/evidence/task-5-build.txt
  ```

  **Commit**: YES (groups with Wave 1)
  - Message: `refactor(constexpr): add static to function-local constexpr string_views`
  - Files: `src/UI/screens/PausePopupScreen.cpp`, `src/UI/screens/MainMenuScreen.cpp`
  - Pre-commit: `cmake --build build --config Release --target BiofuelGame`

- [x] 6. Deduplicate ShaderManager load preamble

  **What to do**:
  - In `ShaderManager.cpp`: Extract the duplicated "unload existing if present, then load" pattern from `load()` and `loadFromMemory()` into a private helper method like `unloadExisting(std::string_view name)`
  - The helper should: (1) find existing shader by name, (2) unload if valid, (3) erase from map
  - Both `load()` and `loadFromMemory()` call the helper, then proceed with their specific loading logic
  - Build and verify

  **Must NOT do**:
  - Do NOT redesign ShaderManager's map, API, or caching strategy
  - Do NOT change ShaderManager.hpp public API
  - Do NOT add reference counting or complex loading logic

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Simple extract method refactoring in one file
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1
  - **Blocks**: Task 11 (App.cpp shader init)
  - **Blocked By**: None

  **References**:
  - `src/Utils/render/ShaderManager.cpp` — Lines 36-46 and 63-73 are nearly identical
  - `src/Utils/render/ShaderManager.hpp` — Private section where helper should be declared

  **Why Each Reference Matters**:
  - ShaderManager.cpp: The duplicated code to extract
  - ShaderManager.hpp: Where to add the private helper declaration

  **Acceptance Criteria**:

  - [ ] `load()` and `loadFromMemory()` no longer contain duplicated unload logic
  - [ ] New private helper `unloadExisting()` exists
  - [ ] Build succeeds
  - [ ] Game runs and blur shaders still compile from memory

  **QA Scenarios**:

  ```
  Scenario: Duplicated load preamble is extracted
    Tool: Bash (PowerShell)
    Steps:
      1. `Select-String -Path "src\Utils\render\ShaderManager.cpp" -Pattern "UnloadShader"` — expect exactly 1 match (in the helper), not 2
    Expected Result: UnloadShader is called from only one location (the helper), not duplicated
    Failure Indicators: UnloadShader called in 2+ places
    Evidence: .sisyphus/evidence/task-6-dedup.txt

  Scenario: Shaders still compile from memory after refactoring
    Tool: Bash
    Steps:
      1. `cmake --build build --config Release --target BiofuelGame`
      2. Run game with `BIOFUEL_DEV_STARTUP_PAUSE_POPUP=ON`
      3. Verify PausePopupScreen appears with blur backdrop
    Expected Result: Game runs, blur renders correctly
    Failure Indicators: Game crashes, no blur effect, or shader compilation error in console
    Evidence: .sisyphus/evidence/task-6-shader-test.txt
  ```

  **Commit**: YES (groups with Wave 1)
  - Message: `refactor(shader): extract duplicated unload preamble in ShaderManager`
  - Files: `src/Utils/render/ShaderManager.hpp`, `src/Utils/render/ShaderManager.cpp`
  - Pre-commit: `cmake --build build --config Release --target BiofuelGame`

- [x] 7. Create ShaderModule.hpp base config + convention

  **What to do**:
  - Create `src/Utils/render/Shader/ShaderModule.hpp`
  - Define a `ShaderModuleConfig` struct with constexpr fields:
    ```cpp
    struct ShaderModuleConfig {
        std::string_view name;          // Shader name for ShaderManager lookup
        std::string_view fragmentSource; // GLSL fragment shader source (constexpr)
        const char* vertexSource = nullptr; // nullptr = use Raylib default vertex shader
    };
    ```
  - Define the convention (documented in comments) that each shader module:
    1. Provides a `static constexpr ShaderModuleConfig CONFIG` member
    2. Provides `static constexpr std::string_view NAME` (equals CONFIG.name)
    3. Provides cached uniform location getters if needed
    4. Does NOT include ShaderManager.hpp or raylib.h (compile-time priority)
    5. Does NOT use virtual methods (no inheritance, convention-only)
  - The convention is that modules are standalone classes with a consistent API, NOT inheriting from a base class
  - Module headers include ONLY `Core/Types.hpp` and `<string_view>`

  **Must NOT do**:
  - Do NOT create a virtual base class with virtual bind()/apply() methods
  - Do NOT include `<raylib.h>` or `ShaderManager.hpp` in this header
  - Do NOT add any .cpp file (header-only convention)
  - Do NOT add factory patterns or auto-registration

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: Design decision for new architecture, needs careful thought
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Must be first in Wave 2
  - **Blocks**: Tasks 8, 9, 10
  - **Blocked By**: None (Wave 1)

  **References**:
  - `src/Utils/render/ShaderManager.hpp` — Current API that modules will connect to
  - `src/Utils/render/EmbeddedShaders.hpp` — Current shader sources that will migrate
  - `src/Core/Types.hpp` — Project types (i32, f32, etc.)
  - `src/README.md` — Coding standards (constexpr, noexcept, project types, namespaces)
  - `src/Utils/README.md` — Utils rules (no cross-dependencies, header-only preference, no singletons except resource caches)

  **Why Each Reference Matters**:
  - ShaderManager.hpp: Modules interact with it at registration time, but NOT via includes
  - EmbeddedShaders.hpp: Source content will migrate to individual modules
  - Core/Types.hpp: Module must use project types
  - READMEs: Coding standards and organization rules

  **Acceptance Criteria**:

  - [ ] `src/Utils/render/Shader/ShaderModule.hpp` exists
  - [ ] Contains `ShaderModuleConfig` struct with `name`, `fragmentSource`, `vertexSource` fields
  - [ ] Does NOT include raylib.h or ShaderManager.hpp
  - [ ] Does NOT use virtual methods
  - [ ] Namespace is `biofuel::utils::render::shader`
  - [ ] Build succeeds (new file, no consumers yet)

  **QA Scenarios**:

  ```
  Scenario: ShaderModule.hpp follows all conventions
    Tool: Bash (PowerShell)
    Steps:
      1. `Test-Path "src\Utils\render\Shader\ShaderModule.hpp"` — expect True
      2. `Select-String -Path "src\Utils\render\Shader\ShaderModule.hpp" -Pattern "virtual"` — expect 0 matches
      3. `Select-String -Path "src\Utils\render\Shader\ShaderModule.hpp" -Pattern "raylib.h|ShaderManager"` — expect 0 matches
      4. `Select-String -Path "src\Utils\render\Shader\ShaderModule.hpp" -Pattern "namespace biofuel::utils::render::shader"` — expect 1 match
      5. `Select-String -Path "src\Utils\render\Shader\ShaderModule.hpp" -Pattern "ShaderModuleConfig"` — expect 1+ matches
      6. `cmake --build build --config Release --target BiofuelGame` — expect success
    Expected Result: File exists, follows convention, compiles
    Failure Indicators: Virtual methods found, raylib include found, wrong namespace
    Evidence: .sisyphus/evidence/task-7-module-convention.txt

  Scenario: Build succeeds with new file
    Tool: Bash
    Steps:
      1. `cmake --build build --config Release --target BiofuelGame`
    Expected Result: Exit code 0
    Evidence: .sisyphus/evidence/task-7-build.txt
  ```

  **Commit**: YES (groups with Wave 2)
  - Message: `feat(shader): create ShaderModule convention and config struct`
  - Files: `src/Utils/render/Shader/ShaderModule.hpp`
  - Pre-commit: `cmake --build build --config Release --target BiofuelGame`

- [x] 8. Create BlurHModule.hpp

  **What to do**:
  - Create `src/Utils/render/Shader/BlurHModule.hpp`
  - Move the `BLUR_H_FS` GLSL source from `EmbeddedShaders.hpp` into this module as `static constexpr std::string_view FRAGMENT_SOURCE`
  - Define the module class following the convention from Task 7:
    ```cpp
    namespace biofuel::utils::render::shader {
    class BlurHModule {
    public:
        static constexpr std::string_view NAME = "blur_h";
        static constexpr std::string_view FRAGMENT_SOURCE = R"(<GLSL source>)";
        static constexpr const char* VERTEX_SOURCE = nullptr; // Use Raylib default

        // Cached uniform locations (set during init)
        static constexpr std::string_view UNIFORM_TEXEL_SIZE = "texelSize";
        static constexpr std::string_view UNIFORM_BLUR_RADIUS = "blurRadius";
    };
    }
    ```
  - Include ONLY `Core/Types.hpp` and `<string_view>`
  - The GLSL source is the EXACT same content currently in `EmbeddedShaders.hpp` BLUR_H_FS

  **Must NOT do**:
  - Do NOT modify the GLSL source — it must produce pixel-identical output
  - Do NOT include ShaderManager.hpp or raylib.h
  - Do NOT add .cpp file (header-only)
  - Do NOT add virtual methods or inheritance

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Simple file creation following established convention
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES (with Task 9)
  - **Parallel Group**: Wave 2
  - **Blocks**: Task 10
  - **Blocked By**: Task 7

  **References**:
  - `src/Utils/render/EmbeddedShaders.hpp` — Current BLUR_H_FS source to migrate
  - `src/Utils/render/Shader/ShaderModule.hpp` — Convention and config struct to follow
  - `src/AnimationController/screen/ScreenBlurEffect.cpp` — Consumer that references "blur_h" by name

  **Why Each Reference Matters**:
  - EmbeddedShaders.hpp: Source of the GLSL code to move
  - ShaderModule.hpp: The convention this module must follow
  - ScreenBlurEffect.cpp: Will be updated to reference BlurHModule::NAME instead of "blur_h"

  **Acceptance Criteria**:

  - [ ] `src/Utils/render/Shader/BlurHModule.hpp` exists
  - [ ] Contains `NAME = "blur_h"` as constexpr string_view
  - [ ] Contains `FRAGMENT_SOURCE` with exact same GLSL as current BLUR_H_FS
  - [ ] Does NOT include ShaderManager.hpp or raylib.h
  - [ ] Build succeeds

  **QA Scenarios**:

  ```
  Scenario: BlurHModule follows convention and has correct GLSL source
    Tool: Bash (PowerShell)
    Steps:
      1. `Test-Path "src\Utils\render\Shader\BlurHModule.hpp"` — expect True
      2. `Select-String -Path "src\Utils\render\Shader\BlurHModule.hpp" -Pattern "raylib.h|ShaderManager"` — expect 0 matches
      3. `Select-String -Path "src\Utils\render\Shader\BlurHModule.hpp" -Pattern "NAME.*=.*blur_h"` — expect 1 match
      4. `Select-String -Path "src\Utils\render\Shader\BlurHModule.hpp" -Pattern "#version 330"` — expect 1 match (GLSL source preserved)
      5. `cmake --build build --config Release --target BiofuelGame` — expect success
    Expected Result: Module file exists, follows convention, contains correct GLSL
    Evidence: .sisyphus/evidence/task-8-blurhmodule.txt

  Scenario: Build succeeds
    Tool: Bash
    Steps:
      1. `cmake --build build --config Release --target BiofuelGame`
    Expected Result: Exit code 0
    Evidence: .sisyphus/evidence/task-8-build.txt
  ```

  **Commit**: YES (groups with Wave 2)
  - Message: `feat(shader): create BlurHModule with embedded GLSL source`
  - Files: `src/Utils/render/Shader/BlurHModule.hpp`
  - Pre-commit: `cmake --build build --config Release --target BiofuelGame`

- [x] 9. Create BlurVModule.hpp

  **What to do**:
  - Create `src/Utils/render/Shader/BlurVModule.hpp`
  - Move the `BLUR_V_FS` GLSL source from `EmbeddedShaders.hpp` into this module as `static constexpr std::string_view FRAGMENT_SOURCE`
  - Define the module class following the same convention as BlurHModule:
    ```cpp
    namespace biofuel::utils::render::shader {
    class BlurVModule {
    public:
        static constexpr std::string_view NAME = "blur_v";
        static constexpr std::string_view FRAGMENT_SOURCE = R"(<GLSL source>)";
        static constexpr const char* VERTEX_SOURCE = nullptr;

        static constexpr std::string_view UNIFORM_TEXEL_SIZE = "texelSize";
        static constexpr std::string_view UNIFORM_BLUR_RADIUS = "blurRadius";
    };
    }
    ```
  - Include ONLY `Core/Types.hpp` and `<string_view>`

  **Must NOT do**:
  - Do NOT modify the GLSL source
  - Do NOT include ShaderManager.hpp or raylib.h
  - Do NOT add .cpp file

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Mirror of Task 8
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES (with Task 8)
  - **Parallel Group**: Wave 2
  - **Blocks**: Task 10
  - **Blocked By**: Task 7

  **References**:
  - `src/Utils/render/EmbeddedShaders.hpp` — Current BLUR_V_FS source
  - `src/Utils/render/Shader/ShaderModule.hpp` — Convention
  - `src/AnimationController/screen/ScreenBlurEffect.cpp` — Consumer referencing "blur_v"

  **Acceptance Criteria**:
  - [ ] File exists with correct convention
  - [ ] GLSL source matches current BLUR_V_FS exactly
  - [ ] Build succeeds

  **QA Scenarios**:

  ```
  Scenario: BlurVModule follows convention and has correct GLSL source
    Tool: Bash (PowerShell)
    Steps:
      1. `Test-Path "src\Utils\render\Shader\BlurVModule.hpp"` — expect True
      2. `Select-String -Path "src\Utils\render\Shader\BlurVModule.hpp" -Pattern "raylib.h|ShaderManager"` — expect 0 matches
      3. `Select-String -Path "src\Utils\render\Shader\BlurVModule.hpp" -Pattern "NAME.*=.*blur_v"` — expect 1 match
      4. `Select-String -Path "src\Utils\render\Shader\BlurVModule.hpp" -Pattern "#version 330"` — expect 1 match
      5. `cmake --build build --config Release --target BiofuelGame` — expect success
    Expected Result: Module file exists, follows convention, contains correct GLSL
    Evidence: .sisyphus/evidence/task-9-blurvmodule.txt

  Scenario: Build succeeds
    Tool: Bash
    Steps:
      1. `cmake --build build --config Release --target BiofuelGame`
    Expected Result: Exit code 0
    Evidence: .sisyphus/evidence/task-9-build.txt
  ```

  **Commit**: YES (groups with Wave 2)
  - Message: `feat(shader): create BlurVModule with embedded GLSL source`
  - Files: `src/Utils/render/Shader/BlurVModule.hpp`
  - Pre-commit: `cmake --build build --config Release --target BiofuelGame`

- [x] 10. Migrate EmbeddedShaders.hpp → module-embedded sources

  **What to do**:
  - Update `EmbeddedShaders.hpp` to include the module headers instead of containing GLSL source directly:
    ```cpp
    // EmbeddedShaders.hpp - Now delegates to individual shader modules
    #include "Utils/render/Shader/BlurHModule.hpp"
    #include "Utils/render/Shader/BlurVModule.hpp"

    namespace biofuel::utils::render::embedded {
        // Backward compatibility aliases — prefer using BlurHModule::FRAGMENT_SOURCE directly
        inline constexpr std::string_view BLUR_H_FS = shader::BlurHModule::FRAGMENT_SOURCE;
        inline constexpr std::string_view BLUR_V_FS = shader::BlurVModule::FRAGMENT_SOURCE;
    }
    ```
  - This maintains backward compatibility: any existing code using `embedded::BLUR_H_FS` still works
  - But new code should use `shader::BlurHModule::FRAGMENT_SOURCE` directly
  - Build and verify blur shaders still compile from memory

  **Must NOT do**:
  - Do NOT delete EmbeddedShaders.hpp (backward compatibility aliases are kept)
  - Do NOT remove the `embedded` namespace aliases
  - Do NOT change any GLSL source content

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: Migration that affects existing code, needs careful compatibility
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Wave 2 (after Tasks 8, 9)
  - **Blocks**: Task 11
  - **Blocked By**: Tasks 8, 9

  **References**:
  - `src/Utils/render/EmbeddedShaders.hpp` — File being refactored
  - `src/Utils/render/Shader/BlurHModule.hpp` — New module (created in Task 8)
  - `src/Utils/render/Shader/BlurVModule.hpp` — New module (created in Task 9)
  - `src/Core/App.cpp` — Current consumer of `embedded::BLUR_H_FS`

  **Why Each Reference Matters**:
  - EmbeddedShaders.hpp: The file being refactored from source container to delegation layer
  - App.cpp: Must verify it still compiles and works after migration

  **Acceptance Criteria**:
  - [ ] `EmbeddedShaders.hpp` includes BlurHModule.hpp and BlurVModule.hpp
  - [ ] `embedded::BLUR_H_FS` and `embedded::BLUR_V_FS` aliases still work
  - [ ] Build succeeds
  - [ ] Game runs, blur still works

  **QA Scenarios**:

  ```
  Scenario: EmbeddedShaders delegates to modules
    Tool: Bash (PowerShell)
    Steps:
      1. `Select-String -Path "src\Utils\render\EmbeddedShaders.hpp" -Pattern "BlurHModule|BlurVModule"` — expect 2 matches
      2. `Select-String -Path "src\Utils\render\EmbeddedShaders.hpp" -Pattern "BLUR_H_FS.*=.*BlurHModule"` — expect 1 match
      3. `cmake --build build --config Release --target BiofuelGame` — expect success
    Expected Result: EmbeddedShaders delegates to modules, backward compat aliases work
    Failure Indicators: Build fails, GLSL source missing, alias compilation error
    Evidence: .sisyphus/evidence/task-10-migration.txt

  Scenario: Blur still works after migration
    Tool: Bash
    Steps:
      1. Build game: `cmake --build build --config Release --target BiofuelGame`
      2. Run game with `BIOFUEL_DEV_STARTUP_PAUSE_POPUP=ON`
      3. Verify PausePopupScreen appears with blur backdrop
      4. Press ESC to close, verify no black flash
    Expected Result: Blur effect works identically to before
    Failure Indicators: Game crash, no blur, shader compilation error
    Evidence: .sisyphus/evidence/task-10-blur-test.txt
  ```

  **Commit**: YES (groups with Wave 2)
  - Message: `refactor(shader): migrate EmbeddedShaders to module-based GLSL sources`
  - Files: `src/Utils/render/EmbeddedShaders.hpp`
  - Pre-commit: `cmake --build build --config Release --target BiofuelGame`

- [x] 11. Update App.cpp to module-based shader initialization

  **What to do**:
  - In `src/Core/App.cpp`: Replace the manual `loadFromMemory` calls with module-based registration:
    ```cpp
    #include "Utils/render/Shader/BlurHModule.hpp"
    #include "Utils/render/Shader/BlurVModule.hpp"

    // In init():
    using namespace utils::render::shader;
    utils::render::ShaderManager::instance().loadFromMemory(
        BlurHModule::NAME.data(), BlurHModule::VERTEX_SOURCE, BlurHModule::FRAGMENT_SOURCE.data());
    utils::render::ShaderManager::instance().loadFromMemory(
        BlurVModule::NAME.data(), BlurVModule::VERTEX_SOURCE, BlurVModule::FRAGMENT_SOURCE.data());
    ```
  - Remove the `#include "Utils/render/EmbeddedShaders.hpp"` and `using namespace utils::render::embedded` if no longer needed
  - Build and verify shaders still compile from memory

  **Must NOT do**:
  - Do NOT change the ShaderManager API
  - Do NOT add auto-registration magic
  - Do NOT remove the `using namespace` for other namespaces still in use

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Simple include and string replacement in one file
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Wave 3 (first, after Wave 2)
  - **Blocks**: None
  - **Blocked By**: Tasks 6, 10

  **References**:
  - `src/Core/App.cpp` — Current shader initialization code
  - `src/Utils/render/Shader/BlurHModule.hpp` — New module reference
  - `src/Utils/render/Shader/BlurVModule.hpp` — New module reference
  - `src/Utils/render/ShaderManager.hpp` — loadFromMemory API

  **Acceptance Criteria**:
  - [ ] App.cpp uses `BlurHModule::NAME` instead of raw string `"blur_h"`
  - [ ] App.cpp uses `BlurHModule::FRAGMENT_SOURCE.data()` instead of `BLUR_H_FS.data()`
  - [ ] `EmbeddedShaders.hpp` include removed from App.cpp (or kept if backward compat aliases are used elsewhere)
  - [ ] Build succeeds, game runs, blur works

  **QA Scenarios**:

  ```
  Scenario: App.cpp uses module-based shader names
    Tool: Bash (PowerShell)
    Steps:
      1. `Select-String -Path "src\Core\App.cpp" -Pattern "BlurHModule|BlurVModule"` — expect 2+ matches
      2. `Select-String -Path "src\Core\App.cpp" -Pattern '"blur_h"|"blur_v"'` — expect 0 raw string matches
      3. Build and run: `cmake --build build --config Release && run with BIOFUEL_DEV_STARTUP_PAUSE_POPUP=ON`
    Expected Result: Module names used, no raw strings, blur works
    Failure Indicators: Raw string names still present, build fails, blur doesn't work
    Evidence: .sisyphus/evidence/task-11-app-module-init.txt

  Scenario: Blur still works after App.cpp changes
    Tool: Bash
    Steps:
      1. Build game
      2. Run with `BIOFUEL_DEV_STARTUP_PAUSE_POPUP=ON`
      3. Verify pause popup appears with blur backdrop
    Expected Result: Pixel-identical blur output
    Evidence: .sisyphus/evidence/task-11-blur-test.txt
  ```

  **Commit**: YES (groups with Wave 3)
  - Message: `refactor(shader): update App.cpp to use module-based shader initialization`
  - Files: `src/Core/App.cpp`
  - Pre-commit: `cmake --build build --config Release --target BiofuelGame`

- [x] 12. Update ScreenBlurEffect to use module references

  **What to do**:
  - In `src/AnimationController/screen/ScreenBlurEffect.cpp`: Replace raw shader name strings with module references:
    ```cpp
    #include "Utils/render/Shader/BlurHModule.hpp"
    #include "Utils/render/Shader/BlurVModule.hpp"

    // Replace: shaderMgr.get("blur_h")
    // With:     shaderMgr.get(BlurHModule::NAME.data())

    // Replace: shaderMgr.has("blur_h")
    // With:     shaderMgr.has(BlurHModule::NAME.data())
    ```
  - Update the uniform name strings to use module constants:
    ```cpp
    // Replace: getLocation(blurH, "texelSize")
    // With:     getLocation(blurH, BlurHModule::UNIFORM_TEXEL_SIZE.data())

    // Replace: getLocation(blurH, "blurRadius")
    // With:     getLocation(blurH, BlurHModule::UNIFORM_BLUR_RADIUS.data())
    ```
  - Same for blur_v
  - Build and verify blur still works

  **Must NOT do**:
  - Do NOT change ScreenBlurEffect.hpp (only .cpp changes)
  - Do NOT change the rendering logic
  - Do NOT add any new includes to the .hpp file

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: String replacement in one .cpp file
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES (with Tasks 11, 13-16)
  - **Parallel Group**: Wave 3
  - **Blocks**: None
  - **Blocked By**: Task 10

  **References**:
  - `src/AnimationController/screen/ScreenBlurEffect.cpp` — Consumer to update
  - `src/Utils/render/Shader/BlurHModule.hpp` — Module with NAME and uniform constants
  - `src/Utils/render/Shader/BlurVModule.hpp` — Module with NAME and uniform constants

  **Acceptance Criteria**:
  - [ ] No raw shader name strings in ScreenBlurEffect.cpp
  - [ ] No raw uniform name strings in ScreenBlurEffect.cpp
  - [ ] Build succeeds, blur works identically

  **QA Scenarios**:

  ```
  Scenario: ScreenBlurEffect uses module references
    Tool: Bash (PowerShell)
    Steps:
      1. `Select-String -Path "src\AnimationController\screen\ScreenBlurEffect.cpp" -Pattern '"blur_h"|"blur_v"'` — expect 0 matches
      2. `Select-String -Path "src\AnimationController\screen\ScreenBlurEffect.cpp" -Pattern '"texelSize"|"blurRadius"'` — expect 0 matches
      3. `Select-String -Path "src\AnimationController\screen\ScreenBlurEffect.cpp" -Pattern "BlurHModule|BlurVModule"` — expect 2+ matches
    Expected Result: All shader/uniform names reference module constants
    Failure Indicators: Any raw string shader name or uniform name remaining
    Evidence: .sisyphus/evidence/task-12-module-refs.txt

  Scenario: Build and blur test
    Tool: Bash
    Steps:
      1. `cmake --build build --config Release --target BiofuelGame`
      2. Run game, test blur
    Expected Result: Build succeeds, blur works
    Evidence: .sisyphus/evidence/task-12-build.txt
  ```

  **Commit**: YES (groups with Wave 3)
  - Message: `refactor(shader): update ScreenBlurEffect to use module references`
  - Files: `src/AnimationController/screen/ScreenBlurEffect.cpp`
  - Pre-commit: `cmake --build build --config Release --target BiofuelGame`

- [x] 13. Update CMakeLists.txt for Shader/ directory

  **What to do**:
  - In `src/CMakeLists.txt`: Add `Utils/render/Shader` to the `target_include_directories` list
  - Verify GLOB_RECURSE picks up the new .hpp files automatically
  - Build and verify the new files are found

  **Must NOT do**:
  - Do NOT change the GLOB_RECURSE pattern
  - Do NOT add individual file references (GLOB_RECURSE handles this)
  - Do NOT modify any other build settings

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Simple CMake include path addition
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES (with Tasks 11, 12, 14-16)
  - **Parallel Group**: Wave 3
  - **Blocks**: None
  - **Blocked By**: Task 7

  **References**:
  - `src/CMakeLists.txt` — Current build configuration with include directories

  **Acceptance Criteria**:
  - [ ] `Utils/render/Shader` appears in include directories
  - [ ] Build finds new module .hpp files
  - [ ] Game still builds and runs

  **QA Scenarios**:

  ```
  Scenario: CMake includes Shader directory
    Tool: Bash (PowerShell)
    Steps:
      1. `Select-String -Path "src\CMakeLists.txt" -Pattern "Shader"` — expect 1+ match
      2. `cmake --build build --config Release --target BiofuelGame` — expect success
    Expected Result: Shader directory in include paths, build succeeds
    Evidence: .sisyphus/evidence/task-13-cmake.txt
  ```

  **Commit**: YES (groups with Wave 3)
  - Message: `build(shader): add Shader module directory to CMake include paths`
  - Files: `src/CMakeLists.txt`
  - Pre-commit: `cmake --build build --config Release --target BiofuelGame`

- [x] 14. Write Shader/README.md documentation

  **What to do**:
  - Create `src/Utils/render/Shader/README.md`
  - Document: (1) How to create a new Shader Module, (2) How to use existing modules, (3) How to initialize modules in App::init(), (4) The module convention (no virtual, no raylib includes, no ShaderManager includes), (5) Directory structure, (6) Examples with code
  - Follow the style of existing READMEs in the project (see `src/Utils/render/README.md`, `src/AnimationController/README.md`)
  - Include a step-by-step "Creating a New Shader Module" guide with a concrete example (e.g., a hypothetical vignette shader)
  - Include a "Using a Shader Module" guide showing how to reference a module in consumer code
  - Include a "Initializing Shader Modules" guide showing App::init() registration

  **Must NOT do**:
  - Do NOT add implementation code — this is documentation only
  - Do NOT create hypothetical shader files — use examples only

  **Recommended Agent Profile**:
  - **Category**: `writing`
    - Reason: Documentation-focused task
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 3
  - **Blocks**: None
  - **Blocked By**: Tasks 7, 8, 9 (need to exist for reference)

  **References**:
  - `src/Utils/render/README.md` — Existing documentation style
  - `src/Utils/render/Shader/ShaderModule.hpp` — The convention being documented
  - `src/Utils/render/Shader/BlurHModule.hpp` — Example module
  - `src/Utils/render/Shader/BlurVModule.hpp` — Example module
  - `src/Core/App.cpp` — Module-based initialization example

  **Acceptance Criteria**:
  - [ ] README.md exists in `src/Utils/render/Shader/`
  - [ ] Contains "Creating a New Shader Module" section
  - [ ] Contains "Using a Shader Module" section
  - [ ] Contains "Initializing Shader Modules" section
  - [ ] Contains directory structure diagram

  **QA Scenarios**:

  ```
  Scenario: README exists and covers all sections
    Tool: Bash (PowerShell)
    Steps:
      1. `Test-Path "src\Utils\render\Shader\README.md"` — expect True
      2. `Select-String -Path "src\Utils\render\Shader\README.md" -Pattern "Creating"` — expect 1+ matches
      3. `Select-String -Path "src\Utils\render\Shader\README.md" -Pattern "Using"` — expect 1+ matches
      4. `Select-String -Path "src\Utils\render\Shader\README.md" -Pattern "Initializing"` — expect 1+ matches
    Expected Result: README covers all required sections
    Evidence: .sisyphus/evidence/task-14-readme.txt
  ```

  **Commit**: YES (groups with Wave 3)
  - Message: `docs(shader): add Shader/README.md with module creation and usage guide`
  - Files: `src/Utils/render/Shader/README.md`
  - Pre-commit: None (documentation only)

- [x] 15. Update Utils/render/README.md for new structure

  **What to do**:
  - Update `src/Utils/render/README.md` to include the new `Shader/` subdirectory in the architecture diagram
  - Add a section about the Shader Module system
  - Update the "Adding a New Shader" instructions to point to the module system instead of EmbeddedShaders.hpp
  - Keep all existing content about Renderer, ShaderManager, and EmbeddedShaders (which now delegates to modules)

  **Must NOT do**:
  - Do NOT remove existing content about Renderer or ShaderManager
  - Do NOT change the coding standards section

  **Recommended Agent Profile**:
  - **Category**: `writing`
    - Reason: Documentation update
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 3
  - **Blocks**: None
  - **Blocked By**: Task 7

  **References**:
  - `src/Utils/render/README.md` — Current documentation to update
  - `src/Utils/render/Shader/README.md` — New README for cross-reference
  - `src/Utils/render/Shader/ShaderModule.hpp` — New architecture to document

  **Acceptance Criteria**:
  - [ ] README.md updated with Shader/ subdirectory in architecture
  - [ ] "Adding a New Shader" section points to module system
  - [ ] All existing content preserved

  **QA Scenarios**:

  ```
  Scenario: README updated with Shader section
    Tool: Bash (PowerShell)
    Steps:
      1. `Select-String -Path "src\Utils\render\README.md" -Pattern "Shader"` — expect 3+ matches
      2. `Select-String -Path "src\Utils\render\README.md" -Pattern "Module"` — expect 2+ matches
      3. `Select-String -Path "src\Utils\render\README.md" -Pattern "Renderer"` — expect existing content preserved
    Expected Result: README has new Shader Module section and preserves existing content
    Evidence: .sisyphus/evidence/task-15-readme-update.txt
  ```

  **Commit**: YES (groups with Wave 3)
  - Message: `docs(render): update README with Shader Module system architecture`
  - Files: `src/Utils/render/README.md`
  - Pre-commit: None (documentation only)

- [x] 16. Fix BlurConfig designated initializers and verify Color struct

  **What to do**:
  - In `src/AnimationController/screen/ScreenBlurEffect.hpp`: Change BlurConfig default initialization:
    ```cpp
    // FROM:
    Color tintColor = {15, 15, 25, 0};
    // TO:
    Color tintColor = {.r = 15, .g = 15, .b = 25, .a = 0};
    ```
  - In `src/UI/screens/PausePopupScreen.hpp`: Change BLUR_CONFIG initialization:
    ```cpp
    // FROM:
    .tintColor = {15, 15, 25, 0},
    // TO:
    .tintColor = {.r = 15, .g = 15, .b = 25, .a = 0},
    ```
  - In `src/Utils/font/FontUtils.hpp`: Change `int baseSize` to `i32 baseSize`
  - Build and verify — this tests that C++20 designated initializers work with Raylib's Color struct on MSVC

  **Must NOT do**:
  - Do NOT change any visual values (colors, alphas, etc.)
  - Do NOT change BlurConfig semantics
  - Do NOT proceed if designated initializers don't compile — fall back to positional init with a comment explaining why

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Style fix in 3 files, plus type fix in 1 file
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES (with Tasks 11-15)
  - **Parallel Group**: Wave 3
  - **Blocks**: None
  - **Blocked By**: Task 1 (event types must be consistent)

  **References**:
  - `src/AnimationController/screen/ScreenBlurEffect.hpp` — BlurConfig with positional Color init
  - `src/UI/screens/PausePopupScreen.hpp` — BLUR_CONFIG with positional Color init
  - `src/Utils/font/FontUtils.hpp` — Raw `int baseSize` parameter
  - `src/README.md` — Coding standard: "Designated initializers for clarity"

  **Acceptance Criteria**:
  - [ ] BlurConfig uses designated Color initializer `{.r = 15, .g = 15, .b = 25, .a = 0}`
  - [ ] FontUtils.hpp uses `i32 baseSize` instead of `int baseSize`
  - [ ] Build succeeds on MSVC (verifies C++20 designated init with Raylib Color)
  - [ ] Game runs, blur still works with same alpha values

  **QA Scenarios**:

  ```
  Scenario: Designated initializers compile on MSVC
    Tool: Bash
    Steps:
      1. `cmake --build build --config Release --target BiofuelGame`
      2. Verify exit code 0
      3. `Select-String -Path "src\AnimationController\screen\ScreenBlurEffect.hpp" -Pattern "\.r = 15"` — expect 1 match
      4. `Select-String -Path "src\UI\screens\PausePopupScreen.hpp" -Pattern "\.r = 15"` — expect 1 match
      5. `Select-String -Path "src\Utils\font\FontUtils.hpp" -Pattern "i32 baseSize"` — expect 1 match
    Expected Result: Build succeeds, designated init works with Raylib Color
    Failure Indicators: Build fails (MSVC doesn't support designated init for C structs — fall back to positional)
    Evidence: .sisyphus/evidence/task-16-designated-init.txt

  Scenario: Blur alpha values unchanged
    Tool: Bash
    Steps:
      1. Build and run game with `BIOFUEL_DEV_STARTUP_PAUSE_POPUP=ON`
      2. Verify pause popup blur looks identical to before
    Expected Result: Pixel-identical visual output
    Evidence: .sisyphus/evidence/task-16-blur-test.txt
  ```

  **Commit**: YES (groups with Wave 3)
  - Message: `refactor(style): use designated initializers for Color and i32 for FontUtils`
  - Files: `src/AnimationController/screen/ScreenBlurEffect.hpp`, `src/UI/screens/PausePopupScreen.hpp`, `src/Utils/font/FontUtils.hpp`, `src/Utils/font/FontUtils.cpp`
  - Pre-commit: `cmake --build build --config Release --target BiofuelGame`

---

## Final Verification Wave (MANDATORY — after ALL implementation tasks)

> 4 review agents run in PARALLEL. ALL must APPROVE. Present consolidated results to user and get explicit "okay" before completing.

- [x] F1. **Plan Compliance Audit** — `oracle`
  Read the plan end-to-end. For each "Must Have": verify implementation exists (read file, grep pattern, run command). For each "Must NOT Have": search codebase for forbidden patterns — reject with file:line if found. Check evidence files exist in .sisyphus/evidence/. Compare deliverables against plan.
  Output: `Must Have [N/N] | Must NOT Have [N/N] | Tasks [N/N] | VERDICT: APPROVE/REJECT`

- [x] F2. **Code Quality Review** — `unspecified-high`
  Run `cmake --build build --config Release --target BiofuelGame`. Check for warnings. Review all changed files for: `as any`/`@ts-ignore`, empty catches, console.log in prod, commented-out code, unused imports. Check coding standards: raw int/float in event structs, missing [[nodiscard]] on singletons, missing noexcept on Renderer helpers. Verify Shader Module files follow convention-only pattern (no virtual methods). Verify module headers don't include ShaderManager.hpp or raylib.h.
  Output: `Build [PASS/FAIL] | Standards [N/N pass] | Modules [N/N correct] | VERDICT`

- [x] F3. **Real Manual QA** — `unspecified-high` (+ `playwright` skill)
  Build and run the game. Verify: (1) MainMenuScreen renders, (2) ESC → PausePopupScreen appears with blur backdrop, (3) ESC → PausePopupScreen dismisses cleanly, (4) No visual artifacts, (5) Shader Module files exist. Screenshot evidence for each step.
  Output: `Scenarios [N/N pass] | Visual [CLEAN/ISSUES] | VERDICT`

- [x] F4. **Scope Fidelity Check** — `deep`
  For each task: read "What to do", read actual diff. Verify 1:1 — everything in spec was built, nothing beyond spec was built. Check "Must NOT do" compliance. Detect cross-task contamination. Flag unaccounted changes.
  Output: `Tasks [N/N compliant] | Contamination [CLEAN/N issues] | Unaccounted [CLEAN/N files] | VERDICT`

---

## Commit Strategy

- **Wave 1**: `refactor(event-types): fix coding standard violations across event structs and singletons` - Event files, App.hpp, Renderer, ShaderManager, screen .cpp files
- **Wave 2**: `feat(shader): add Module-based shader system with BlurH and BlurV modules` - Shader/ directory, EmbeddedShaders update
- **Wave 3**: `refactor(shader): integrate modules into App init and ScreenBlurEffect, add docs` - App.cpp, ScreenBlurEffect, CMakeLists, READMEs

---

## Success Criteria

### Verification Commands
```powershell
# Build succeeds with no warnings
cmake --build build --config Release --target BiofuelGame
# Expected: Build succeeds, 0 warnings

# No raw types in event structs
Select-String -Path "src\Data\event\**\*.hpp" -Pattern "\bint\b|\bfloat\b" | Where-Object { $_.Line -notmatch "//" }
# Expected: 0 matches

# All singleton instance() methods have [[nodiscard]]
Select-String -Path "src\**\*.hpp" -Pattern "static \w+& instance\(\)" | ForEach-Object { $_.LineNumber; $_.Line }
# Expected: Each result's preceding line contains [[nodiscard]]

# Shader Module files exist
Test-Path "src\Utils\render\Shader\ShaderModule.hpp"
Test-Path "src\Utils\render\Shader\BlurHModule.hpp"
Test-Path "src\Utils\render\Shader\BlurVModule.hpp"
Test-Path "src\Utils\render\Shader\README.md"
# Expected: All True

# App.cpp uses module names
Select-String -Path "src\Core\App.cpp" -Pattern "BlurHModule|BlurVModule"
# Expected: Matches found
```

### Final Checklist
- [x] All "Must Have" present
- [x] All "Must NOT Have" absent
- [x] Build succeeds with zero warnings
- [x] Game runs and blur effect works identically