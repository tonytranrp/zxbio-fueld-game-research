#pragma once

#include <format>
#include <string_view>
#include <utility>

namespace engine::core {

enum class LogLevel { Debug, Info, Warn, Error };

void log(LogLevel level, std::string_view message);

template <typename... Args>
void log(LogLevel level, std::format_string<Args...> fmt, Args&&... args) {
    log(level, std::string_view(std::format(fmt, std::forward<Args>(args)...)));
}

} // namespace engine::core
