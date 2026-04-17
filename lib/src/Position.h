#pragma once

#include <ostream>
struct Position {
    int line;
    int column;
    int offset;

    // For testing/debugging
    bool operator==(Position const&) const = default;
    friend std::ostream& operator<<(std::ostream& oss, Position const& pos);
};
