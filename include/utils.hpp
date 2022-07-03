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

static int log_level = 0;

TimePoint now();
double elapsled(TimePoint start, TimePoint stop);
double elapsled(TimePoint tp);

void read_from_stdin(std::vector<uint8_t>& file);

std::string get_unit(std::list<std::string> const& units, double& value, double factor = 1024);
std::string fmt_unit(std::list<std::string> const& units, double value, double factor = 1024);

template <typename... T> inline void log(std::string_view fmt, T&&... args) {
    const auto& vargs = fmt::make_format_args(args...);
    fmt::vprint(stderr, fmt, vargs);
}
template <typename... T> inline void log_error(std::string_view fmt, T&&... args) {
    const auto& vargs = fmt::make_format_args(args...);
    fmt::vprint(stderr, fg(fmt::color::crimson), fmt, vargs);
}
template <typename... T> inline void log_info(std::string_view fmt, T&&... args) {
    if (log_level > 0)
        log(fmt, args...);
}
template <typename... T> inline void log_debug(std::string_view fmt, T&&... args) {
    if (log_level > 1)
        log(fmt, args...);
}
inline void log_done(TimePoints& tps, std::string name, bool new_line = true) {
    const auto last = tps.back().second;
    const auto curr = tps.emplace_back(name, now()).second;
    if (log_level > 1)
        log("Done ({})\n", fmt_unit({ "µs", "ms", "s" }, elapsled(last, curr), 1000));
    else if (log_level > 0 && new_line)
        log("\n");
}
}