# Decisions

## 2026-05-09
- Shader Module pattern: Convention-only (no virtual, no inheritance) — each module is standalone class with consistent API
- Module headers: Only include Core/Types.hpp and <string_view> — NO raylib.h or ShaderManager.hpp
- Compile-time priority: constexpr + minimal includes
- DrawTextureRec bypass in ScreenBlurEffect: Documented as known exception, NOT fixed in this workstream
- C++20 designated initializers with Raylib Color: Must verify on MSVC (Task 16 has fallback)
- AnimationEvents const char* → string_view: Safe because all usage sites pass string literals