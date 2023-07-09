#pragma once
#include "match/Action.hpp"
#include "match/MatchResult.hpp"
#include "match/MultinameMatcher.hpp"
#include "match/NumberMatcher.hpp"
#include <abc/AbcFile.hpp>
#include <memory>
#include <yaml-cpp/yaml.h>

namespace athes::detfm::match {
namespace abc = swf::abc;

class TraitMatcher {
public:
    bool enabled = false;
    MultinameMatcher name;
    NumberMatcher kind;
    NumberMatcher attr;
    NumberMatcher slot_id;
    NumberMatcher index;
    MultinameMatcher type_name;
    std::shared_ptr<Action> action;

    TraitMatcher();

    bool parse(YAML::Node& node);
    void tostring(YAML::Emitter& out);

    MatchResult match(std::shared_ptr<abc::AbcFile>& abc, abc::Trait& trait);
    virtual MatchResult match_value(std::shared_ptr<abc::AbcFile>& abc, abc::Trait& trait);

    virtual bool parse_value(YAML::Node& node) { return true; };
    virtual void tostring_value(YAML::Emitter& out) {};
};
}