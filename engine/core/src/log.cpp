#include "engine/core/log.hpp"

#include <iostream>

namespace engine::core {

namespace {

std::string_view level_name(LogLevel level) {
    switch (level) {
    case LogLevel::Debug:
        return "DEBUG";
    case LogLevel::Info:
        return "INFO";
    case LogLevel::Warn:
        return "WARN";
    case LogLevel::Error:
        return "ERROR";
    }
    return "?";
}

} // namespace

void log(LogLevel level, std::string_view message) {
    std::cout << std::format("[{}] {}\n", level_name(level), message);
}

} // namespace engine::core
