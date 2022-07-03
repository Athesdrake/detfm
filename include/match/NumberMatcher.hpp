#pragma once
#include "match/MatchResult.hpp"
#include <cstdint>
#include <yaml-cpp/yaml.h>

namespace match {
enum class NumberOpType {
    eq,
    neq,
    lt,
    gt,
    le,
    ge,
    band,
};

class NumberMatcher {

public:
    bool enabled = false;
    bool negate  = false;
    NumberOpType optype;
    int64_t value;

    NumberMatcher();
    NumberMatcher(int64_t value);
    NumberMatcher(NumberOpType optype, int64_t value, bool negate = false);

    MatchResult match(int64_t value);

    void tostring(YAML::Emitter& out);

private:
    MatchResult eval(int64_t value);
};

YAML::Emitter& operator<<(YAML::Emitter& out, NumberMatcher& matcher);
}