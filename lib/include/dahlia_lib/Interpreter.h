#pragma once

#include <expected>

#include "Ast.h"
#include "Position.h"
#include "Signal.h"
#include "Stack.h"
#include "Value.h"
#include "dahlia_lib/RuntimeError.h"

class Interpreter {
public:
    explicit Interpreter() : pos_{} {}

    std::expected<Value, RuntimeError> run(ProgramNode const& program);

private:
    Stack stack_;
    Position pos_;

    Value visitProgram(ProgramNode const& program);

    Value visitFunctionDefinition(FunctionNode const& fun);

    [[nodiscard]] Value visitExpr(ExprNode const& expr) const;

    Signal visitStatement(StatementNode const& statement);

    void visitAssign(AssignStmt const& statement, Position pos);
    Signal visitWhileLoop(WhileLoop const& loop);
    void visitLetBinding(LetBinding const& let, Position pos);

    Signal visitBlock(BlockNode const& block);

    Value visitFunctionCall(FunctionCall const& fun_call);

    [[nodiscard]] EvalResult visitIdentifier(Identifier const& ident) const;

    [[nodiscard]] EvalResult visitVecLiteral(VecLiteral const& lit) const;
};
