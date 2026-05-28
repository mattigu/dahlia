#include "dahlia_lib/Interpreter.h"

#include <expected>
#include <ranges>

#include "dahlia_lib/Ast.h"
#include "dahlia_lib/RuntimeError.h"
#include "dahlia_lib/Signal.h"
#include "dahlia_lib/Stack.h"

std::expected<Value, RuntimeError> Interpreter::run(
    ProgramNode const& program) {
    try {
        return visitProgram(program);
    } catch (RuntimeError const& err) {
        return std::unexpected(err);
    }
}

Value Interpreter::visitProgram(ProgramNode const& program) {
    // parse builtins

    auto const main = program->functions.find("main");

    if (main == program->functions.cend()) {
        throw RuntimeError{.kind = MissingMainFunction{}, .pos = program.pos()};
    }
    // Verify main params/return type?
    // Create global call context
    stack_.pushContext();
    auto const main_res = visitFunctionDefinition(main->second);
    stack_.popContext();
    return main_res;
}

Value Interpreter::visitFunctionDefinition(FunctionNode const& fun) {
    // Verify args and param types
    auto signal = visitBlock(fun->block);

    return std::visit(Overloaded{
                          [](ReturnSignal const& sig) { return sig.value; },
                          [](std::monostate) { return Value{}; },
                          [](BreakSignal const& sig) -> Value {
                              throw RuntimeError{.kind = UnexpectedBreak{},
                                                 .pos = sig.pos};
                          },
                          [](ContinueSignal const& sig) -> Value {
                              throw RuntimeError{.kind = UnexpectedContinue{},
                                                 .pos = sig.pos};
                          },
                      },
                      signal);
}

[[nodiscard]] Value Interpreter::visitExpr(ExprNode const& expr) const {
    return std::visit(
        Overloaded{

            [](IntLiteral const& lit) { return Value(lit.value); },
            [](BoolLiteral const& lit) { return Value(lit.value); },
            [](FloatLiteral const& lit) { return Value(lit.value); },
            [](StringLiteral const& lit) { return Value(lit.value); },
            [this](VecLiteral const& lit) { return visitVecLiteral(lit); },
            [this](Identifier const& ident) { return visitIdentifier(ident); },

            [](auto const& value) { return Value{}; }

        },
        *expr);
}

Signal Interpreter::visitStatement(StatementNode const& statement) {
    return std::visit(
        Overloaded{
            [this](ReturnStmt const& stmt) {
                if (stmt.value) {
                    return Signal{ReturnSignal{visitExpr(*stmt.value)}};
                }
                return Signal{ReturnSignal{}};
            },
            [&](BreakStmt const& stmt) {
                return Signal{BreakSignal{statement.pos()}};
            },
            [&](ContinueStmt const& stmt) {
                return Signal{ContinueSignal{statement.pos()}};
            },
            [this](BlockNode const& block) { return visitBlock(block); },
            [this](LetBinding const& let) {
                visitLetBinding(let);
                return Signal{};
            },

            [](auto const& value) { return Signal{}; },

        },
        *statement);
}

void Interpreter::visitLetBinding(LetBinding const& let) {}

Signal Interpreter::visitBlock(BlockNode const& block) {
    stack_.current().pushScope();

    for (auto const& statement : block->statements) {
        auto signal = visitStatement(statement);
        if (signal != Signal{std::monostate{}}) {
            stack_.current().popScope();
            return signal;
        }
    }

    stack_.current().popScope();
    return Signal{};
}

Value Interpreter::visitFunctionCall(FunctionCall const& fun_call) {
    stack_.pushContext();
    // evaluate args

    std::vector<Value> args;
    args.reserve(fun_call.args.size());
    for (auto const& arg : fun_call.args) {
        args.push_back(visitExpr(arg));
    }

    // verify args to params

    // push new context

    // Visit definition

    stack_.popContext();
}

Value Interpreter::visitIdentifier(Identifier const& ident) const {
    auto const* val = stack_.current().lookupValue(ident.identifier);
    if (val == nullptr) {
        // throw RuntimeError{.kind = UsedUndeclaredVariable{}, .pos=ident.}
    }
    return *val;
}

Value Interpreter::visitVecLiteral(VecLiteral const& lit) const {
    if (lit.elements.empty()) {
        return UninitVec{};
    }

    auto first_elem = visitExpr(lit.elements[0]);
    auto expected_type = typeFor(first_elem);

    std::vector<Value> elements;
    elements.reserve(lit.elements.size());
    elements.push_back(std::move(first_elem));

    for (auto const& elem : lit.elements | std::views::drop(1)) {
        auto value = visitExpr(elem);
        if (typeFor(value) != expected_type) {
            // Type error
        }
        elements.push_back(std::move(value));
    }

    return VecValue{.type = std::move(expected_type),
                    .elements = std::move(elements)};
}
