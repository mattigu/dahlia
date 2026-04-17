#include "CharReader.h"

#include "Position.h"

CharReader::CharReader(std::istream& input)
    : input_{input},
      current_{STX},
      position_{.line = 1, .column = 0, .offset = -1} {}

char CharReader::current() const noexcept { return current_; }

Position CharReader::position() const noexcept { return position_; }

char CharReader::next() {
    if (current_ == ETX) {
        return ETX;
    }

    char chr = 0;

    position_.offset += 1;
    position_.column += 1;

    if (input_.get(chr)) {
        if (current_ == NEWLINE) {
            position_.column = 1;
            position_.line += 1;
        }
        current_ = chr;

    } else if (input_.eof()) {
        current_ = ETX;
    } else if (input_.fail()) {
        throw std::runtime_error(
            std::format("Failed to read next character at {}:{} (stream error)",
                        position_.line, position_.column));
    }
    // There is also input_.fail() to check.

    return current_;
}
