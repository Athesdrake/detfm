#include "utils.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <errno.h>
#include <fmt/format.h>
#include <list>
#include <ratio>
#include <stdexcept>
#include <vector>

using namespace fmt::literals;

namespace athes::utils {
TimePoint now() { return Clock::now(); }
double elapsled(TimePoint start, TimePoint stop) {
    std::chrono::duration<double, std::micro> dt = stop - start;
    return dt.count();
}
double elapsled(TimePoint tp) { return elapsled(tp, now()); }

void read_from_stdin(std::vector<uint8_t>& file) {
    (void)!std::freopen(nullptr, "rb", stdin);
    if (std::ferror(stdin))
        throw std::runtime_error(std::strerror(errno));

    size_t length;
    std::array<uint8_t, 1024> buf;

    while ((length = std::fread(buf.data(), sizeof(buf[0]), buf.size(), stdin)) > 0) {
        if (std::ferror(stdin) && !std::feof(stdin))
            throw std::runtime_error(std::strerror(errno));

        file.insert(file.end(), buf.data(), buf.data() + length);
    }
}

std::string get_unit(std::list<std::string> const& units, double& value, double factor) {
    auto it = units.begin();
    while (value >= factor && ++it != units.end())
        value /= factor;

    return *it;
}
std::string fmt_unit(std::list<std::string> const& units, double value, double factor) {
    return fmt::format(
        "{:.2f} {}",
        fmt::styled(value, fmt::fg(fmt::color::dark_cyan) | fmt::emphasis::italic),
        get_unit(units, value, factor));
}
}