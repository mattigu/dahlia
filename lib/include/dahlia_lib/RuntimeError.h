#pragma once

#include <cstddef>
#include <variant>

#include "dahlia_lib/Ast.h"
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

struct VariableRedefinition {
    std::string identifier;
    Position original_pos;
    bool operator==(VariableRedefinition const&) const noexcept = default;
};

struct ConstAssignment {
    std::string identifier;
    bool operator==(ConstAssignment const&) const noexcept = default;
};

struct UseOfUndeclaredVariable {
    bool operator==(UseOfUndeclaredVariable const&) const noexcept = default;
};

struct VecTypeMismatch {
    Type first;
    Type other;
    bool operator==(VecTypeMismatch const&) const noexcept = default;
};

using RuntimeErrorKind =
    std::variant<UnexpectedBreak, UnexpectedContinue, MissingMainFunction,
                 UseOfUndeclaredVariable, ConstAssignment, VecTypeMismatch>;

struct RuntimeError {
    RuntimeErrorKind kind;
    Position pos;

    bool operator==(RuntimeError const&) const noexcept = default;
};
