#pragma once

#include <ostream>
struct Position {
    int line;
    int column;
    int offset;

    bool operator==(Position const&) const = default;
};

template <>
struct std::formatter<Position> : std::formatter<std::string_view> {
    static auto format(Position const& pos, std::format_context& ctx) {
        return std::format_to(ctx.out(), "{{{}:{} +{}}}", pos.line, pos.column,
                              pos.offset);
    }
};

std::ostream& operator<<(std::ostream& oss, Position const& pos);
