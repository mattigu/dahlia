#pragma once
#include <variant>

#include "Token.h"

struct ExpectedToken {
    TokenKind expected;
    TokenKind got;

    bool operator==(ExpectedToken const&) const = default;
};

struct ExpectedType {
    bool operator==(ExpectedType const&) const = default;
};

using ParserDiagnosticKind = std::variant<ExpectedToken, ExpectedType>;
