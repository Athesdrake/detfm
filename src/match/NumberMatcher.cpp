#include "match/NumberMatcher.hpp"

namespace athes::detfm::match {
NumberMatcher::NumberMatcher() { }
NumberMatcher::NumberMatcher(int64_t value) : NumberMatcher(NumberOpType::eq, value) { }
NumberMatcher::NumberMatcher(NumberOpType optype, int64_t value, bool negate)
    : enabled(true), negate(negate), optype(optype), value(value) { }

MatchResult NumberMatcher::match(int64_t value) {
    if (!enabled)
        return MatchResult::skip;

    return negate ? !eval(value) : eval(value);
}

MatchResult NumberMatcher::eval(int64_t value) {
    switch (optype) {
    case NumberOpType::eq:
        return this->value == value;
    case NumberOpType::neq:
        return this->value != value;
    case NumberOpType::lt:
        return this->value < value;
    case NumberOpType::gt:
        return this->value > value;
    case NumberOpType::le:
        return this->value <= value;
    case NumberOpType::ge:
        return this->value >= value;
    case NumberOpType::band:
        return this->value & value;
    default:
        return MatchResult::skip;
    }
}

YAML::Emitter& operator<<(YAML::Emitter& out, NumberMatcher& matcher) {
    return out << matcher.value;
}
}