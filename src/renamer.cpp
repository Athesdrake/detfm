#include "renamer.hpp"
#include <algorithm>
#include <cctype>
#include <fmt/compile.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <iterator>
#include <list>
#include <stdexcept>
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

std::string StringFmt<1>::format(int a) { return fmt::format(value, a); }
std::string StringFmt<2>::format(int a, int b) { return fmt::format(value, a, b); }
std::optional<std::string> StringFmt<1>::valid(std::string const& format) {
    return check_format(format, fmt::make_format_args(0));
}
std::optional<std::string> StringFmt<2>::valid(std::string const& format) {
    return check_format(format, fmt::make_format_args(0, 0));
}

Fmt::Fmt() { }

void Fmt::check_formats() const {
    std::list<std::pair<std::string, const StringFmtBase*>> formats = {
        { "classes", &classes },
        { "consts", &consts },
        { "functions", &functions },
        { "names", &names },
        { "vars", &vars },
        { "methods", &methods },
        { "errors", &errors },
        { "unknown_recv_packet", &unknown_recv_packet },
        { "packet_subhandler", &packet_subhandler },
        { "recv_packet", &recv_packet },
        { "sent_packet", &sent_packet },
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

Renamer::Renamer(std::shared_ptr<abc::AbcFile> const& abc, Fmt const& fmt) : abc(abc), fmt(fmt) {
    fmt.check_formats();
}

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