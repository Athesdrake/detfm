#include "match/MatchResult.hpp"

namespace athes::detfm::match {
const MatchResult MatchResult::match   = Result::match;
const MatchResult MatchResult::nomatch = Result::nomatch;
const MatchResult MatchResult::skip    = Result::skip;

MatchResult::MatchResult(const bool& value) : MatchResult(value ? match : nomatch) { }
MatchResult::MatchResult(const Result& value) : result(value) { }
MatchResult::MatchResult(const MatchResult& value) : result(value.result) { }
const MatchResult& MatchResult::operator!() const {
    switch (result) {
    case Result::match:
        return nomatch;
    case Result::nomatch:
        return match;
    }
    return skip;
}
const MatchResult& MatchResult::operator&&(const MatchResult& other) const {
    if (result == nomatch || (result == match && other.result == skip))
        return *this;
    return other;
}
const MatchResult& MatchResult::operator&=(const MatchResult& other) {
    result = operator&&(other).result;
    return *this;
}
bool MatchResult::operator==(const MatchResult& other) const { return result == other.result; }
bool MatchResult::operator==(const Result& other) const { return result == other; }
bool operator==(const MatchResult::Result& lhs, const MatchResult& rhs) { return rhs == lhs; }
}