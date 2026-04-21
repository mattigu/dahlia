#pragma once
#include <variant>

struct UnterminatedString {
    bool operator==(UnterminatedString const&) const = default;
};
struct InvalidEscapeSequence {
    char chr;
    bool operator==(InvalidEscapeSequence const&) const = default;
};
struct InvalidHexEscape {
    char chr;
    bool operator==(InvalidHexEscape const&) const = default;

};

using LexerDiagnosticKind =
    std::variant<UnterminatedString, InvalidEscapeSequence, InvalidHexEscape>;
