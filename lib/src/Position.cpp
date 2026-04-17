#include "Position.h"

#include <format>

std::ostream& operator<<(std::ostream& oss, Position const& pos) {
    return oss << std::format("{{line={}, column={}, offset={}}}", pos.line,
                              pos.column, pos.offset);
}
