#include <phantasm/log.hpp>

#include <cstdio>

namespace phantasm {

namespace {
constexpr std::string_view level_name(LogLevel level) noexcept {
    switch (level) {
    case LogLevel::Debug: return "debug";
    case LogLevel::Info: return "info";
    case LogLevel::Warn: return "warn";
    case LogLevel::Error: return "error";
    }
    return "unknown";
}
} // namespace

std::string format_log_line(LogLevel level, std::string_view message) {
    return std::format("[{}] {}", level_name(level), message);
}

void log_message(LogLevel level, std::string_view message) {
    const bool is_problem = level == LogLevel::Warn || level == LogLevel::Error;
    std::FILE* stream = is_problem ? stderr : stdout;
    const std::string line = format_log_line(level, message);
    std::fprintf(stream, "%s\n", line.c_str());
}

} // namespace phantasm
