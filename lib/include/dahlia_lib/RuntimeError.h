#pragma once

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
    Position original_pos;
    bool operator==(VariableRedefinition const&) const noexcept = default;
};

struct UseOfUnkownIdentifier {
    bool operator==(UseOfUnkownIdentifier const&) const noexcept = default;
};

struct VecTypeMismatch {
    Type first;
    Type other;
    bool operator==(VecTypeMismatch const&) const noexcept = default;
};

struct AssignmentTypeMismatch {
    bool operator==(AssignmentTypeMismatch const&) const noexcept = default;
};

struct ArithmeticOverflow {
    bool operator==(ArithmeticOverflow const&) const noexcept = default;
};

struct DivisionByZero {
    bool operator==(DivisionByZero const&) const noexcept = default;
};

struct InvalidOperands {
    Type lhs;
    Type rhs;

    bool operator==(InvalidOperands const&) const noexcept = default;
};

struct InvalidOperand {
    Type type;

    bool operator==(InvalidOperand const&) const noexcept = default;
};

struct UnparsableString {
    std::string val;
    Type targetType;

    bool operator==(UnparsableString const&) const noexcept = default;
};

struct InvalidConversion {
    Type from;
    Type to;

    bool operator==(InvalidConversion const&) const noexcept = default;
};

struct InvalidForRange {
    bool operator==(InvalidForRange const&) const noexcept = default;
};

struct CallDepthExceeded {
    bool operator==(CallDepthExceeded const&) const noexcept = default;
};

struct VoidTypeInExpression {
    bool operator==(VoidTypeInExpression const&) const noexcept = default;
};

struct MutViolation {
    bool operator==(MutViolation const&) const noexcept = default;
};

struct MutArgExpression {
    bool operator==(MutArgExpression const&) const noexcept = default;
};

struct ArgumentCountMismatch {
    bool operator==(ArgumentCountMismatch const&) const noexcept = default;
};

struct MissingReturnValue {
    bool operator==(MissingReturnValue const&) const noexcept = default;
};

struct UnexpectedReturnValue {
    bool operator==(UnexpectedReturnValue const&) const noexcept = default;
};

struct CannotInferEmptyVec {
    bool operator==(CannotInferEmptyVec const&) const noexcept = default;
};

struct MutArgTypeMismatch {
    bool operator==(MutArgTypeMismatch const&) const noexcept = default;
};

using RuntimeErrorKind = std::variant<
    UnexpectedBreak, UnexpectedContinue, MissingMainFunction,
    UseOfUnkownIdentifier, VecTypeMismatch, ArithmeticOverflow, DivisionByZero,
    InvalidOperands, InvalidOperand, UnparsableString, InvalidConversion,
    InvalidForRange, AssignmentTypeMismatch, VariableRedefinition,
    CallDepthExceeded, VoidTypeInExpression, MutViolation, MutArgExpression,
    ArgumentCountMismatch, MissingReturnValue, UnexpectedReturnValue,
    CannotInferEmptyVec, MutArgTypeMismatch>;

struct RuntimeError {
    RuntimeErrorKind kind;
    Position pos;

    bool operator==(RuntimeError const&) const noexcept = default;
};
