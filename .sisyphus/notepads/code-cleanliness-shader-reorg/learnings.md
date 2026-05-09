# Learnings

## 2026-05-09 Session Start
- Plan: code-cleanliness-shader-reorg
- 16 implementation tasks + 4 final verification tasks
- Wave 1: 6 parallel quick tasks (type fixes, noexcept, nodiscard, constexpr, dedup)
- Wave 2: 4 tasks (shader module system) - T7 must go first
- Wave 3: 6 tasks (integration, docs, designated init fix)
- Key decisions: Convention-only shader modules (no virtual), headers include only Core/Types.hpp + string_view

## 2026-05-10 nodiscard Quick Fix
- `EventManager.hpp`: Added `[[nodiscard]]` to `instance()` — `dispatcher()` already had it
- `FontUtils.hpp`: Added `[[nodiscard]]` to `instance()` — `get()` and `has()` already had it
- `ScreenManager.hpp`: Already had `[[nodiscard]]` on `instance()` — no change needed
- `AnimationManager.hpp`: Already had `[[nodiscard]]` on `instance()` — no change needed
- `ShaderManager.hpp`: Already had `[[nodiscard]]` on `instance()` — no change needed
- Build succeeded: `BiofuelGame.exe` compiled cleanly
- Pattern: Most singletons already had `[[nodiscard]]` on `instance()`; only `EventManager` and `FontManager` needed the fix

## 2026-05-10 noexcept for screenWidth/screenHeight
- `Render.hpp`: Added `noexcept` to `screenWidth()` and `screenHeight()` declarations
- `Render.cpp`: Added `noexcept` to both definitions
- `ShaderManager.hpp`: Verified `get()`, `has()`, `getLocation()`, `setValue()`, `setValueTexture()` already have `noexcept` — no changes needed
- Build succeeded: `BiofuelGame.exe` compiled cleanly
- Pattern: `screenWidth()` and `screenHeight()` are pure accessors (just call Raylib's getter), correctly marked `noexcept`

## 2026-05-10 ShaderModule.hpp Foundation
- Created src/Utils/render/Shader/ShaderModule.hpp � base config struct + convention docs
- ShaderModuleConfig struct: 
ame (string_view), ragmentSource (string_view), ertexSource (const char*, default nullptr)
- Includes only Core/Types.hpp and <string_view> � no raylib, no ShaderManager
- No virtual methods, no inheritance � convention-only approach
- Detailed block comments explain: why convention (not base class), what each module provides (NAME, FRAGMENT_SOURCE, VERTEX_SOURCE, CONFIG, uniform constants), how registration works (explicit in App::init())
- New directory src/Utils/render/Shader/ created for module files
- Build verified: BiofuelGame.exe compiled cleanly

## 2026-05-10 BlurVModule.hpp (T9)
- Created src/Utils/render/Shader/BlurVModule.hpp — vertical Gaussian blur shader module
- NAME = "blur_v", FRAGMENT_SOURCE is character-identical to BLUR_V_FS in EmbeddedShaders.hpp
- VERTEX_SOURCE = nullptr, CONFIG uses designated initializers
- UNIFORM_TEXEL_SIZE = "texelSize", UNIFORM_BLUR_RADIUS = "blurRadius"
- Includes only ShaderModule.hpp (which includes Core/Types.hpp + string_view)
- Header-only, no .cpp file, no virtual methods or inheritance
- Build verified: BiofuelGame.exe compiled cleanly

## 2026-05-10 Wave 3 Integration + Namespace Fix
- T11-T16 all delegated and reported successful, but ScreenBlurEffect.cpp had a build error
- Root cause: `using namespace utils::render::shader;` inside `namespace biofuel::animation::screen` doesn't resolve correctly — `utils` doesn't resolve to `biofuel::utils` from within a nested namespace
- Fix: Removed the `using namespace` directive and used fully qualified `utils::render::shader::BlurHModule` and `utils::render::shader::BlurVModule` instead
- Lesson: In C++ nested namespaces, `using namespace X` inside `namespace A::B` resolves `X` relative to the global namespace, not relative to `A`. If `X` is `utils::render::shader` (which is really `biofuel::utils::render::shader`), it won't be found. Use fully qualified names or `using namespace biofuel::utils::render::shader`.
- Wave 3 committed as `24b058a`
- All 16 implementation tasks complete, 3 commits total
