#include "dahlia_lib/Interpreter.h"

#include <cassert>
#include <expected>
#include <optional>
#include <ranges>
#include <utility>
#include <variant>
#include <vector>

#include "dahlia_lib/Ast.h"
#include "dahlia_lib/Position.h"
#include "dahlia_lib/RuntimeError.h"
#include "dahlia_lib/Signal.h"
#include "dahlia_lib/Stack.h"
#include "dahlia_lib/Value.h"
#include "dahlia_lib/Variable.h"

Interpreter::Interpreter(BuiltinMap builtins, InterpreterOpts opts)
    : options_(opts), builtins_(std::move(builtins)) {
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

StackTrace Interpreter::stackTrace() const noexcept {
    return stack_.stackTrace();
}

Value Interpreter::visitProgram(ProgramNode const& program) {
    auto const main = program->functions.find("main");

    if (main == program->functions.cend()) {
        throw RuntimeError{.kind = MissingMainFunction{}, .pos = program.pos()};
    }

    auto const duplicate = std::ranges::find_if(
        program->functions,
        [&](auto const& val) { return builtins_.contains(val.first); });

    if (duplicate != program->functions.end()) {
        throw RuntimeError{
            .kind = BuiltinRedifined{.identifier = duplicate->first},
            .pos = duplicate->second.pos()};
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
            [&](IndexExpr const& expr) -> EvalResult {
                auto lhs = visitExpr(*expr.object);
                auto const res = index(lhs, visitExpr(*expr.index));
                if (!res) {
                    return std::unexpected(res.error());
                }
                return *res.value();
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
            [&](FilterExpr const& filter) -> EvalResult {
                return visitFilterExpr(filter, expr.pos());
            },
            [&](MapExpr const& map) -> EvalResult {
                return visitMapExpr(map, expr.pos());
            },

            [](auto const& value) -> EvalResult { return Value{}; }

        },
        *expr);
    // Every expression error except function call, MapExpr and Filter goes
    // through this.
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

            [&](AddAssignStmt const& assign) {
                visitCompoundAssign(assign.value, assign.target,
                                    statement.pos(), add);
                return Signal{};
            },
            [&](SubAssignStmt const& assign) {
                visitCompoundAssign(assign.value, assign.target,
                                    statement.pos(), subtract);
                return Signal{};
            },
            [&](DivAssignStmt const& assign) {
                visitCompoundAssign(assign.value, assign.target,
                                    statement.pos(), divide);
                return Signal{};
            },
            [&](MulAssignStmt const& assign) {
                visitCompoundAssign(assign.value, assign.target,
                                    statement.pos(), multiply);
                return Signal{};
            },
            [&](ModAssignStmt const& assign) {
                visitCompoundAssign(assign.value, assign.target,
                                    statement.pos(), modulo);
                return Signal{};
            },

            [](auto const& value) { return Signal{}; },

        },
        *statement);
}

void Interpreter::visitAssign(AssignStmt const& statement, Position pos) {
    auto& val_ref = visitLValue(statement.target, pos);

    auto value = visitExpr(statement.value);

    auto const val_type = typeFor(value);
    auto const var_type = typeFor(val_ref);

    if (isCoercibleEmptyVec(val_type)) {
        auto coerced = coerceVec(std::move(value), var_type);

        if (!coerced) {
            throw RuntimeError{
                .kind = InvalidConversion{.from = val_type, .to = var_type},
                .pos = pos};
        }
        val_ref = std::move(*coerced);

    } else {
        auto coerced = coerceIfNeeded(value, var_type);
        if (!coerced) {
            throw RuntimeError{.kind = coerced.error(),
                               .pos = statement.value.pos()};
        }
        val_ref = std::move(*coerced);
    }
}

