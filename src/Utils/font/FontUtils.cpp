#include "FontUtils.hpp"
#include <spdlog/spdlog.h>

namespace biofuel::utils::font {

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
    unload(name);
    Font font = LoadFontEx(std::string{path}.c_str(), baseSize, nullptr, 0);
    if (!IsFontValid(font)) {
        spdlog::warn("FontManager: Failed to load font '{}' from '{}'", name, path);
        return;
    }
    m_fonts[std::string{name}] = font;
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

Font FontManager::get(std::string_view name) const noexcept {
    const std::string key{name};
    auto it = m_fonts.find(key);
    if (it != m_fonts.end()) {
        return it->second;
    }
    return GetFontDefault();
}

bool FontManager::has(std::string_view name) const noexcept {
    return m_fonts.find(std::string{name}) != m_fonts.end();
}

} // namespace biofuel::utils::font
