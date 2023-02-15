#pragma once
#include <abc/parser/Parser.hpp>
#include <fmt/core.h>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace abc = swf::abc;

struct StringFmtBase {
    std::string value;

    StringFmtBase(std::string value) : value(value) { }
    StringFmtBase(const char* value) : value(value) { }

    virtual std::optional<std::string> valid() const = 0;
};

template <int slots> struct StringFmt;
template <> struct StringFmt<1> : public StringFmtBase {
    using StringFmtBase::StringFmtBase;

    static std::optional<std::string> valid(std::string const& format);
    std::optional<std::string> valid() const override { return StringFmt<1>::valid(value); }

    std::string format(int a);
};
template <> struct StringFmt<2> : public StringFmtBase {
    using StringFmtBase::StringFmtBase;

    static std::optional<std::string> valid(std::string const& format);
    std::optional<std::string> valid() const override { return StringFmt<2>::valid(value); }

    std::string format(int a, int b);
};

class Fmt {
public:
    StringFmt<1> classes   = "class_{:03d}";
    StringFmt<1> consts    = "const_{:03d}";
    StringFmt<1> functions = "function_{:03d}";
    StringFmt<1> names     = "name_{:03d}";
    StringFmt<1> vars      = "var_{:03d}";
    StringFmt<1> methods   = "method_{:03d}";
    StringFmt<1> errors    = "error{:d}";

    StringFmt<1> packet_subhandler  = "PacketSubHandler_{:02x}";
    StringFmt<2> clientbound_packet = "CPacket_{:02x}_{:02x}";
    StringFmt<2> serverbound_packet = "SPacket_{:02x}_{:02x}";

    StringFmt<1> unknown_clientbound_packet  = "CPacket_u{:02d}";
    StringFmt<1> tribulle_clientbound_packet = "TCPacket_{:04x}";
    StringFmt<1> tribulle_serverbound_packet = "TSPacket_{:04x}";

    Fmt();

    // throw an error if any format is invalid
    void check_formats() const;
};

// Helper that renames invalid symbols using the format specified
class Renamer {
public:
    Renamer(std::shared_ptr<abc::AbcFile> const& abc, Fmt const& fmt);

    static bool invalid(std::string const& name);
    static bool invalid(std::string_view name);

    // Rename invalid symbols inside the abc file
    void rename();
    // Rename the class's name, its inherited class and all traits if invalids
    void rename(abc::Class& klass);
    // Rename the trait's name if invalid
    void rename(abc::Trait& trait);
    // Rename the method's exceptions and traits if invalids
    void rename(abc::Method& method);
    // Rename the exception variable's name to "error" if invalid
    void rename(abc::Exception& err);
    // Rename the exception variable's name to the specified counter if invalid
    void rename(abc::Exception& err, int counter);

private:
    std::shared_ptr<abc::AbcFile> abc;
    Fmt fmt;
    struct {
        int classes   = 0;
        int consts    = 0;
        int functions = 0;
        int names     = 0;
        int vars      = 0;
        int methods   = 0;
    } counters;
};
