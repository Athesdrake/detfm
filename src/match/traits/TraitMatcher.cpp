#include "match/traits/TraitMatcher.hpp"

#define CHECK_RESULT(value)                                                                        \
    if ((result &= value) == MatchResult::nomatch)                                                 \
        return result;

namespace match {
TraitMatcher::TraitMatcher() { }
MatchResult TraitMatcher::match(std::shared_ptr<abc::AbcFile>& abc, abc::Trait& trait) {
    if (!enabled)
        return MatchResult::skip;

    auto result = MatchResult::skip;

    CHECK_RESULT(name.match(abc, trait.name))
    CHECK_RESULT(attr.match(trait.attr))
    CHECK_RESULT(slot_id.match(trait.slot_id))
    return match_value(abc, trait);
}

MatchResult TraitMatcher::match_value(std::shared_ptr<abc::AbcFile>& abc, abc::Trait& trait) {
    if ((trait.kind == abc::TraitKind::Const || trait.kind == abc::TraitKind::Slot)
        && trait.slot.kind == 0)
        return type_name.match(abc, trait.slot.type);

    return MatchResult::nomatch;
}

bool TraitMatcher::parse(YAML::Node& node) {
    type_name = node["type"].Scalar();
    if (node["const"] && node["const"].IsScalar())
        kind = NumberMatcher(
            NumberOpType::eq,
            uint8_t(node["const"].as<bool>(true) ? abc::TraitKind::Const : abc::TraitKind::Slot));

    if (node["slotid"] && node["slotid"].IsScalar())
        slot_id = node["slotid"].as<int64_t>();

    action  = ActionRename::create(node["action"]);
    enabled = true;

    return parse_value(node);
}
void TraitMatcher::tostring(YAML::Emitter& out) {
    if (action != nullptr)
        out << YAML::Key << "action" << YAML::Value << action;

    out << YAML::Key << "type" << YAML::Value << type_name;
    if (kind.value == static_cast<int64_t>(abc::TraitKind::Const))
        out << YAML::Key << "const" << YAML::Value << true;

    if (slot_id.enabled)
        out << YAML::Key << "slotid" << YAML::Value << slot_id;

    tostring_value(out);
}
}