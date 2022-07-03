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
            "tagname"_a = tag.getTagName(),
            "tagid"_a   = uint8_t(tag.getId()),
            "name"_a    = tag.name,
            "lazy"_a    = tag.is_lazy);
    }
};