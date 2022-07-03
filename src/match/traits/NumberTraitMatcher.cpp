#include "match/traits/NumberTraitMatcher.hpp"
#include "match/MultinameMatcher.hpp"
#include <vector>

namespace match {
NumberTraitMatcher::NumberTraitMatcher() : TraitMatcher() { }

MatchResult NumberTraitMatcher::match_value(std::shared_ptr<abc::AbcFile>& abc, abc::Trait& trait) {
    if (trait.kind == abc::TraitKind::Const || trait.kind == abc::TraitKind::Slot) {
        auto result = type_name.match(abc, trait.slot.type);
        if (result == MatchResult::nomatch || !value.has_value())
            return result;

        switch (trait.slot.kind) {
        case 3:
            return *value == abc->cpool.integers[trait.index];
        case 4:
            return *value == abc->cpool.uintegers[trait.index];
        case 6:
            return *value == abc->cpool.doubles[trait.index];
        }
    }

    return MatchResult::nomatch;
}

bool NumberTraitMatcher::parse_value(YAML::Node& node) {
    if (node["value"] && node["value"].IsScalar())
        value = node["value"].as<double>();

    return true;
}
void NumberTraitMatcher::tostring_value(YAML::Emitter& out) {
    if (value.has_value())
        out << YAML::Key << "value" << YAML::Value << *value;
}
}