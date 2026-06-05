#pragma once

#include <expected>

#include "Ast.h"
#include "Builtins.h"
#include "Position.h"
#include "Signal.h"
#include "Stack.h"
#include "Value.h"
#include "dahlia_lib/RuntimeError.h"

struct InterpreterOpts {
    int max_call_depth;
};

class Interpreter {
public:
    explicit Interpreter(BuiltinMap builtins,
                         InterpreterOpts opts = InterpreterOpts{
                             .max_call_depth = 100});

    std::expected<Value, RuntimeError> run(ProgramNode const& program);

    StackTrace stackTrace() const noexcept;

private:
    Stack stack_;
    InterpreterOpts options_;
    BuiltinMap builtins_;
    Program const* program_ = nullptr;

    static EvalResult coerceIfNeeded(Value val, Type const& target);

    Value visitProgram(ProgramNode const& program);

    Value visitFunctionDefinition(FunctionNode const& fun);

    [[nodiscard]] Value visitExpr(ExprNode const& expr);
    Value visitFilterExpr(FilterExpr const& expr, Position pos);
    Value visitMapExpr(MapExpr const& expr, Position pos);

    Value callBuiltin(BuiltinFunction const& builtin,
                      std::vector<ExprNode> const& arg_exprs, Position pos);

    Signal visitStatement(StatementNode const& statement);

    void visitAssign(AssignStmt const& statement, Position pos);
    Value& visitLValue(LValue const& lval, Position pos);

    Signal visitWhileLoop(WhileLoop const& loop);
    Signal visitForLoop(ForLoop const& loop, Position pos);
    Signal visitIfStmt(IfStmt const& stmt);

    void visitLetBinding(LetBinding const& let, Position pos);

    Signal visitBlock(BlockNode const& block);

    Value visitFunctionCall(FunctionCall const& fun_call, Position pos);

    [[nodiscard]] EvalResult visitIdentifier(Identifier const& ident) const;

    [[nodiscard]] EvalResult visitVecLiteral(VecLiteral const& lit);


    template <typename Op>
    void visitCompoundAssign(ExprNode const& value, LValue const& target,
                             Position pos, Op op) {  // NOLINT
        auto val = visitExpr(value);
        auto& lval = visitLValue(target, pos);
        auto const res = op(lval, std::move(val));
        if (!res) {
            throw RuntimeError{.kind = res.error(), .pos = pos};
        }
        lval = std::move(*res);
    }
};
