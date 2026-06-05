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
    std::string identifier;
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

struct LetBindingTypeMismatch {
    bool operator==(LetBindingTypeMismatch const&) const noexcept = default;
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
    int max_depth;

    bool operator==(CallDepthExceeded const&) const noexcept = default;
};

struct VoidTypeInExpression {
    bool operator==(VoidTypeInExpression const&) const noexcept = default;
};

struct AssignmentToImmutable {
    bool operator==(AssignmentToImmutable const&) const noexcept = default;
};

struct ImmutablePassedToMut {
    bool operator==(ImmutablePassedToMut const&) const noexcept = default;
};

struct MutArgExpression {
    bool operator==(MutArgExpression const&) const noexcept = default;
};

struct ArgumentCountMismatch {
    int expected;
    int got;

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

struct BuiltinRedifined {
    std::string identifier;
    bool operator==(BuiltinRedifined const&) const noexcept = default;
};

struct IndexOutOfBounds {
    std::int64_t index;
    bool operator==(IndexOutOfBounds const&) const noexcept = default;
};

struct ExpectedFunction {
    bool operator==(ExpectedFunction const&) const noexcept = default;
};

struct VoidFunctionInMapFilter {
    bool operator==(VoidFunctionInMapFilter const&) const noexcept = default;
};

using RuntimeErrorKind =
    std::variant<UnexpectedBreak, UnexpectedContinue, MissingMainFunction,
                 UseOfUnkownIdentifier, VecTypeMismatch, ArithmeticOverflow,
                 DivisionByZero, InvalidOperands, InvalidOperand,
                 UnparsableString, InvalidConversion, InvalidForRange,
                 LetBindingTypeMismatch, VariableRedefinition,
                 AssignmentToImmutable, ImmutablePassedToMut, CallDepthExceeded,
                 VoidTypeInExpression, MutArgExpression, ArgumentCountMismatch,
                 MissingReturnValue, UnexpectedReturnValue, CannotInferEmptyVec,
                 MutArgTypeMismatch, BuiltinRedifined, IndexOutOfBounds,
                 ExpectedFunction, VoidFunctionInMapFilter>;

struct RuntimeError {
    RuntimeErrorKind kind;
    Position pos;

    bool operator==(RuntimeError const&) const noexcept = default;
};
