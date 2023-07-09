#include "match/ClassMatcher.hpp"
#include "match/Action.hpp"
#include "match/errors.hpp"
#include "match/traits/BoolTraitMatcher.hpp"
#include "match/traits/MethodTraitMatcher.hpp"
#include "match/traits/NumberTraitMatcher.hpp"
#include "match/traits/StringTraitMatcher.hpp"
#include "match/traits/TraitMatcher.hpp"
#include <algorithm>
#include <fmt/format.h>
#include <set>
#include <variant>

#define CHECK_RESULT(value)                                                                        \
    if ((result &= value) == MatchResult::nomatch)                                                 \
        return result;

namespace athes::detfm::match {

ClassMatcher::ClassMatcher() { }

MatchResult ClassMatcher::match(std::shared_ptr<abc::AbcFile>& abc, uint32_t index) {
    if (!enabled)
        return MatchResult::skip;

    auto& klass = abc->classes[index];
    auto result = MatchResult::skip;

    CHECK_RESULT(name.match(abc, klass.name))
    CHECK_RESULT(super_name.match(abc, klass.name))
    CHECK_RESULT(flags.match(klass.flags))
    CHECK_RESULT(iinit.match(abc, klass.iinit))
    CHECK_RESULT(cinit.match(abc, klass.cinit))
    return match_traits(abc, klass);
}

MatchResult ClassMatcher::match_traits(std::shared_ptr<abc::AbcFile>& abc, abc::Class& klass) {
    if (this->itraits.empty() && this->ctraits.empty())
        return MatchResult::skip;

    std::set<abc::Trait*> itraits;
    std::set<abc::Trait*> ctraits;
    for (auto& trait : klass.itraits)
        itraits.insert(&trait);
    for (auto& trait : klass.ctraits)
        ctraits.insert(&trait);

    for (auto& matcher : this->itraits) {
        auto result = std::find_if(itraits.begin(), itraits.end(), [&](abc::Trait* trait) {
            return matcher->match(abc, *trait) == MatchResult::match;
        });
        if (result == itraits.end())
            return MatchResult::nomatch;

        if (matcher->action != nullptr)
            matcher->action->value = *result;
        itraits.erase(result);
    }

    for (auto& matcher : this->ctraits) {
        auto result = std::find_if(ctraits.begin(), ctraits.end(), [&](abc::Trait* trait) {
            return matcher->match(abc, *trait) == MatchResult::match;
        });
        if (result == ctraits.end())
            return MatchResult::nomatch;

        if (matcher->action != nullptr)
            matcher->action->value = *result;
        ctraits.erase(result);
    }

    if (action != nullptr)
        action->value = &klass;

    auto empty = itraits.empty() && ctraits.empty();
    return !strict || (empty) ? MatchResult::match : MatchResult::nomatch;
}

void ClassMatcher::execute_actions() {
    if (action)
        action->execute();

    for (auto& trait : itraits)
        if (trait->action)
            trait->action->execute();

    for (auto& trait : ctraits)
        if (trait->action)
            trait->action->execute();
}

bool ClassMatcher::parse(YAML::Node& node) {
    if (!node || !node.IsMap())
        return false;

    if (node["strict"])
        strict = node["strict"].as<bool>(true);

    if (node["debug"])
        debug = node["debug"].as<std::string>();

    if (node["static traits"] && node["static traits"].IsSequence()) {
        for (auto child : node["static traits"]) {
            if (!child.IsMap() || !child["type"])
                continue;

            auto type = child["type"].Scalar();
            if (type == "String") {
                if (!ctraits.emplace_back(std::make_shared<StringTraitMatcher>())->parse(child))
                    return false;
            } else if (type == "Number" || type == "int") {
                if (!ctraits.emplace_back(std::make_shared<NumberTraitMatcher>())->parse(child))
                    return false;
            } else if (type == "Boolean") {
                if (!ctraits.emplace_back(std::make_shared<BoolTraitMatcher>())->parse(child))
                    return false;
            } else {
                if (!ctraits.emplace_back(std::make_shared<TraitMatcher>())->parse(child))
                    return false;
            }
        }
    }

    if (node["static methods"] && node["static methods"].IsSequence()) {
        for (auto child : node["static methods"]) {
            if (!child.IsMap())
                continue;

            if (!ctraits.emplace_back(std::make_shared<MethodTraitMatcher>())->parse(child))
                return false;
        }
    }

    action = ActionRename::create(node["action"]);
    return true;
}

void ClassMatcher::tostring(YAML::Emitter& out) {
    out << YAML::BeginMap;
    if (action != nullptr)
        out << YAML::Key << "action" << YAML::Value << action;

    if (!debug.empty())
        out << YAML::Key << "debug" << YAML::Value << debug;

    out << YAML::Key << "strict" << YAML::Value << strict;
    std::vector<std::shared_ptr<TraitMatcher>> static_traits, static_methods;

    for (auto& trait : ctraits) {
        if (trait->kind.value == static_cast<int64_t>(abc::TraitKind::Method)) {
            static_methods.push_back(trait);
        } else {
            static_traits.push_back(trait);
        }
    }

    if (!static_traits.empty()) {
        out << YAML::Key << "static traits" << YAML::Value << YAML::BeginSeq;
        for (auto& trait : static_traits) {
            out << YAML::BeginMap;
            trait->tostring(out);
            out << YAML::EndMap;
        }
        out << YAML::EndSeq;
    }
    if (!static_methods.empty()) {
        out << YAML::Key << "static methods" << YAML::Value << YAML::BeginSeq;
        for (auto& trait : static_methods) {
            out << YAML::BeginMap;
            trait->tostring(out);
            out << YAML::EndMap;
        }
        out << YAML::EndSeq;
    }

    out << YAML::EndMap;
}

void load_classdef(std::string filename, std::list<std::shared_ptr<match::ClassMatcher>>& classes) {
    auto tree = YAML::LoadFile(filename);
    auto node = tree["classes"];

    if (!node)
        throw ParserError("Node 'classes' is missing from file", filename, node.Mark());

    if (!node.IsSequence())
        throw ParserError("Node 'classes' must be a sequence.", filename, node.Mark());

    for (auto child : node) {
        auto klass = std::make_shared<match::ClassMatcher>();
        if (!klass->parse(child))
            throw ParserError("Unable to parse class.", filename, child.Mark());

        classes.push_back(klass);
    }
}
}