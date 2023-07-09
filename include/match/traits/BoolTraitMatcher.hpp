#pragma once
#include "match/MatchResult.hpp"
#include "match/traits/TraitMatcher.hpp"
#include <abc/AbcFile.hpp>
#include <memory>
#include <optional>
#include <string>
#include <yaml-cpp/yaml.h>

namespace athes::detfm::match {
namespace abc = swf::abc;

class BoolTraitMatcher : public TraitMatcher {
public:
    std::optional<bool> value;

    BoolTraitMatcher();

    MatchResult match_value(std::shared_ptr<abc::AbcFile>& abc, abc::Trait& trait) override;
    bool parse_value(YAML::Node& node) override;
    void tostring_value(YAML::Emitter& out) override;
};
}