
#include "dahlia_lib/DiagnosticMessages.h"

#include <format>
#include <variant>

#include "dahlia_lib/Ast.h"
#include "dahlia_lib/LexerDiagnostics.h"
#include "dahlia_lib/ParserDiagnostics.h"
#include "dahlia_lib/RuntimeError.h"

template <class... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};

std::string messageFor(LexerDiagnosticKind const& diag) {
    return std::visit(

        Overloaded{[](UnterminatedString const&) {
                       return std::string("unterminated string");
                   },
                   [](InvalidEscapeSequence const& diag) {
                       return std::format("invalid escape sequence '{}'",
                                          diag.chr);
                   },
                   [](InvalidHexEscape const& diag) {
                       return std::format("invalid hexadecimal character '{}'",
                                          diag.chr);
                   },
                   [](UnexpectedChar const& diag) {
                       return std::format("unexpected char '{}'", diag.chr);
                   },
                   [](ExpectedChar const& diag) {
                       return std::format("expected '{}', got '{}'",
                                          diag.expected, diag.got);
                   },
                   [](InvalidNumericSeparator const& diag) {
                       return std::string("invalid numeric separator");
                   },
                   [](IntegerOverflow const& diag) {
                       return std::string("integer overflow");
                   },
                   [](FloatOutOfRange const& diag) {
                       return std::string("float out of range");
                   },
                   [](StringLiteral const& diag) {
                       return std::string("string too long");
                   },
                   [](CommentTooLong const& diag) {
                       return std::string("comment too long");
                   },
                   [](StringTooLong const& diag) {
                       return std::string("comment too long");
                   },
                   [](IdentifierTooLong const& diag) {
                       return std::string("identifier too long");
                   }},
        diag);
}

std::string messageFor(ParserDiagnosticKind const& diag) {
    return std::visit(
        Overloaded{
            [](ExpectedToken const& diag) {
                return std::format(R"(expected token "{}", got "{}")",
                                   diag.expected, diag.got);
            },
            [](ExpectedType const& diag) {
                return std::string("expected type");
            },
            [](ExpectedExpression const& diag) {
                return std::string("expected expression");
            },
            [](FunctionRedefined const& diag) {
                return std::format(
                    "redefinition of function \"{}\" (previously defined "
                    "at {})",
                    diag.identifier, diag.original_pos);
            },
            [](ExpectedIdentifier const& diag) {
                return std::string("expected identifier");
            },
            [](ExpectedBlock const& diag) {
                return std::string("expected block");
            },
            [](ParameterRedefined const& diag) {
                return std::format("duplicate function parameter \"{}\"",
                                   diag.identifier);
            }},

        diag);
}

std::string messageFor(RuntimeErrorKind const& err) {
    return std::visit(

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
                return std::format("use of unknown identifier {}",
                                   err.identifier);
            },
            [](VecTypeMismatch const& err) {
                return std::string("values in vec are not of the same type");
            },
            [](ArithmeticOverflow const& err) {
                return std::string("overflow in arithmetic expression");
            },
            [](DivisionByZero const& err) {
                return std::string("division by 0");
            },
            [](InvalidOperands const& err) {
                return std::format("invalid operands {}, {}", err.lhs, err.rhs);
            },
            [](InvalidOperand const& err) {
                return std::format("invalid operands {}", err.type);
            },
            [](UnparsableString const& err) {
                return std::format(R"(string "{}" unparsable to "{}")", err.val,
                                   err.targetType);
            },
            [](InvalidConversion const& err) {
                return std::format("cannot convert from {} to {}", err.from,
                                   err.to);
            },
            [](InvalidForRange const& err) {
                return std::string("invalid for range");
            },
            [](LetBindingTypeMismatch const& err) {
                return std::string(
                    "types must explictly match with the type annotation");
            },
            [](VariableRedefinition const& err) {
                return std::format(
                    "redefinition of variable \"{}\" (previously defined "
                    "at {})",
                    err.identifier, err.original_pos);
            },
            [](CallDepthExceeded const& err) {
                return std::format("max call depth of {} exceeded",
                                   err.max_depth);
            },
            [](VoidTypeInExpression const& err) {
                return std::string("void type in expression");
            },
            [](ImmutablePassedToMut const& err) {
                return std::string(
                    "cannot pass immutable variable to mutable parameter");
            },
            [](AssignmentToImmutable const& err) {
                return std::string("cannot assign to immutable variable");
            },

            [](MutArgExpression const& err) {
                return std::string(
                    "cannot pass expression to a mutable parameter");
            },
            [](ArgumentCountMismatch const& err) {
                return std::format("expected {} arguments, got {}",
                                   err.expected, err.got);
            },
            [](MissingReturnValue const& err) {
                return std::string("missing return value");
            },

            [](UnexpectedReturnValue const& err) {
                return std::string("unexpected return value");
            },
            [](CannotInferEmptyVec const& err) {
                return std::string(
                    "cannot infer type of empty vector, type annotation "
                    "required");
            },
            [](MutArgTypeMismatch const& err) {
                return std::string(
                    "mutable parameters require exact type match");
            },
            [](BuiltinRedifined const& err) {
                return std::format("redefinition of builtin function \"{}\"",
                                   err.identifier);
            },
            [](IndexOutOfBounds const& err) {
                return std::format("index {} is out of bounds", err.index);
            },
            [](ExpectedFunction const& err) {
                return std::string(
                    "operator requires a function as the right-hand operand");
            },

            [](VoidFunctionInMapFilter const&) {
                return std::string(
                    "function passed to map/filter must return a value");
            }},

        err);
}
