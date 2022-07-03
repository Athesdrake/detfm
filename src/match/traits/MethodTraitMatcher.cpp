#include "match/traits/MethodTraitMatcher.hpp"

namespace match {
MethodTraitMatcher::MethodTraitMatcher() : TraitMatcher() { }

MatchResult MethodTraitMatcher::match_value(std::shared_ptr<abc::AbcFile>& abc, abc::Trait& trait) {
    if (trait.kind == abc::TraitKind::Method)
        return value.match(abc, trait.index);

    return MatchResult::nomatch;
}

bool MethodTraitMatcher::parse_value(YAML::Node& node) {
    if (node["dispid"] && node["dispid"].IsScalar())
        slot_id = node["dispid"].as<int64_t>();

    int64_t attrs = 0;
    bool negate;
    if (node["final"] && node["final"].IsScalar()) {
        negate = !node["final"].as<bool>(true);
        attrs  = 1;
    }

    if (node["override"] && node["override"].IsScalar()) {
        if (negate != !node["override"].as<bool>(true))
            return false;

        attrs &= 2;
    }

    if (attrs != 0)
        attr = NumberMatcher(NumberOpType::band, attrs, negate);

    value.parse(node);
    action  = ActionRename::create(node["action"]);
    enabled = true;
    return true;
}

void MethodTraitMatcher::tostring_value(YAML::Emitter& out) {
    if (action != nullptr)
        out << YAML::Key << "action" << YAML::Value << action;

    if (slot_id.enabled)
        out << YAML::Key << "dispid" << YAML::Value << slot_id;

    if (attr.enabled) {
        if (attr.value & 1)
            out << YAML::Key << "final" << YAML::Value << !attr.negate;

        if (attr.value & 2)
            out << YAML::Key << "override" << YAML::Value << !attr.negate;
    }
    out << value;
}
}