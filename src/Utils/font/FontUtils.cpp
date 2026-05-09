#include "FontUtils.hpp"

namespace biofuel::utils::font {

FontManager::~FontManager() {
    unloadAll();
}

FontManager& FontManager::instance() {
    static FontManager instance;
    return instance;
}

void FontManager::load(const std::string& name, const std::string& path, i32 baseSize) {
    unload(name);
    m_fonts[name] = LoadFontEx(path.c_str(), baseSize, nullptr, 0);
}

void FontManager::unload(const std::string& name) {
    auto it = m_fonts.find(name);
    if (it != m_fonts.end()) {
        UnloadFont(it->second);
        m_fonts.erase(it);
    }
}

void FontManager::unloadAll() {
    for (auto& [name, font] : m_fonts) {
        UnloadFont(font);
    }
    m_fonts.clear();
}

Font FontManager::get(const std::string& name) const {
    auto it = m_fonts.find(name);
    if (it != m_fonts.end()) {
        return it->second;
    }
    return GetFontDefault();
}

bool FontManager::has(const std::string& name) const {
    return m_fonts.find(name) != m_fonts.end();
}

} // namespace biofuel::utils::font
