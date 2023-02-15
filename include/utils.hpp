#pragma once
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fmt/color.h>
#include <fmt/core.h>
#include <list>
#include <string>
#include <string_view>
#include <vector>

namespace utils {
using Clock      = std::chrono::high_resolution_clock;
using TimePoint  = typename Clock::time_point;
using TimePoints = typename std::vector<std::pair<std::string, utils::TimePoint>>;

TimePoint now();
double elapsled(TimePoint start, TimePoint stop);
double elapsled(TimePoint tp);

void read_from_stdin(std::vector<uint8_t>& file);

std::string get_unit(std::list<std::string> const& units, double& value, double factor = 1024);
std::string fmt_unit(std::list<std::string> const& units, double value, double factor = 1024);

enum class LogLevel : int {
    CRITICAL = 50,
    ERROR    = 40,
    WARNING  = 30,
    INFO     = 20,
    DEBUG    = 10,
    NOTSET   = 0,
};

struct Logger {
    LogLevel level;

    Logger(LogLevel level = LogLevel::NOTSET) : level(level) {
        if (level == LogLevel::NOTSET)
            level == LogLevel::INFO;
    }

    inline bool enabled_for(LogLevel lvl) { return level <= lvl; }

    template <typename... T> inline void log(std::string_view fmt, T&&... args) {
        const auto& vargs = fmt::make_format_args(args...);
        fmt::vprint(stderr, fmt, vargs);
    }
    template <typename... T>
    inline void log(fmt::text_style&& style, std::string_view fmt, T&&... args) {
        const auto& vargs = fmt::make_format_args(args...);
        fmt::vprint(stderr, style, fmt, vargs);
    }

    // Logs a critical message in red & bold
    template <typename... T> inline void critical(std::string_view fmt, T&&... args) {
        if (enabled_for(LogLevel::CRITICAL))
            log(fmt::emphasis::bold | fg(fmt::color::red), fmt, args...);
    }
    // Logs an error message in crimson
    template <typename... T> inline void error(std::string_view fmt, T&&... args) {
        if (enabled_for(LogLevel::ERROR))
            log(fg(fmt::color::crimson), fmt, args...);
    }
    // Logs a warning message in gold
    template <typename... T> inline void warn(std::string_view fmt, T&&... args) {
        if (enabled_for(LogLevel::WARNING))
            log(fg(fmt::color::gold), fmt, args...);
    }
    // Logs an info message
    template <typename... T> inline void info(std::string_view fmt, T&&... args) {
        if (enabled_for(LogLevel::INFO))
            log(fmt, args...);
    }
    // Logs a debug message
    template <typename... T> inline void debug(std::string_view fmt, T&&... args) {
        if (enabled_for(LogLevel::DEBUG))
            log(fmt, args...);
    }
    inline void log_done(TimePoints& tps, std::string name, bool new_line = true) {
        const auto last = tps.back().second;
        const auto curr = tps.emplace_back(name, now()).second;
        if (enabled_for(LogLevel::DEBUG))
            info("Done ({})\n", fmt_unit({ "µs", "ms", "s" }, elapsled(last, curr), 1000));
        else if (enabled_for(LogLevel::INFO) && new_line)
            info("\n");
    }
};
}