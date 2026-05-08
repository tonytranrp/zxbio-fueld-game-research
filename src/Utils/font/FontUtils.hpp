#pragma once

#include <raylib.h>
#include <string>
#include <unordered_map>

namespace biofuel::utils::font {

// ------------------------------------------------------------------------------
// FontManager - Font loading and caching utility
// Wraps Raylib font functions with name-based lookup.
// ------------------------------------------------------------------------------
class FontManager {
public:
    static FontManager& instance();

    void load(const std::string& name, const std::string& path, int baseSize);
    void unload(const std::string& name);
    void unloadAll();

    [[nodiscard]] Font get(const std::string& name) const;
    [[nodiscard]] bool has(const std::string& name) const;

private:
    FontManager() = default;
    ~FontManager();

    std::unordered_map<std::string, Font> m_fonts;
};

} // namespace biofuel::utils::font
