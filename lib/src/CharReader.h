#pragma once

#include <istream>

#include "Position.h"

constexpr char STX = '\x02';
constexpr char ETX = '\x03';
constexpr char NEWLINE = '\n';

class CharReader {
public:
    CharReader(std::istream& input);

    char next();
    [[nodiscard]] char current() const noexcept;

    [[nodiscard]] Position position() const noexcept;

private:
    std::istream& input_;  // NOLINT

    char current_;

    Position position_;
};
