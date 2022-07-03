#pragma once
#include "match/MatchResult.hpp"
#include <abc/AbcFile.hpp>
#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <yaml-cpp/yaml.h>

namespace match {
namespace abc = swf::abc;

class MultinameMatcher {
public:
    bool enabled = false;
    std::variant<uint32_t, std::string> value;

    MultinameMatcher();
    MultinameMatcher(uint32_t value);
    MultinameMatcher(std::string const& value);
    MultinameMatcher(std::variant<uint32_t, std::string> value);

    MatchResult match(std::shared_ptr<abc::AbcFile>& abc, uint32_t index);

    void tostring(YAML::Emitter& out);
};

YAML::Emitter& operator<<(YAML::Emitter& out, MultinameMatcher& trait);
YAML::Emitter& operator<<(YAML::Emitter& out, std::vector<MultinameMatcher>& traits);
}