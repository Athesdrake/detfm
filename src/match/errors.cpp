#include "match/errors.hpp"
#include <fmt/core.h>

namespace match {

ParserError::ParserError(std::string message, std::string filename, YAML::Mark mark)
    : message(message), filename(filename), mark(mark) { }

const char* ParserError::what() const noexcept {
    return fmt::format("{} {}:{}", message, filename, mark.line).c_str();
}

}