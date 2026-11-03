#pragma once

#include <format>
#include <string>
#include <string_view>
#include <utility>

namespace phantasm {

enum class LogLevel { Debug, Info, Warn, Error };

[[nodiscard]] std::string format_log_line(LogLevel level, std::string_view message);

void log_message(LogLevel level, std::string_view message);

template <typename... Args>
void log_debug(std::format_string<Args...> fmt, Args&&... args) {
    log_message(LogLevel::Debug, std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void log_info(std::format_string<Args...> fmt, Args&&... args) {
    log_message(LogLevel::Info, std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void log_warn(std::format_string<Args...> fmt, Args&&... args) {
    log_message(LogLevel::Warn, std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void log_error(std::format_string<Args...> fmt, Args&&... args) {
    log_message(LogLevel::Error, std::format(fmt, std::forward<Args>(args)...));
}

} // namespace phantasm
