# Learnings

## 2026-05-08 Session Start
- Build system: MSVC 19.50, CMake 3.20+, Visual Studio 2026 Community
- CPM with tarballs (not git) for dependency fetching
- GLOB_RECURSE in src/CMakeLists.txt auto-discovers sources
- Executable: build/bin/Release/BiofuelGame.exe (~538KB)
- Raylib 5.5, entt 3.14.0, nlohmann_json, taskflow 3.7.0, spdlog 1.14.1
- Namespace convention: biofuel::utils::render, biofuel::utils::event, biofuel::event::screen, biofuel::event::input
- Type aliases in Core/Types.hpp: f32, i32, etc.
- EventManager is Meyers singleton with init/shutdown lifecycle
- Data.hpp is the bridge: Data::events(), Data::eventBus()
- App.cpp currently does ALL rendering inline in render() method
- Game.hpp/cpp is orphaned — never included by App, never instantiated

## 2026-05-08 Wave 2: App Refactor + MainMenuScreen
- App::init(): calls Data::screens().init() then push(MainMenuScreen) after Data::events().init()
- App::shutdown(): calls Data::screens().shutdown() BEFORE events shutdown
- App::update(): delegates to Data::screens().update(dt)
- App::processInput(): delegates to Data::screens().handleInput()
- App::render(): beginFrame(BLACK), screens().render(), FPS counter, endFrame() - 10 lines total
- MainMenuScreen onRender() is IDENTICAL to old App::render() inline code (same text, positions, colors, font sizes)
- Data::screens() follows exact same singleton pattern as Data::events()
- Screen existing methods are called directly (not via front() shenanigans) because ScreenManager::update/render/handleInput delegate properly
- CMake GLOB_RECURSE with CONFIGURE_DEPENDS auto-detected new files in UI/screens/
- Build output: BiofuelGame.exe in build/bin/Release/

## 2026-05-08 Wave 3: ScreenManager Transition Enhancement
- isTransitioning() iterates all screens — any screen with non-None TransitionState blocks new ops
- pop() now sets TransitionOut instead of immediate removal; onExit/onResume deferred to update()
- replace() pauses old top + sets TransitionOut, pushes new with TransitionIn; no onExit/onResume called immediately
- update() handles TransitionOut completion (clamp progress, keep state for needsRemoval) AND TransitionIn completion (set to None)
- Removal loop: manually iterates (not erase-remove) to call onExit() before erase and detect wasTop for onResume
- wasTop check: compare raw pointer before erase — only true in pop() case (transitioning screen is back()), false in replace() (new screen is back())
- render() fade overlay already correct — TransitionIn alpha=1-progress, TransitionOut alpha=progress — no changes needed
- clear() bypasses transitions — immediate onExit+pop_back loop unchanged
- clangd diagnostics are false positives (missing MSVC include paths); MSVC build is ground truth
