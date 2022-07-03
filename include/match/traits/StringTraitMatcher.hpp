#pragma once
#include "match/MatchResult.hpp"
#include "match/traits/TraitMatcher.hpp"
#include <abc/AbcFile.hpp>
#include <memory>
#include <optional>
#include <string>
#include <yaml-cpp/yaml.h>

namespace match {
namespace abc = swf::abc;

class StringTraitMatcher : public TraitMatcher {
public:
    std::optional<std::string> value;

    StringTraitMatcher();

    MatchResult match_value(std::shared_ptr<abc::AbcFile>& abc, abc::Trait& trait) override;
    bool parse_value(YAML::Node& node) override;
    void tostring_value(YAML::Emitter& out) override;
};
}