#pragma once

namespace match {
class MatchResult {
public:
    enum class Result { match, nomatch, skip };

    static const MatchResult match;
    static const MatchResult nomatch;
    static const MatchResult skip;

    Result result;
    MatchResult(const bool& value);
    MatchResult(const Result& value);
    MatchResult(const MatchResult& value);

    const MatchResult& operator!() const;
    const MatchResult& operator&&(const MatchResult& other) const;
    const MatchResult& operator&=(const MatchResult& other);
    bool operator==(const MatchResult& other) const;
    bool operator==(const Result& other) const;
};

bool operator==(const MatchResult::Result& lhs, const MatchResult& rhs);
}