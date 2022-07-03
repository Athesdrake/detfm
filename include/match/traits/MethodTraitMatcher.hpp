#pragma once
#include "match/Action.hpp"
#include "match/MatchResult.hpp"
#include "match/MethodMatcher.hpp"
#include "match/traits/TraitMatcher.hpp"
#include <abc/AbcFile.hpp>
#include <memory>

namespace match {
namespace abc = swf::abc;

class MethodTraitMatcher : public TraitMatcher {
public:
    MethodMatcher value;

    MethodTraitMatcher();

    MatchResult match_value(std::shared_ptr<abc::AbcFile>& abc, abc::Trait& trait) override;

    bool parse_value(YAML::Node& node) override;
    void tostring_value(YAML::Emitter& out) override;
};
}