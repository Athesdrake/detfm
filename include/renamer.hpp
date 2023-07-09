#pragma once
#include "utils.hpp"
#include <abc/parser/Parser.hpp>
#include <fmt/core.h>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>

namespace athes::detfm {
namespace abc = swf::abc;
using json    = nlohmann::json;

std::optional<std::string> check_format(std::string const& format, fmt::format_args args);
struct StringFmtBase {
    std::string value;

    StringFmtBase(std::string value) : value(value) { }
    StringFmtBase(const char* value) : value(value) { }

    virtual std::optional<std::string> valid(std::string const&) const = 0;
    std::optional<std::string> valid() const { return valid(value); }
};

template <typename... Types> struct StringFmt : public StringFmtBase {
    using StringFmtBase::StringFmtBase;

    std::optional<std::string> valid(std::string const& format) const override {
        return check_format(format, fmt::make_format_args(Types()...));
    }

    std::string format(Types... args) { return fmt::format(value, args...); }
};

class Fmt {
public:
    StringFmt<uint32_t> classes   = "class_{:03d}";
    StringFmt<uint32_t> consts    = "const_{:03d}";
    StringFmt<uint32_t> functions = "function_{:03d}";
    StringFmt<uint32_t> names     = "name_{:03d}";
    StringFmt<uint32_t> vars      = "var_{:03d}";
    StringFmt<uint32_t> methods   = "method_{:03d}";
    StringFmt<uint32_t> errors    = "error{:d}";

    StringFmt<uint8_t, uint8_t, std::string> clientbound_packet  = "CPacket{:02x}{:02x}{}";
    StringFmt<uint8_t, uint8_t, std::string> serverbound_packet  = "SPacket{:02x}{:02x}{}";
    StringFmt<uint16_t, std::string> tribulle_clientbound_packet = "TCPacket_{:04x}{}";
    StringFmt<uint16_t, std::string> tribulle_serverbound_packet = "TSPacket_{:04x}{}";

    StringFmt<uint16_t> packet_subhandler          = "PacketSubHandler_{:02x}";
    StringFmt<uint16_t> unknown_clientbound_packet = "CPacket_u{:02d}";

    Fmt();

    void from_json(json& config, utils::Logger& logger);
    static json to_json();

protected:
    using Fields = std::vector<std::pair<std::string, StringFmtBase*>>;
    Fields get_fields();
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
}