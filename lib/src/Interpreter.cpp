#include "dahlia_lib/Interpreter.h"

#include <cassert>
#include <expected>
#include <ranges>
#include <variant>

#include "dahlia_lib/Ast.h"
#include "dahlia_lib/Position.h"
#include "dahlia_lib/RuntimeError.h"
#include "dahlia_lib/Signal.h"
#include "dahlia_lib/Stack.h"
#include "dahlia_lib/Value.h"
#include "dahlia_lib/Variable.h"

Interpreter::Interpreter(InterpreterOpts opts) : options_(opts) {
    assert(opts.max_call_depth > 0);
}

std::expected<Value, RuntimeError> Interpreter::run(
    ProgramNode const& program) {
    program_ = &(*program);
    try {
        return visitProgram(program);
    } catch (RuntimeError const& err) {
        return std::unexpected(err);
    }
}

Value Interpreter::visitProgram(ProgramNode const& program) {
    auto const main = program->functions.find("main");

    if (main == program->functions.cend()) {
        throw RuntimeError{.kind = MissingMainFunction{}, .pos = program.pos()};
    }
    auto const main_res =
        visitFunctionCall(FunctionCall{.identifier = "main"}, program.pos());
    return main_res;
}

Value Interpreter::visitFunctionDefinition(FunctionNode const& fun) {
    // Verify args and param types
    auto signal = visitBlock(fun->block);

    auto return_val =
        std::visit(Overloaded{
                       [](ReturnSignal const& sig) { return sig.value; },
                       [](std::monostate) { return Value{}; },
                       [&](BreakSignal const& sig) -> Value {
                           throw RuntimeError{.kind = UnexpectedBreak{},
                                              .pos = signal.pos};
                       },
                       [&](ContinueSignal const& sig) -> Value {
                           throw RuntimeError{.kind = UnexpectedContinue{},
                                              .pos = signal.pos};
                       },
                   },
                   signal.kind);

    if (fun->return_type) {
        if (std::holds_alternative<std::monostate>(return_val)) {
            throw RuntimeError{.kind = MissingReturnValue{}, .pos = signal.pos};
        }
        auto const type = typeFor(return_val);

        if (type == **fun->return_type) {
            return return_val;
        }
        auto const coerced = coerce(return_val, **fun->return_type);

        if (!coerced) {
            throw RuntimeError{.kind = coerced.error(), .pos = signal.pos};
        }
        return *coerced;
    }
    if (!std::holds_alternative<std::monostate>(return_val)) {
        throw RuntimeError{.kind = UnexpectedReturnValue{}, .pos = signal.pos};
    }
    return Value{};
}

[[nodiscard]] Value Interpreter::visitExpr(ExprNode const& expr) {
    auto const res = std::visit(
        Overloaded{
            [](IntLiteral const& lit) -> EvalResult { return lit.value; },
            [](BoolLiteral const& lit) -> EvalResult { return lit.value; },
            [](FloatLiteral const& lit) -> EvalResult { return lit.value; },
            [](StringLiteral const& lit) -> EvalResult { return lit.value; },
            [&](VecLiteral const& lit) -> EvalResult {
                return visitVecLiteral(lit);
            },
            [&](Identifier const& ident) -> EvalResult {
                return visitIdentifier(ident);
            },
            [&](FunctionCall const& call) -> EvalResult {
                return visitFunctionCall(call, expr.pos());
            },

            // Arithmetic
            [&](AddExpr const& expr) -> EvalResult {
                return add(visitExpr(*expr.left), visitExpr(*expr.right));
            },
            [&](SubExpr const& expr) -> EvalResult {
                return subtract(visitExpr(*expr.left), visitExpr(*expr.right));
            },
            [&](DivExpr const& expr) -> EvalResult {
                return divide(visitExpr(*expr.left), visitExpr(*expr.right));
            },
            [&](MulExpr const& expr) -> EvalResult {
                return multiply(visitExpr(*expr.left), visitExpr(*expr.right));
            },
            [&](ModExpr const& expr) -> EvalResult {
                return modulo(visitExpr(*expr.left), visitExpr(*expr.right));
            },
            // Comparison
            [&](EqExpr const& expr) {
                return eq(visitExpr(*expr.left), visitExpr(*expr.right));
            },
            [&](NeqExpr const& expr) {
                return neq(visitExpr(*expr.left), visitExpr(*expr.right));
            },
            [&](LtExpr const& expr) {
                return lt(visitExpr(*expr.left), visitExpr(*expr.right));
            },
            [&](GtExpr const& expr) {
                return gt(visitExpr(*expr.left), visitExpr(*expr.right));
            },
            [&](LeExpr const& expr) {
                return le(visitExpr(*expr.left), visitExpr(*expr.right));
            },
            [&](GeExpr const& expr) {
                return ge(visitExpr(*expr.left), visitExpr(*expr.right));
            },

            // Logic
            [&](NotExpr const& expr) {
                return logicalNot(visitExpr(*expr.operand));
            },

            // Short circuiting
            [&](AndExpr const& expr) -> EvalResult {
                return toBool(visitExpr(*expr.left)) &&
                       toBool(visitExpr(*expr.right));
            },
            [&](OrExpr const& expr) -> EvalResult {
                return toBool(visitExpr(*expr.left)) ||
                       toBool(visitExpr(*expr.right));
            },

            // Special
            [&](LengthExpr const& expr) {
                return length(visitExpr(*expr.operand));
            },
            [&](NegExpr const& expr) {
                return negation(visitExpr(*expr.operand));
            },
            [&](IntersectExpr const& expr) {
                return intersect(visitExpr(*expr.left), visitExpr(*expr.right));

            },
            [&](InExpr const& expr) {
                return contains(visitExpr(*expr.left), visitExpr(*expr.right));
            },

            [](auto const& value) -> EvalResult { return Value{}; }

        },
        *expr);
    // Every expression error except function call goes through this.
    if (res == Value{std::monostate{}}) {
        throw RuntimeError{.kind = VoidTypeInExpression{}, .pos = expr.pos()};
    }

    if (res) {
        return res.value();
    }
    throw RuntimeError{.kind = res.error(), .pos = expr.pos()};
}

