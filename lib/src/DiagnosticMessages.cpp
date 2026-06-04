
#include "dahlia_lib/DiagnosticMessages.h"

#include <variant>

#include "dahlia_lib/RuntimeError.h"

template <class... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};

std::string messageFor(LexerDiagnosticKind const& diag) {
    return std::visit(
        Overloaded{[](auto const&) { return std::string("No message yet"); }},
        diag);
}
std::string messageFor(ParserDiagnosticKind const& diag) {
    return std::visit(
        Overloaded{
            Overloaded{
                [](auto const&) { return std::string("No message yet"); }},

        },
        diag);
}

// UnexpectedBreak, UnexpectedContinue, MissingMainFunction,
//                  UseOfUnkownIdentifier, VecTypeMismatch, ArithmeticOverflow,
//                  DivisionByZero, InvalidOperands, InvalidOperand,
//                  UnparsableString, InvalidConversion, InvalidForRange,
//                  AssignmentTypeMismatch, VariableRedefinition,
//                  CallDepthExceeded, VoidTypeInExpression, MutViolation,
//                  MutArgExpression, ArgumentCountMismatch, MissingReturnValue,
//                  UnexpectedReturnValue, CannotInferEmptyVec,
//                  MutArgTypeMismatch, BuiltinRedifined, IndexOutOfBounds,
//                  ExpectedFunction, VoidMapFunction
std::string messageFor(RuntimeErrorKind const& err) {
    return std::visit(
        Overloaded{
            Overloaded{
                [](UnexpectedContinue const& err) {
                    return std::string(
                        "\"continue\" used outside of "
                        "loop.");
                },
                [](UnexpectedBreak const& err) {
                    return std::string(
                        "\"break\" used outside of "
                        "loop.");
                },
                [](MissingMainFunction const& err) {
                    return std::string("no main function found");
                },
                [](UseOfUnkownIdentifier const& err) {
                    return std::string("use of unknown identifier");
                },
                [](VecTypeMismatch const& err) {
                    return std::string(
                        "values in vec are not of the same type");
                },
                [](ArithmeticOverflow const& err) {
                    return std::string("overflow in arithmetic expression");
                },
                [](DivisionByZero const& err) {
                    return std::string("division by 0");
                },
                [](auto const&) { return std::string("No message yet"); }},

        },
        err);
}
