#pragma once
#include <abc/AbcFile.hpp>
#include <string>
#include <variant>
#include <yaml-cpp/yaml.h>

namespace athes::detfm::match {
namespace abc = swf::abc;

class Action {
public:
    std::variant<abc::Class*, abc::Trait*> value;

    Action() { }
    virtual void execute()                    = 0;
    virtual void tostring(YAML::Emitter& out) = 0;
};

class ActionRename : public Action {
public:
    std::string new_name;

    ActionRename(std::string name) : new_name(name) { }
    static std::shared_ptr<ActionRename> create(YAML::Node node);

    void execute() override;
    void tostring(YAML::Emitter& out) override;
};

YAML::Emitter& operator<<(YAML::Emitter& out, std::shared_ptr<Action>& action);
}