#include "match/traits/BoolTraitMatcher.hpp"
#include "match/MultinameMatcher.hpp"
#include <vector>

namespace match {
BoolTraitMatcher::BoolTraitMatcher() : TraitMatcher() { }

MatchResult BoolTraitMatcher::match_value(std::shared_ptr<abc::AbcFile>& abc, abc::Trait& trait) {
    if (trait.kind == abc::TraitKind::Const || trait.kind == abc::TraitKind::Slot) {
        auto result = type_name.match(abc, trait.slot.type);
        if (result == MatchResult::nomatch || !value.has_value())
            return result;

        switch (trait.slot.kind) {
        case 0x0A:
            return *value == false;
        case 0x0B:
            return *value == true;
        }
    }

    return MatchResult::nomatch;
}

bool BoolTraitMatcher::parse_value(YAML::Node& node) {
    if (node["value"] && node["value"].IsScalar())
        value = node["value"].as<bool>();

    return true;
}
void BoolTraitMatcher::tostring_value(YAML::Emitter& out) {
    if (value.has_value())
        out << YAML::Key << "value" << YAML::Value << *value;
}
}