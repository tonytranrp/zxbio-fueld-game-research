# Plan: Real Task-Based Loading Screen System

## Goal

Replace the current single monolithic loading task with a 13-task granular startup pipeline where every task name reflects actual initialization work happening at that moment. Move all init from `App::init()` into `LoadingScreen` tasks so the loading bar covers the full startup experience.

## Files Changed

| File | Change |
|------|--------|
| `src/Core/App.cpp` | Strip `init()` to window creation only; pass config values to LoadingScreen |
| `src/UI/screens/LoadingScreen.hpp` | Add constructor params + config members; add `DOTS_INTERVAL` constant |
| `src/UI/screens/LoadingScreen.cpp` | Rewrite `buildTasks()` with 13 real tasks; merge dots into task name in `onRender()`; remove separate "Loading" line |
| `src/UI/ScreenManager.hpp` | Add `preloadCrossfadeShader()` and `preloadTransitionTextures()` |
| `src/UI/ScreenManager.cpp` | Implement both preload methods (delegate to existing private `ensure*`) |

No files created. No files deleted.

---

## Task Breakdown (13 real tasks)

### Window & Input (3 tasks)
| # | Name | Weight | Code |
|---|------|--------|------|
| 1 | `Configuring input...` | 0.3 | `SetExitKey(KEY_NULL)` |
| 2 | `Setting window constraints...` | 0.3 | `SetWindowMinSize(width, height)` |
| 3 | `Setting target framerate...` | 0.3 | `SetTargetFPS(targetFps)` |

### Core Systems (4 tasks)
| # | Name | Weight | Code |
|---|------|--------|------|
| 4 | `Initializing event bus...` | 0.5 | `Data::events().init()` |
| 5 | `Initializing screen stack...` | 0.5 | `Data::screens().init()` |
| 6 | `Initializing animation system...` | 0.5 | `AnimationManager::instance().init()` |
| 7 | `Initializing shader system...` | 0.3 | `ShaderManager::instance().init()` |

### Shader Compilation (4 tasks)
| # | Name | Weight | Code |
|---|------|--------|------|
| 8 | `Compiling blur horizontal shader...` | 2.0 | `sm.loadFromMemory("blur_h", ...)` |
| 9 | `Compiling blur vertical shader...` | 2.0 | `sm.loadFromMemory("blur_v", ...)` |
| 10 | `Compiling crossfade shader...` | 2.0 | `sm.loadFromMemory("crossfade", ...)` |
| 11 | `Compiling background shader...` | 2.0 | `sm.loadFromMemory("mainmenu_bg", ...)` |

### Crossfade Preload (2 tasks)
| # | Name | Weight | Code |
|---|------|--------|------|
| 12 | `Caching transition shader...` | 1.0 | `Data::screens().preloadCrossfadeShader()` |
| 13 | `Allocating render buffers...` | 1.5 | `Data::screens().preloadTransitionTextures()` |

**Total weight: 13.5** — shader compiles dominate, which is realistic.

---

## Detailed Changes

### 1. `src/Core/App.cpp` — Strip `init()` to bare minimum

**Remove:** SetExitKey, SetWindowMinSize, ToggleFullscreen block, SetTargetFPS, EventManager::init, ScreenManager::init, AnimationManager::init, ShaderManager::init

**Keep:** SetConfigFlags + InitWindow + m_initialized/m_running

**Change push line to:** `Data::screens().push(std::make_unique<ui::screens::LoadingScreen>(m_config.width, m_config.height, m_config.targetFps, m_config.fullscreen));`

### 2. `src/UI/screens/LoadingScreen.hpp` — Constructor + members

Add constructor `LoadingScreen(i32 width, i32 height, i32 targetFps, bool fullscreen)`. Add members `m_appWidth`, `m_appHeight`, `m_appTargetFps`, `m_appFullscreen`. Add `DOTS_INTERVAL = 0.4f`.

### 3. `src/UI/screens/LoadingScreen.cpp` — 13 tasks + integrated dots

`buildTasks()` populates all 13 tasks. `onRender()` merges animated dots into task name: `status = m_tasks.currentName() + std::string(dotCount, '.')`. Remove separate "Loading" dots block.

### 4-5. `src/UI/ScreenManager.hpp/.cpp` — Public preload methods

Add `preloadCrossfadeShader()` and `preloadTransitionTextures()` — thin public wrappers around existing private `ensure*` methods.

---

## Verification

1. Loading screen shows `"Configuring input..."` immediately after window opens
2. Task names rotate through all 13 tasks (one per frame)
3. Progress bar moves smoothly through all weight increments
4. Dots cycle on each task name
5. All complete → transition to MainMenuScreen
6. Press any key after tasks done → skip remaining timer
7. No separate "Loading" text — only task name with dots
