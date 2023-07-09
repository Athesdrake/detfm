#pragma once
#include "match/MatchResult.hpp"
#include "match/MultinameMatcher.hpp"
#include "match/NumberMatcher.hpp"
#include <abc/AbcFile.hpp>
#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>
#include <yaml-cpp/yaml.h>

namespace athes::detfm::match {
namespace abc = swf::abc;

class MethodMatcher {
public:
    bool enabled = false;
    MultinameMatcher return_type;
    MultinameMatcher name;
    NumberMatcher flags;
    std::variant<NumberMatcher, std::vector<MultinameMatcher>> params;
    std::vector<std::string> param_names;

    NumberMatcher max_stack;
    NumberMatcher local_count;
    NumberMatcher init_scope_depth;
    NumberMatcher max_scope_depth;

    NumberMatcher exceptions;
    NumberMatcher traits;

    MethodMatcher();

    MatchResult match(std::shared_ptr<abc::AbcFile>& abc, uint32_t index);

    bool parse(YAML::Node& node);
    void tostring(YAML::Emitter& out);

private:
    MatchResult match_params(std::shared_ptr<abc::AbcFile>& abc, abc::Method& method);
    MatchResult match_param_names(std::shared_ptr<abc::AbcFile>& abc, abc::Method& method);
};

YAML::Emitter& operator<<(YAML::Emitter& out, MethodMatcher& trait);
}