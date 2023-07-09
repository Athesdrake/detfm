#include "match/MultinameMatcher.hpp"
#include <cstdint>
#include <vector>

namespace athes::detfm::match {
MultinameMatcher::MultinameMatcher() { }
MultinameMatcher::MultinameMatcher(uint32_t value) : enabled(true), value(value) { }
MultinameMatcher::MultinameMatcher(std::string const& value) : enabled(true), value(value) { }
MultinameMatcher::MultinameMatcher(std::variant<uint32_t, std::string> value)
    : enabled(true), value(value) { }
MatchResult MultinameMatcher::match(std::shared_ptr<abc::AbcFile>& abc, uint32_t index) {
    if (!enabled)
        return MatchResult::skip;

    if (std::holds_alternative<uint32_t>(value))
        return std::get<uint32_t>(value) == index;

    auto& mn = abc->cpool.multinames[index];
    if (mn.kind == abc::MultinameKind::Typename)
        return MatchResult::nomatch;

    auto str = mn.get_name_index();
    return std::get<std::string>(value) == abc->cpool.strings[str];
}

YAML::Emitter& operator<<(YAML::Emitter& out, MultinameMatcher& mn) {
    if (!mn.enabled)
        return out;

    if (std::holds_alternative<uint32_t>(mn.value))
        return out << std::get<uint32_t>(mn.value);

    return out << std::get<std::string>(mn.value);
}

YAML::Emitter& operator<<(YAML::Emitter& out, std::vector<MultinameMatcher>& traits) {
    out << YAML::BeginSeq;
    for (auto& trait : traits)
        out << trait;

    return out << YAML::EndSeq;
}
}