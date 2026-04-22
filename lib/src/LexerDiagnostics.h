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

struct UnexpectedChar {
    char chr;
    bool operator==(UnexpectedChar const&) const = default;
};

struct ExpectedChar {
    char expected;
    char got;
    bool operator==(ExpectedChar const&) const = default;
};

struct InvalidNumericSeparator {
    bool operator==(InvalidNumericSeparator const&) const = default;
};

struct InvalidNumericLiteral {
    bool operator==(InvalidNumericLiteral const&) const = default;
};

struct IntegerOverflow {
    bool operator==(IntegerOverflow const&) const = default;
};

struct FloatOutOfRange {
    bool operator==(FloatOutOfRange const&) const = default;
};

using LexerDiagnosticKind =
    std::variant<UnterminatedString, InvalidEscapeSequence, InvalidHexEscape,
                 UnexpectedChar, ExpectedChar, InvalidNumericSeparator,
                 InvalidNumericLiteral, IntegerOverflow, FloatOutOfRange>;