Signal Interpreter::visitStatement(StatementNode const& statement) {
    return std::visit(
        Overloaded{
            [&](ReturnStmt const& stmt) {
                if (stmt.value) {
                    return Signal{
                        .kind = ReturnSignal{.value = visitExpr(*stmt.value)},
                        .pos = statement.pos()};
                }
                return Signal{.pos = statement.pos()};
            },
            [&](BreakStmt const& stmt) {
                return Signal{.kind = BreakSignal{}, .pos = statement.pos()};
            },
            [&](ContinueStmt const& stmt) {
                return Signal{.kind = ContinueSignal{}, .pos = statement.pos()};
            },
            [this](BlockNode const& block) { return visitBlock(block); },
            [&](FunctionCall const& call) {
                visitFunctionCall(call, statement.pos());
                return Signal{};
            },

            [&](LetBinding const& let) {
                visitLetBinding(let, statement.pos());
                return Signal{};
            },
            [&](WhileLoop const& loop) { return visitWhileLoop(loop); },
            [&](ForLoop const& loop) {
                return visitForLoop(loop, statement.pos());
            },
            [&](IfStmt const& stmt) { return visitIfStmt(stmt); },

            [&](AssignStmt const& assign) {
                visitAssign(assign, statement.pos());
                return Signal{};
            },
            [](auto const& value) { return Signal{}; },

        },
        *statement);
}

void Interpreter::visitAssign(AssignStmt const& statement, Position pos) {
    auto const ident = statement.target.identifier;

    auto* const var = stack_.current().lookupVariable(ident);
    if (var == nullptr) {
        throw RuntimeError{.kind = UseOfUnkownIdentifier{}, .pos = pos};
    }
    if (!var->mut()) {
        throw RuntimeError{.kind = MutViolation{}, .pos = pos};
    }

    // Do indices here later
    auto value = visitExpr(statement.value);

    auto const val_type = typeFor(value);
    auto const var_type = typeFor(var->data());

    auto coerced = coerceIfNeeded(value, var_type);
    if (!coerced) {
        throw RuntimeError{.kind = coerced.error(),
                           .pos = statement.value.pos()};
    }

    var->data() = std::move(*coerced);
}

Signal Interpreter::visitWhileLoop(WhileLoop const& loop) {
    while (toBool(visitExpr(loop.condition))) {
        auto const signal = visitBlock(loop.block);
        if (std::holds_alternative<BreakSignal>(signal.kind)) {
            break;
        }
        if (std::holds_alternative<ReturnSignal>(signal.kind)) {
            return signal;
        }
    }
    return Signal{};
}

Signal Interpreter::visitForLoop(ForLoop const& loop, Position pos) {
    auto const extract_int = [&](Value const& val) -> std::int64_t {
        auto const res = toInt(val);
        if (!res) {
            throw res.error();
        }
        return res.value();
    };
    auto const start = extract_int(visitExpr(loop.range->start));
    auto const end = extract_int(visitExpr(loop.range->end));
    auto const step = extract_int(
        loop.range->step
            .transform([this](ExprNode const& expr) { return visitExpr(expr); })
            .value_or((start < end) ? Value{1} : Value{-1}));
    auto const inclusive = loop.range->inclusive;

    if (start < end && step < 0 || start > end && step > 0 || step == 0 ||
        start == end) {
        throw RuntimeError{.kind = InvalidForRange{}, .pos = loop.range.pos()};
    }
    auto const continue_loop = [&](std::int64_t loop_var) -> bool {
        if (start < end) {
            return inclusive ? loop_var <= end : loop_var < end;
        }
        return inclusive ? loop_var >= end : loop_var > end;
    };

    auto loop_val = start;

    stack_.current().pushScope();
    stack_.current().declareVariable(loop.loop_var,
                                     Variable(loop_val, loop.mut, Position{}));
    auto* const loop_var = stack_.current().lookupVariable(loop.loop_var);

    while (continue_loop(loop_val)) {
        loop_var->data() = loop_val;

        auto const signal = visitBlock(loop.block);
        if (std::holds_alternative<BreakSignal>(signal.kind)) {
            break;
        }
        if (std::holds_alternative<ReturnSignal>(signal.kind)) {
            stack_.current().popScope();
            return signal;
        }

        auto const res = checkedAdd(loop_val, step);
        if (!res) {
            throw RuntimeError{.kind = res.error(), .pos = pos};
        }
        loop_val = res.value();
    }

    stack_.current().popScope();
    return Signal{};
}

