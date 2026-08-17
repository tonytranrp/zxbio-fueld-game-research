#pragma once

#include "engine/core/Types.hpp"
#include <raylib.h>
#include <string>
#include <string_view>
#include <unordered_map>

namespace biofuel::engine::fonts {

// ------------------------------------------------------------------------------
// FontManager - Font loading and caching utility
// Wraps Raylib font functions with name-based lookup.
// ------------------------------------------------------------------------------
class FontManager {
public:
    [[nodiscard]] static FontManager& instance() noexcept;

    void load(std::string_view name, std::string_view path, i32 baseSize);
    void unload(std::string_view name);
    void unloadAll() noexcept;
    void shutdown() noexcept;

    // Returns a reference to the cached entry; the reference stays valid until
    // the name is unload()ed or re-load()ed. Callers must not hold it across
    // those calls.
    [[nodiscard]] const Font& get(std::string_view name) const noexcept;
    [[nodiscard]] bool has(std::string_view name) const noexcept;

private:
    FontManager() = default;
    ~FontManager() noexcept;

    std::unordered_map<std::string, Font> m_fonts;
    bool m_shutDown = false;
};

} // namespace biofuel::engine::fonts
