#include "FontUtils.hpp"
#include <spdlog/spdlog.h>

namespace biofuel::engine::fonts {

FontManager::~FontManager() noexcept {
    if (!m_shutDown) {
        unloadAll();
    }
}

FontManager& FontManager::instance() noexcept {
    static FontManager instance;
    return instance;
}

void FontManager::load(std::string_view name, std::string_view path, i32 baseSize) {
    m_shutDown = false;  // reset in case shutdown() was called before (B014)
    unload(name);
    // Reserve the map slot before the Raylib load so storing the font cannot
    // throw; a throw after LoadFontEx would leak the font's GPU handles.
    Font& slot = m_fonts[std::string{name}];
    Font font = LoadFontEx(std::string{path}.c_str(), baseSize, nullptr, 0);
    if (!IsFontValid(font)) {
        m_fonts.erase(std::string{name});
        spdlog::warn("FontManager: Failed to load font '{}' from '{}'", name, path);
        return;
    }
    slot = font;
}

void FontManager::unload(std::string_view name) {
    const std::string key{name};
    auto it = m_fonts.find(key);
    if (it != m_fonts.end()) {
        UnloadFont(it->second);
        m_fonts.erase(it);
    }
}

void FontManager::unloadAll() noexcept {
    for (auto& [name, font] : m_fonts) {
        UnloadFont(font);
    }
    m_fonts.clear();
}

void FontManager::shutdown() noexcept {
    unloadAll();
    m_shutDown = true;
}

const Font& FontManager::get(std::string_view name) const noexcept {
    const std::string key{name};
    auto it = m_fonts.find(key);
    if (it != m_fonts.end()) {
        return it->second;
    }
    // Raylib's default font is process-lifetime static data; caching one copy
    // lets get() return a reference on the miss path too.
    static const Font defaultFont = GetFontDefault();
    return defaultFont;
}

bool FontManager::has(std::string_view name) const noexcept {
    return m_fonts.find(std::string{name}) != m_fonts.end();
}

} // namespace biofuel::engine::fonts
