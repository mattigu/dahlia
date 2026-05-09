#pragma once
#include <variant>

#include "Token.h"
#include "src/Position.h"

struct ExpectedToken {
    TokenKind expected;
    TokenKind got;

    bool operator==(ExpectedToken const&) const = default;
};

struct ExpectedType {
    bool operator==(ExpectedType const&) const = default;
};

struct ExpectedExpression {
    bool operator==(ExpectedExpression const&) const = default;
};

struct FunctionRedefined {
    std::string identifier;
    Position original_pos;

    bool operator==(FunctionRedefined const&) const = default;
};

using ParserDiagnosticKind =
    std::variant<ExpectedToken, ExpectedType, ExpectedExpression, FunctionRedefined>;
