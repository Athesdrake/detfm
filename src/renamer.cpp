#include "renamer.hpp"
#include <algorithm>
#include <cctype>
#include <fmt/compile.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <iterator>
#include <stdexcept>
#include <unordered_map>
#include <vector>

using namespace fmt::literals;

std::optional<std::string> check_format(std::string const& format, fmt::format_args args) {
    try {
        if (fmt::vformat(format, args) == format)
            return "Formatted cannot be the same as the input. (Missing {}?)";
    } catch (const fmt::format_error& error) {
        return std::make_optional<std::string>(error.what());
    }
    return std::nullopt;
}

Fmt::Fmt() {
    std::unordered_map<std::string, const StringFmtBase*> formats = {
        { "classes", &classes },
        { "consts", &consts },
        { "functions", &functions },
        { "names", &names },
        { "vars", &vars },
        { "methods", &methods },
        { "errors", &errors },
        { "unknown_clientbound_packet", &unknown_clientbound_packet },
        { "packet_subhandler", &packet_subhandler },
        { "clientbound_packet", &clientbound_packet },
        { "serverbound_packet", &serverbound_packet },
    };
    for (const auto& [name, fmt] : formats) {
        auto error = fmt->valid();
        if (error) {
            throw std::runtime_error(fmt::format(
                FMT_COMPILE("\"{value}\" is not a valid format string for [{name}]: {reason}"),
                "value"_a  = fmt->value,
                "name"_a   = name,
                "reason"_a = *error));
        }
    }
}

void Fmt::from_json(json& config, utils::Logger& logger) {
    if (!config.is_object() || !config.contains("formats"))
        return;

    for (auto& [name, field] : get_fields()) {
        json::json_pointer ptr(name);
        if (!config.contains(ptr))
            continue;

        auto& child = config[ptr];
        if (!child.is_string()) {
            std::replace(name.begin(), name.end(), '/', '.');
            logger.error("'{}' must be a string.\n", name.substr(1));
            continue;
        }

        auto value = child.get<std::string>();
        if (value.empty())
            continue;

        auto error = field->valid(value);
        if (error) {
            std::replace(name.begin(), name.end(), '/', '.');
            logger.error("'{}' is not a valid format: {}\n", name.substr(1), *error);
        } else {
            field->value = value;
        }
    }
}

json Fmt::to_json() {
    Fmt fmt;
    json config;

    for (auto& [name, field] : fmt.get_fields())
        config[name] = field->value;

    return config.unflatten();
}

Fmt::Fields Fmt::get_fields() {
    return {
        { "/formats/typenames/classes", &classes },
        { "/formats/typenames/consts", &consts },
        { "/formats/typenames/functions", &functions },
        { "/formats/typenames/names", &names },
        { "/formats/typenames/vars", &vars },
        { "/formats/typenames/methods", &methods },
        { "/formats/typenames/errors", &errors },
        { "/formats/packets/packet_subhandler", &packet_subhandler },
        { "/formats/packets/unknown_clientbound_packet", &unknown_clientbound_packet },
        { "/formats/packets/clientbound", &clientbound_packet },
        { "/formats/packets/serverbound", &serverbound_packet },
        { "/formats/packets/tribulle/clientbound", &tribulle_clientbound_packet },
        { "/formats/packets/tribulle/serverbound", &tribulle_serverbound_packet },
    };
}

Renamer::Renamer(std::shared_ptr<abc::AbcFile> const& abc, Fmt const& fmt) : abc(abc), fmt(fmt) { }

bool Renamer::invalid(std::string const& name) { return invalid(std::string_view(name)); }
bool Renamer::invalid(std::string_view name) {
    return std::any_of(
        std::begin(name), std::end(name), [](const char c) { return !std::isprint(c); });
}

void Renamer::rename() {
    for (auto& klass : abc->classes)
        rename(klass);

    for (auto& method : abc->methods)
        rename(method);
}

void Renamer::rename(abc::Class& klass) {
    if (klass.name != 0 && invalid(klass.get_name()))
        klass.rename(fmt.classes.format(++counters.classes));

    if (klass.super_name != 0 && invalid(klass.get_super_name()))
        klass.rename_super(fmt.classes.format(++counters.classes));

    for (auto& trait : klass.ctraits)
        rename(trait);

    for (auto& trait : klass.itraits)
        rename(trait);
}
void Renamer::rename(abc::Trait& trait) {
    if (trait.name != 0 && invalid(trait.get_name())) {
        switch (trait.kind) {
        case swf::abc::TraitKind::Const:
            trait.rename(fmt.consts.format(++counters.consts));
            break;
        case swf::abc::TraitKind::Method:
            trait.rename(fmt.methods.format(++counters.methods));
            break;
        case swf::abc::TraitKind::Function:
            trait.rename(fmt.functions.format(++counters.functions));
            break;
        default:
            trait.rename(fmt.names.format(++counters.names));
            break;
        }
    }
}

void Renamer::rename(abc::Method& method) {
    int counter = 0;
    if (method.exceptions.size() == 1) {
        rename(method.exceptions[0]);
    } else {
        for (auto& err : method.exceptions)
            rename(err, ++counter);
    }

    for (auto& trait : method.traits)
        rename(trait);
}
void Renamer::rename(abc::Exception& err) {
    if (invalid(err.get_var_name()))
        err.rename_var_name("error");
}
void Renamer::rename(abc::Exception& err, int counter) {
    if (invalid(err.get_var_name())) {
        auto name = fmt.errors.format(counter);
        err.rename_var_name(name);
    }
}