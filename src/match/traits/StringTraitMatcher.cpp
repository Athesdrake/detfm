#include "match/traits/StringTraitMatcher.hpp"
#include "match/MultinameMatcher.hpp"
#include <vector>

namespace match {
StringTraitMatcher::StringTraitMatcher() : TraitMatcher() { }

MatchResult StringTraitMatcher::match_value(std::shared_ptr<abc::AbcFile>& abc, abc::Trait& trait) {
    if ((trait.kind == abc::TraitKind::Const || trait.kind == abc::TraitKind::Slot)
        && trait.slot.kind == 1)
        return type_name.match(abc, trait.slot.type)
            && (!value.has_value() || *value == abc->cpool.strings[trait.index]);

    return MatchResult::nomatch;
}

bool StringTraitMatcher::parse_value(YAML::Node& node) {
    if (node["value"] && node["value"].IsScalar())
        value = node["value"].as<std::string>();

    return true;
}

void StringTraitMatcher::tostring_value(YAML::Emitter& out) {
    if (value.has_value())
        out << YAML::Key << "value" << YAML::Value << *value;
}
}