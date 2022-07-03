#pragma once
#include "match/MatchResult.hpp"
#include "match/MethodMatcher.hpp"
#include "match/MultinameMatcher.hpp"
#include "match/NumberMatcher.hpp"
#include <abc/AbcFile.hpp>
#include <cstdint>
#include <memory>
#include <vector>
#include <yaml-cpp/yaml.h>

namespace match {
namespace abc = swf::abc;

class Action;
class TraitMatcher;

class ClassMatcher {
public:
    bool enabled = true;
    bool strict  = false;
    MultinameMatcher name;
    MultinameMatcher super_name;
    NumberMatcher flags;
    MethodMatcher iinit;
    MethodMatcher cinit;
    std::vector<std::shared_ptr<TraitMatcher>> itraits;
    std::vector<std::shared_ptr<TraitMatcher>> ctraits;
    std::shared_ptr<Action> action;
    std::string debug;

    ClassMatcher();

    MatchResult match(std::shared_ptr<abc::AbcFile>& abc, uint32_t index);
    MatchResult match_traits(std::shared_ptr<abc::AbcFile>& abc, abc::Class& klass);
    void execute_actions();
    bool parse(YAML::Node& node);
    void tostring(YAML::Emitter& out);
};

void load_classdef(std::string filename, std::list<std::shared_ptr<match::ClassMatcher>>& classes);
}