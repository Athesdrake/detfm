#include "match/Action.hpp"

namespace match {
std::shared_ptr<ActionRename> ActionRename::create(YAML::Node node) {
    if (node && node.IsMap() && node["rename"] && node["rename"].IsScalar())
        return std::make_shared<ActionRename>(node["rename"].as<std::string>());

    return nullptr;
}

void ActionRename::execute() {
    if (std::holds_alternative<abc::Class*>(value))
        std::get<abc::Class*>(value)->rename(new_name);
    else if (std::holds_alternative<abc::Trait*>(value))
        std::get<abc::Trait*>(value)->rename(new_name);
}

void ActionRename::tostring(YAML::Emitter& out) {
    out << YAML::BeginMap;
    out << YAML::Key << "rename" << YAML::Value << new_name;
    out << YAML::EndMap;
}

YAML::Emitter& operator<<(YAML::Emitter& out, std::shared_ptr<Action>& action) {
    action->tostring(out);
    return out;
}
}