Value& Interpreter::visitLValue(LValue const& lval, Position pos) {
    auto* const var = stack_.current().lookupVariable(lval.identifier);
    if (var == nullptr) {
        throw RuntimeError{.kind = UseOfUnkownIdentifier{}, .pos = pos};
    }
    if (!var->mut()) {
        throw RuntimeError{.kind = AssignmentToImmutable{}, .pos = pos};
    }

    Value* val = &var->data();
    for (auto const& indice_expr : lval.indices) {
        auto const indice = visitExpr(indice_expr);
        auto indexed = index(*val, indice);
        if (!indexed) {
            throw RuntimeError{.kind = indexed.error(),
                               .pos = indice_expr.pos()};
        }
        val = indexed.value();
    }
    return *val;
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
            throw RuntimeError{.kind = res.error(), .pos = loop.range.pos()};
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
    auto val_type = typeFor(val);

    if (!let.type && isCoercibleEmptyVec(val_type)) {
        throw RuntimeError{.kind = CannotInferEmptyVec{}, .pos = pos};
    }

    // For cases when:  let : [int] = []
    // Convert the EmptyVec into a vec of ints.
    if (let.type && (**let.type).isVec()) {
        if (auto coerced = coerceVec(std::move(val), **let.type)) {
            val = std::move(*coerced);
            val_type = **let.type;
        }
    }

    if (let.type && **let.type != val_type) {
        throw RuntimeError{.kind = LetBindingTypeMismatch{}, .pos = pos};
    }

    auto const duplicate_pos = stack_.current().declareVariable(
        let.identifier, Variable(std::move(val), let.mut, pos));

    if (duplicate_pos) {
        throw RuntimeError{
            .kind = VariableRedefinition{.identifier = let.identifier,
                                         .original_pos = *duplicate_pos},
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
        throw RuntimeError{
            .kind = CallDepthExceeded{.max_depth = options_.max_call_depth},
            .pos = pos};
    }

    auto builtin = builtins_.find(fun_call.identifier);
    if (builtin != builtins_.cend()) {
        return callBuiltin(builtin->second, fun_call.args, pos);
    }

    auto const iter = program_->functions.find(fun_call.identifier);
    if (iter == program_->functions.cend()) {
        throw RuntimeError{.kind = UseOfUnkownIdentifier{}, .pos = pos};
    }
    auto const& fun = iter->second;

    if (fun->params.size() != fun_call.args.size()) {
        throw RuntimeError{
            .kind =
                ArgumentCountMismatch{
                    .expected = static_cast<int>(fun->params.size()),
                    .got = static_cast<int>(fun_call.args.size())},
            .pos = pos};
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
                throw RuntimeError{.kind = ImmutablePassedToMut{},
                                   .pos = arg.pos()};
            }
            if (*param->type != typeFor(var->data())) {
                throw RuntimeError{.kind = MutArgTypeMismatch{},
                                   .pos = arg.pos()};
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

    stack_.pushContext({.name = fun_call.identifier, .pos = pos});
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

    std::vector<Value> elements;
    elements.reserve(lit.elements.size());

    std::optional<Type> expected_type;

    for (auto const& elem : lit.elements) {
        auto value = visitExpr(elem);
        auto elem_type = typeFor(value);

        if (!expected_type && !isCoercibleEmptyVec(elem_type)) {
            expected_type = elem_type;
        } else if (expected_type && elem_type != *expected_type &&
                   !isCoercibleEmptyVec(elem_type)) {
            return std::unexpected(
                VecTypeMismatch{.first = *expected_type, .other = elem_type});
        }

        elements.push_back(std::move(value));
    }

    // [[], [], ...]
    if (!expected_type) {
        return VecValue{.type = Type{PrimitiveType::EmptyVec},
                        .elements = std::move(elements)};
    }
    // Now we one of these
    // [[1,2], []] -> change the empty one to a real vec with the correct type.
    // This [1, 2, []] -> throw [1, 2, 3] -> just return

    for (auto& elem : elements) {
        auto elem_type = typeFor(elem);
        if (elem_type == *expected_type) {
            continue;
        }

        auto coerced = coerceVec(std::move(elem), *expected_type);
        if (!coerced) {
            return std::unexpected(VecTypeMismatch{
                .first = *expected_type, .other = std::move(elem_type)});
        }
        elem = std::move(*coerced);
    }

    return VecValue{.type = std::move(*expected_type),
                    .elements = std::move(elements)};
}

Value Interpreter::visitFilterExpr(FilterExpr const& expr, Position pos) {
    auto lhs = visitExpr(*expr.left);

    auto const vec_pos = expr.left->pos();
    auto const filter_func_pos = expr.right->pos();

    auto* const lhs_vec = std::get_if<VecValue>(&lhs);
    if (lhs_vec == nullptr) {
        throw RuntimeError{.kind = InvalidOperand{.type = typeFor(lhs)},
                           .pos = vec_pos};
    }

    auto const* const func_ident = std::get_if<Identifier>(&**expr.right);
    if (func_ident == nullptr) {
        throw RuntimeError{.kind = ExpectedFunction{}, .pos = filter_func_pos};
    }

    auto const filter_func = program_->functions.find(func_ident->identifier);
    if (filter_func == program_->functions.cend() ||
        filter_func->second->return_type == std::nullopt) {
        throw RuntimeError{.kind = VoidFunctionInMapFilter{},
                           .pos = filter_func_pos};
    }

    std::vector<Value> filtered_vec{};

    stack_.current().pushScope();
    stack_.current().declareVariable("temp", Variable(Value{}, false, vec_pos));
    auto* temp = stack_.current().lookupVariable("temp");

    for (auto& elem : lhs_vec->elements) {
        temp->data() = elem;
        std::vector<ExprNode> args;
        args.emplace_back(filter_func_pos, Identifier{.identifier = "temp"});
        auto const fun_call = FunctionCall{.identifier = func_ident->identifier,
                                           .args = std::move(args)};
        auto res = visitFunctionCall(fun_call, filter_func_pos);
        if (toBool(res)) {
            filtered_vec.push_back(elem);
        }
    }
    stack_.current().popScope();
    return VecValue{.type = lhs_vec->type, .elements = std::move(filtered_vec)};
}

Value Interpreter::visitMapExpr(MapExpr const& expr, Position pos) {
    auto lhs = visitExpr(*expr.left);

    auto const vec_pos = expr.left->pos();
    auto const map_func_pos = expr.right->pos();

    auto* const lhs_vec = std::get_if<VecValue>(&lhs);
    if (lhs_vec == nullptr) {
        throw RuntimeError{.kind = InvalidOperand{.type = typeFor(lhs)},
                           .pos = vec_pos};
    }

    auto const* const func_ident = std::get_if<Identifier>(&**expr.right);
    if (func_ident == nullptr) {
        throw RuntimeError{.kind = ExpectedFunction{}, .pos = map_func_pos};
    }
    auto const map_func = program_->functions.find(func_ident->identifier);
    if (map_func == program_->functions.cend() ||
        map_func->second->return_type == std::nullopt) {
        throw RuntimeError{.kind = VoidFunctionInMapFilter{},
                           .pos = map_func_pos};
    }

    Type return_type = **map_func->second->return_type;

    std::vector<Value> mapped_vec{};
    mapped_vec.reserve(lhs_vec->elements.size());

    stack_.current().pushScope();
    stack_.current().declareVariable("temp", Variable(Value{}, false, vec_pos));
    auto* temp = stack_.current().lookupVariable("temp");

    for (auto& elem : lhs_vec->elements) {
        temp->data() = elem;
        std::vector<ExprNode> args;
        args.emplace_back(map_func_pos, Identifier{.identifier = "temp"});
        auto const fun_call = FunctionCall{.identifier = func_ident->identifier,
                                           .args = std::move(args)};
        auto res = visitFunctionCall(fun_call, map_func_pos);

        mapped_vec.push_back(std::move(res));
    }
    stack_.current().popScope();
    return VecValue{.type = std::move(return_type),
                    .elements = std::move(mapped_vec)};
}

EvalResult Interpreter::coerceIfNeeded(Value val, Type const& target) {
    if (typeFor(val) == target) {
        return val;
    }
    return coerce(val, target);
}

Value Interpreter::callBuiltin(BuiltinFunction const& builtin,
                               std::vector<ExprNode> const& arg_exprs,
                               Position pos) {
    if (arg_exprs.size() != builtin.params.size()) {
        throw RuntimeError{.kind = ArgumentCountMismatch{}, .pos = pos};
    }

    std::vector<Value> args;
    args.reserve(builtin.params.size());
    for (auto const& [param, arg] :
         std::views::zip(builtin.params, arg_exprs)) {
        if (param.mut) {
            auto const* ident = std::get_if<Identifier>(&*arg);
            if (ident == nullptr) {
                throw RuntimeError{.kind = MutArgExpression{},
                                   .pos = arg.pos()};
            }
            auto* var = stack_.current().lookupVariable(ident->identifier);
            if (!var->mut()) {
                throw RuntimeError{.kind = ImmutablePassedToMut{},
                                   .pos = arg.pos()};
            }
            if (param.type != typeFor(var->data())) {
                throw RuntimeError{.kind = MutArgTypeMismatch{},
                                   .pos = arg.pos()};
            }
            args.push_back(var->data());
        } else {
            auto coerced = coerceIfNeeded(visitExpr(arg), param.type);
            if (!coerced) {
                throw RuntimeError{.kind = coerced.error(), .pos = arg.pos()};
            }
            args.emplace_back(std::move(*coerced));
        }
    }
    auto res = builtin.fn(std::move(args));
    if (!res) {
        throw RuntimeError{.kind = res.error(), .pos = pos};
    }
    return *res;
}

std::optional<Value> Interpreter::coerceVec(Value value, Type const& target) {
    if (!target.isVec()) {
        return std::nullopt;
    }

    if (std::holds_alternative<UninitVec>(value)) {
        return VecValue{.type = target.element(), .elements = {}};
    }

    auto* vec = std::get_if<VecValue>(&value);
    if (vec == nullptr) {
        return std::nullopt;
    }

    if (vec->type == target.element()) {
        return value;
    }

    std::vector<Value> coerced;
    coerced.reserve(vec->elements.size());
    for (auto& elem : vec->elements) {
        auto result = coerceVec(std::move(elem), target.element());
        if (!result) {
            return std::nullopt;
        }
        coerced.push_back(std::move(*result));
    }
    return VecValue{.type = target.element(), .elements = std::move(coerced)};
}

bool Interpreter::isCoercibleEmptyVec(Type const& type) noexcept {
    if (type == PrimitiveType::EmptyVec) {
        return true;
    }
    if (type.isVec()) {
        return isCoercibleEmptyVec(type.element());
    }
    return false;
}
