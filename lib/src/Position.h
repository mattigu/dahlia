#pragma once

#include <ostream>
struct Position {
    int line;
    int column;
    int offset;

    bool operator==(Position const&) const = default;
};

template <>
struct std::formatter<Position> : std::formatter<std::string> {
    auto format(Position const& pos, std::format_context& ctx) const {
        return std::formatter<std::string>::format(
            std::format("{{line={}, column={}, offset={}}}", pos.line,
                        pos.column, pos.offset),
            ctx);
    }
};

std::ostream& operator<<(std::ostream& oss, Position const& pos);
