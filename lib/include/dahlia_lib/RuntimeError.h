#pragma once

#include <variant>

#include "dahlia_lib/Position.h"

struct UnexpectedContinue {
    bool operator==(UnexpectedContinue const&) const noexcept = default;
};

struct UnexpectedBreak {
    bool operator==(UnexpectedBreak const&) const noexcept = default;
};

struct MissingMainFunction {
    bool operator==(MissingMainFunction const&) const noexcept = default;
};

struct UsedUndeclaredVariable {
    bool operator==(UsedUndeclaredVariable const&) const noexcept = default;
};

using RuntimeErrorKind =
    std::variant<UnexpectedBreak, UnexpectedContinue, MissingMainFunction,
                 UsedUndeclaredVariable>;

struct RuntimeError {
    RuntimeErrorKind kind;
    Position pos;

    bool operator==(RuntimeError const&) const noexcept = default;
};
