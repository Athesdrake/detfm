#include "match/MethodMatcher.hpp"
#include <algorithm>

#define CHECK_RESULT(value)                                                                        \
    if ((result &= value) == MatchResult::nomatch)                                                 \
        return result;

namespace athes::detfm::match {
MethodMatcher::MethodMatcher() { }

MatchResult MethodMatcher::match(std::shared_ptr<abc::AbcFile>& abc, uint32_t index) {
    if (!enabled)
        return MatchResult::skip;

    auto& method = abc->methods[index];
    auto result  = MatchResult::skip;

    CHECK_RESULT(return_type.match(abc, method.return_type))
    CHECK_RESULT(name.match(abc, method.name))
    CHECK_RESULT(flags.match(method.flags))
    CHECK_RESULT(max_stack.match(method.max_stack))
    CHECK_RESULT(local_count.match(method.local_count))
    CHECK_RESULT(init_scope_depth.match(method.init_scope_depth))
    CHECK_RESULT(max_scope_depth.match(method.max_scope_depth))
    CHECK_RESULT(exceptions.match(method.exceptions.size()))
    CHECK_RESULT(traits.match(method.traits.size()))
    CHECK_RESULT(match_params(abc, method))
    return match_param_names(abc, method);
}

bool MethodMatcher::parse(YAML::Node& node) {
    if (node["return_type"] && node["return_type"].IsScalar())
        return_type = node["return_type"].as<std::string>();

    if (node["params"]) {
        if (node["params"].IsScalar()) {
            params = node["params"].as<int64_t>();
        } else if (node["params"].IsSequence()) {
            std::vector<MultinameMatcher> params;
            for (auto param : node["params"])
                params.emplace_back(param.as<std::string>());

            this->params = params;
        }
    }

    if (node["maxstack"] && node["maxstack"].IsScalar())
        max_stack = node["maxstack"].as<int64_t>();

    if (node["localcount"] && node["localcount"].IsScalar())
        local_count = node["localcount"].as<int64_t>();

    if (node["initscopedepth"] && node["initscopedepth"].IsScalar())
        init_scope_depth = node["initscopedepth"].as<int64_t>();

    if (node["maxscopedepth"] && node["maxscopedepth"].IsScalar())
        max_scope_depth = node["maxscopedepth"].as<int64_t>();

    enabled = true;
    return true;
}

MatchResult MethodMatcher::match_params(std::shared_ptr<abc::AbcFile>& abc, abc::Method& method) {
    if (std::holds_alternative<NumberMatcher>(params))
        return std::get<NumberMatcher>(params).match(method.params.size());

    auto& matchers = std::get<std::vector<MultinameMatcher>>(params);
    if (matchers.size() == 0)
        return MatchResult::skip;

    if (matchers.size() != method.params.size())
        return MatchResult::nomatch;

    for (auto i = 0; i < matchers.size(); ++i)
        if (matchers[i].match(abc, method.params[i]) == MatchResult::nomatch)
            return MatchResult::nomatch;

    return MatchResult::match;
}

MatchResult
MethodMatcher::match_param_names(std::shared_ptr<abc::AbcFile>& abc, abc::Method& method) {
    for (auto const& param : method.param_names) {
        if (!std::any_of(param_names.begin(), param_names.end(), [&param](auto const& value) {
                return param == value;
            }))
            return MatchResult::nomatch;
    }
    return MatchResult::match;
}

YAML::Emitter& operator<<(YAML::Emitter& out, MethodMatcher& method) {
    if (method.return_type.enabled) {
        out << YAML::Key << "return_type" << YAML::Value;
        if (std::holds_alternative<uint32_t>(method.return_type.value))
            out << std::get<uint32_t>(method.return_type.value);
        else
            out << std::get<std::string>(method.return_type.value);
    }

    if (std::holds_alternative<NumberMatcher>(method.params)) {
        auto params = std::get<NumberMatcher>(method.params);
        if (params.enabled)
            out << YAML::Key << "params" << YAML::Value << params.value;
    } else {
        auto params = std::get<std::vector<MultinameMatcher>>(method.params);
        if (!params.empty())
            out << YAML::Key << "params" << YAML::Value << params;
    }

    return out;
}
}