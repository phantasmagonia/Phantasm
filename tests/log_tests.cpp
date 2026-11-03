#include <phantasm/log.hpp>

#include <catch2/catch_test_macros.hpp>

using phantasm::LogLevel;

TEST_CASE("Log lines are prefixed with their level") {
    CHECK(phantasm::format_log_line(LogLevel::Info, "hello") == "[info] hello");
    CHECK(phantasm::format_log_line(LogLevel::Warn, "careful") == "[warn] careful");
    CHECK(phantasm::format_log_line(LogLevel::Error, "boom") == "[error] boom");
    CHECK(phantasm::format_log_line(LogLevel::Debug, "x=1") == "[debug] x=1");
}