Signal Interpreter::visitIfStmt(IfStmt const& stmt) {
    auto const eval_branch =
        [&](ExprNode const& cond,
            BlockNode const& block) -> std::optional<Signal> {
        if (toBool(visitExpr(cond))) {
            return visitBlock(block);
        }
        return std::nullopt;
    };

    if (auto const signal = eval_branch(stmt.if_cond, stmt.if_block)) {
        return *signal;
    }

    for (auto const& [cond, block] : stmt.else_if_branches) {
        if (auto const signal = eval_branch(cond, block)) {
            return *signal;
        }
    }
    if (stmt.else_block) {
        return visitBlock(*stmt.else_block);
    }

    return {};
}

void Interpreter::visitLetBinding(LetBinding const& let, Position pos) {
    auto val = visitExpr(let.value);

    if (let.type && **let.type != typeFor(val)) {
        throw RuntimeError{.kind = AssignmentTypeMismatch{}, .pos = pos};
    }

    auto const duplicate_pos = stack_.current().declareVariable(
        let.identifier, Variable(std::move(val), let.mut, pos));

    if (duplicate_pos) {
        throw RuntimeError{
            .kind = VariableRedefinition{.original_pos = *duplicate_pos},
            .pos = pos};
    }
}

Signal Interpreter::visitBlock(BlockNode const& block) {
    stack_.current().pushScope();

    for (auto const& statement : block->statements) {
        auto signal = visitStatement(statement);
        if (!std::holds_alternative<std::monostate>(signal.kind)) {
            stack_.current().popScope();
            return signal;
        }
    }

    stack_.current().popScope();
    return Signal{.pos = block.pos()};
}

Value Interpreter::visitFunctionCall(FunctionCall const& fun_call,
                                     Position pos) {
    if (stack_.callDepth() >= options_.max_call_depth) {
        throw RuntimeError{.kind = CallDepthExceeded{}, .pos = pos};
    }

    auto const iter = program_->functions.find(fun_call.identifier);
    if (iter == program_->functions.cend()) {
        throw RuntimeError{.kind = UseOfUnkownIdentifier{}, .pos = pos};
    }
    auto const& fun = iter->second;

    if (fun->params.size() != fun_call.args.size()) {
        throw RuntimeError{.kind = ArgumentCountMismatch{}, .pos = pos};
    }

    std::vector<Variable> args_vars;
    args_vars.reserve(fun_call.args.size());

    for (auto const& [param, arg] :
         std::views::zip(fun->params, fun_call.args)) {
        if (param->mut) {
            auto const* ident = std::get_if<Identifier>(&*arg);
            if (ident == nullptr) {
                throw RuntimeError{.kind = MutArgExpression{},
                                   .pos = arg.pos()};
            }
            auto* var = stack_.current().lookupVariable(ident->identifier);
            if (!var->mut()) {
                throw RuntimeError{.kind = MutViolation{}, .pos = arg.pos()};
            }
            args_vars.emplace_back(&var->data(), true, arg.pos());
        } else {
            auto coerced = coerceIfNeeded(visitExpr(arg), *param->type);
            if (!coerced) {
                throw RuntimeError{.kind = coerced.error(), .pos = arg.pos()};
            }

            args_vars.emplace_back(std::move(*coerced), false, arg.pos());
        }
    }

    stack_.pushContext();
    stack_.current().pushScope();

    for (auto const& [param, var] : std::views::zip(fun->params, args_vars)) {
        stack_.current().declareVariable(param->identifier, std::move(var));
    }

    auto res = visitFunctionDefinition(fun);

    stack_.current().pushScope();
    stack_.popContext();

    return res;
}

EvalResult Interpreter::visitIdentifier(Identifier const& ident) const {
    auto const* val = stack_.current().lookupValue(ident.identifier);
    if (val == nullptr) {
        return std::unexpected(UseOfUnkownIdentifier{});
    }
    return *val;
}

EvalResult Interpreter::visitVecLiteral(VecLiteral const& lit) {
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
            return std::unexpected(VecTypeMismatch{.first = expected_type,
                                                   .other = typeFor(value)});
        }
        elements.push_back(std::move(value));
    }

    return VecValue{.type = std::move(expected_type),
                    .elements = std::move(elements)};
}

EvalResult Interpreter::coerceIfNeeded(Value val, Type const& target) {
    if (typeFor(val) == target) {
        return val;
    }
    return coerce(val, target);
}
