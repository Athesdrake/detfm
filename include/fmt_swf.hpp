#pragma once
#include <cstdint>
#include <fmt/format.h>
#include <swf/swf.hpp>
#include <swf/tags/DoABCTag.hpp>

template <> struct fmt::formatter<swf::DoABCTag> : formatter<string_view> {
    constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}')
            throw format_error("invalid format");
        return it;
    }

    auto format(swf::DoABCTag& tag, format_context& ctx) const -> format_context::iterator {
        return format_to(
            ctx.out(),
            "[{tagname}:0x{tagid:02x}] \"{name}\" lazy:{lazy}",
            "tagname"_a = fmt::styled(tag.getTagName(), fmt::fg(fmt::color::dodger_blue)),
            "tagid"_a   = fmt::styled(uint8_t(tag.getId()), fmt::fg(fmt::color::purple)),
            "name"_a    = fmt::styled(tag.name, fmt::fg(fmt::color::olive_drab)),
            "lazy"_a = fmt::styled(tag.is_lazy, fmt::fg(fmt::color::navy) | fmt::emphasis::italic));
    }
};