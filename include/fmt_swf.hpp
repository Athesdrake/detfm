#pragma once
#include <cstdint>
#include <fmt/format.h>
#include <swf/swf.hpp>
#include <swf/tags/DoABCTag.hpp>

template <> struct fmt::formatter<swf::DoABCTag> {
    constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}')
            throw format_error("invalid format");
        return it;
    }

    template <typename FormatContext>
    auto format(swf::DoABCTag& tag, FormatContext& ctx) -> decltype(ctx.out()) {
        return format_to(
            ctx.out(),
            "[{tagname}:0x{tagid:02x}] \"{name}\" lazy:{lazy}",
            "tagname"_a = fmt::styled(tag.getTagName(), fmt::fg(fmt::color::dodger_blue)),
            "tagid"_a   = fmt::styled(uint8_t(tag.getId()), fmt::fg(fmt::color::purple)),
            "name"_a    = fmt::styled(tag.name, fmt::fg(fmt::color::olive_drab)),
            "lazy"_a = fmt::styled(tag.is_lazy, fmt::fg(fmt::color::navy) | fmt::emphasis::italic));
    }
};