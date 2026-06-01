#include <expected>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "dahlia_lib/Ast.h"
#include "dahlia_lib/Interpreter.h"
#include "dahlia_lib/Position.h"
#include "dahlia_lib/RuntimeError.h"
#include "dahlia_lib/Value.h"
#include "doctest.h"

struct TestOptions {
    std::optional<Type> return_type = PrimitiveType::Int;
    InterpreterOpts opts = InterpreterOpts{.max_call_depth = 10};
};

class InterpreterFixture {
public:
    void init(TestOptions const& opts, ProgramNode program) {
        program_ = std::move(program);
        interpreter_.emplace(Interpreter(opts.opts));
    }

    template <typename... FunctionNodes>
    void initMain(TestOptions const& opts,
                  std::vector<StatementNode> statements,
                  FunctionNodes&&... fns) {
        init(opts,
             ProgramNode(
                 pos0_,
                 makeProgram(
                     std::forward<FunctionNodes>(fns)...,
                     FunctionNode(
                         pos0_,
                         Function{.identifier = "main",
                                  .return_type = opts.return_type.transform(
                                      [&](auto const& type) {
                                          return TypeNode(pos0_, type);
                                      }),
                                  .block = BlockNode(
                                      pos0_, Block{std::move(statements)})}))));
    }

    void initExpr(TestOptions const& opts, ExprNode expr) {
        initMain(opts, makeStatements(
                           StatementNode(pos0_, ReturnStmt{std::move(expr)})));
    }

    template <typename... FunctionNodes>
    Program makeProgram(FunctionNodes&&... fns) {
        Program program;
        program.functions.reserve(sizeof...(fns));
        (program.functions.emplace(fns->identifier,
                                   std::forward<FunctionNodes>(fns)),
         ...);
        return program;
    }

    template <typename T, typename... Args>
    std::vector<T> makeVec(Args&&... args) {
        std::vector<T> vec;
        vec.reserve(sizeof...(args));
        (vec.emplace_back(std::forward<Args>(args)), ...);
        return vec;
    }

    template <typename... Args>
    std::vector<StatementNode> makeStatements(Args&&... args) {
        return makeVec<StatementNode>(std::forward<Args>(args)...);
    }

    template <typename... Args>
    std::vector<ExprNode> makeExprs(Args&&... args) {
        return makeVec<ExprNode>(std::forward<Args>(args)...);
    }

    template <typename... Args>
    std::vector<ParamNode> makeParams(Args&&... args) {
        return makeVec<ParamNode>(std::forward<Args>(args)...);
    }

    template <typename... Args>
    std::vector<std::pair<ExprNode, BlockNode>> makeElseIfs(Args&&... args) {
        return makeVec<std::pair<ExprNode, BlockNode>>(
            std::forward<Args>(args)...);
    }

    std::expected<Value, RuntimeError> run() {
        return interpreter_->run(*program_);
    }

    StatementNode return_a() {  // NOLINT
        return StatementNode(
            pos0_, ReturnStmt{.value = ExprNode(pos0_, Identifier{"a"})});
    }

    StatementNode let_a_eq(Value x) {  // NOLINT
        return StatementNode(pos0_,
                             LetBinding{.identifier = "a",
                                        .mut = false,
                                        .value = expr_from_val(std::move(x))});
    }
    StatementNode let_mut_a_eq(Value x) {  // NOLINT
        return StatementNode(pos0_,
                             LetBinding{.identifier = "a",
                                        .mut = true,
                                        .value = expr_from_val(std::move(x))});
    }

    RangeNode range_in_0_5(Position pos) {  // NOLINT
        return RangeNode(pos, Range{.start = ExprNode(pos0_, IntLiteral{0}),
                                    .inclusive = false,
                                    .end = ExprNode(pos0_, IntLiteral{5}),
                                    .step = std::nullopt});
    }

    StatementNode a_plus_eq_1() {  // NOLINT
        return StatementNode(
            pos0_,
            AssignStmt(
                LValue{.identifier = "a", .indices = {}},
                ExprNode(pos0_, AddExpr(ExprNode(pos0_, Identifier{"a"}),
                                        ExprNode(pos0_, IntLiteral{1})))));
    }

    StatementNode a_plus_eq_b() {  // NOLINT
        return StatementNode(
            pos0_,
            AssignStmt(
                LValue{.identifier = "a", .indices = {}},
                ExprNode(pos0_, AddExpr(ExprNode(pos0_, Identifier{"a"}),
                                        ExprNode(pos0_, Identifier{"b"})))));
    }

    FunctionNode makeRecursiveCounter(Position pos) {
        // fn count(mut n : int) -> int {
        //     if n == 1 { return 1; }
        //     return count(n - 1) + 1;
        // }
        return FunctionNode(
            pos0_,
            Function{
                .identifier = "count",
                .params = makeVec<ParamNode>(ParamNode(
                    pos0_,
                    Param{.type = TypeNode(pos0_, Type{PrimitiveType::Int}),
                          .identifier = "n",
                          .mut = false})),
                .return_type = TypeNode(pos0_, Type{PrimitiveType::Int}),
                .block = BlockNode(
                    pos0_,
                    Block{
                        .statements = makeStatements(
                            StatementNode(
                                pos0_,
                                IfStmt{
                                    .if_cond = ExprNode(
                                        pos0_,
                                        EqExpr(ExprNode(pos0_, Identifier{"n"}),
                                               ExprNode(pos0_, IntLiteral{1}))),
                                    .if_block = BlockNode(
                                        pos0_,
                                        Block{.statements = makeStatements(
                                                  return_val(1))}),
                                }),
                            StatementNode(
                                pos0_,
                                ReturnStmt{
                                    .value = ExprNode(
                                        pos0_,
                                        AddExpr(
                                            ExprNode(
                                                pos,
                                                FunctionCall{
                                                    .identifier =
                                                        "count",
                                                    .args = makeVec<ExprNode>(
                                                        ExprNode(
                                                            pos0_,
                                                            SubExpr(
                                                                ExprNode(
                                                                    pos0_,
                                                                    Identifier{
                                                                        "n"}),
                                                                ExprNode(
                                                                    pos0_,
                                                                    IntLiteral{
                                                                        1}))))}),
                                            ExprNode(pos0_,
                                                     IntLiteral{1})))}))})});
    }

    FunctionNode fun_void() {  // NOLINT
        return FunctionNode(pos0_,
                            Function{.identifier = "void",
                                     .return_type = std::nullopt,
                                     .block = BlockNode(pos0_, Block{})});
    }

    FunctionNode fun_return_1() {  // NOLINT
        return FunctionNode(
            pos0_,
            Function{.identifier = "return_1",
                     .return_type = TypeNode(pos0_, PrimitiveType::Str),
                     .block = BlockNode(
                         pos0_,
                         Block{makeStatements(StatementNode(
                             pos0_, ReturnStmt{.value = ExprNode(
                                                   pos0_, IntLiteral{1})}))})});
    }

    ExprNode expr_from_val(Value val) {  // NOLINT
        auto expr = std::visit(
            Overloaded{
                [](std::int64_t val) -> Expr { return IntLiteral{val}; },
                [](double val) -> Expr { return FloatLiteral{val}; },
                [](bool val) -> Expr { return BoolLiteral{val}; },
                [](std::string val) -> Expr {
                    return StringLiteral{std::move(val)};
                },
                [](VecValue const&) -> Expr {
                    throw std::runtime_error{
                        "can't make vec literal from value"};
                },
                [](UninitVec) -> Expr {
                    throw std::runtime_error{"can't make uninit vec literal"};
                },
                [](std::monostate) -> Expr {
                    throw std::runtime_error{"can't return monostate"};
                },
            },
            val);
        return {pos0_, std::move(expr)};
    }

    StatementNode return_val(Value val) {  // NOLINT
        return StatementNode(
            pos0_, ReturnStmt{.value = expr_from_val(std::move(val))});
    }

    StatementNode whileALt5(std::vector<StatementNode> extra_statements = {}) {
        auto statements = std::move(extra_statements);
        statements.emplace_back(
            pos0_,
            AssignStmt(
                LValue{.identifier = "a", .indices = {}},
                ExprNode(pos0_, AddExpr(ExprNode(pos0_, Identifier{"a"}),
                                        ExprNode(pos0_, IntLiteral{1})))));
        return StatementNode(
            pos0_,
            WhileLoop{.condition = ExprNode(
                          pos0_, LtExpr(ExprNode(pos0_, Identifier{"a"}),
                                        ExprNode(pos0_, IntLiteral{5}))),
                      .block = BlockNode(
                          pos0_, Block{.statements = std::move(statements)})});
    }

    // NOLINTBEGIN

    Position const pos1 = Position{.line = 0, .column = 0, .offset = 3};
    Position const pos2 = Position{.line = 0, .column = 0, .offset = 4};
    Position const pos3 = Position{.line = 0, .column = 0, .offset = 5};
    // NOLINTEND

private:
    Position pos0_ = Position{.line = 0, .column = 0, .offset = 2};

    std::optional<Interpreter> interpreter_;
    std::optional<ProgramNode> program_;
};

TEST_CASE_FIXTURE(InterpreterFixture, "Interpreter requires a main function") {
    init({},
         ProgramNode(pos2,
                     makeProgram(FunctionNode(
                         pos1, Function{.identifier = "test",
                                        .block = BlockNode(pos1, Block{})}))));

    auto const value = run();

    CHECK(value == std::unexpected(RuntimeError{.kind = MissingMainFunction{},
                                                .pos = pos2}));
}

TEST_CASE_FIXTURE(InterpreterFixture, "Interpreter runs empty main func") {
    init({},
         ProgramNode(pos1,
                     makeProgram(FunctionNode(
                         pos1, Function{.identifier = "main",
                                        .block = BlockNode(pos1, Block{})}))));

    auto const value = run();

    CHECK(value == Value{std::monostate{}});
}

TEST_CASE_FIXTURE(InterpreterFixture, "Interpreter runs empty main func") {
    initMain({}, makeStatements(StatementNode(
                     pos1, ReturnStmt{ExprNode(pos1, IntLiteral{1})})));

    auto const value = run();
    CHECK(value == Value{1});
}

TEST_CASE_FIXTURE(InterpreterFixture, "Interpreter evals int literals") {
    initExpr({}, ExprNode(pos1, IntLiteral{1}));

    auto const value = run();
    CHECK(value == Value{1});
}

TEST_CASE_FIXTURE(InterpreterFixture, "Interpreter evals float literals") {
    initExpr({.return_type = PrimitiveType::Float},
             ExprNode(pos1, FloatLiteral{1.5}));

    auto const value = run();
    CHECK(value == Value{1.5});
}

TEST_CASE_FIXTURE(InterpreterFixture, "Interpreter evals string literals") {
    initExpr({.return_type = PrimitiveType::Str},
             ExprNode(pos1, StringLiteral{"A"}));

    auto const value = run();
    CHECK(value == Value{"A"});
}

TEST_CASE_FIXTURE(InterpreterFixture, "Interpreter evals bool true literal") {
    initExpr({.return_type = PrimitiveType::Bool},
             ExprNode(pos1, BoolLiteral{true}));

    auto const value = run();
    CHECK(value == Value{true});
}

TEST_CASE_FIXTURE(InterpreterFixture, "Interpreter evals bool false literal") {
    initExpr({.return_type = PrimitiveType::Bool},
             ExprNode(pos1, BoolLiteral{false}));

    auto const value = run();
    CHECK(value == Value{false});
}

TEST_CASE_FIXTURE(InterpreterFixture, "Interpreter evals simple vec literals") {
    initExpr({.return_type = Type::vec(PrimitiveType::Int)},
             ExprNode(pos1, VecLiteral{.elements = makeVec<ExprNode>(
                                           ExprNode(pos1, IntLiteral{1}),
                                           ExprNode(pos1, IntLiteral{2}))}));

    auto const value = run();
    CHECK(value == Value{VecValue{.type = PrimitiveType::Int,
                                  .elements = {Value{1}, Value{2}}}});
}

TEST_CASE_FIXTURE(InterpreterFixture, "Interpreter evals simple vec literals") {
    initExpr({.return_type = Type::vec(PrimitiveType::Int)},
             ExprNode(pos1, VecLiteral{.elements = makeVec<ExprNode>(
                                           ExprNode(pos1, IntLiteral{1}),
                                           ExprNode(pos1, IntLiteral{2}))}));

    auto const value = run();
    CHECK(value == Value{VecValue{.type = Type{PrimitiveType::Int},
                                  .elements = {Value{1}, Value{2}}}});
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter doesn't allow different types in vec literals") {
    initExpr(
        {}, ExprNode(pos2, VecLiteral{.elements = makeVec<ExprNode>(
                                          ExprNode(pos1, IntLiteral{1}),
                                          ExprNode(pos1, FloatLiteral{2.0}))}));

    auto const value = run();
    CHECK(value ==
          std::unexpected(RuntimeError{
              .kind = VecTypeMismatch{.first = Type(PrimitiveType::Int),
                                      .other = Type(PrimitiveType::Float)},
              .pos = pos2}));
}

TEST_CASE_FIXTURE(InterpreterFixture, "Interpreter evals nested vec literal") {
    initExpr(
        {.return_type = Type::vec(Type::vec(PrimitiveType::Int))},
        ExprNode(
            pos1,
            VecLiteral{
                .elements = makeVec<ExprNode>(
                    ExprNode(pos1,
                             VecLiteral{.elements = makeVec<ExprNode>(
                                            ExprNode(pos1, IntLiteral{1}))}),
                    ExprNode(pos1, VecLiteral{
                                       .elements = makeVec<ExprNode>(
                                           ExprNode(pos1, IntLiteral{2}),
                                           ExprNode(pos1, IntLiteral{3}))}))}));

    auto const value = run();

    CHECK(
        value ==
        Value{VecValue{
            .type = Type::vec(PrimitiveType::Int),
            .elements = {Value{VecValue{.type = PrimitiveType::Int,
                                        .elements = {Value{1}}}},
                         Value{VecValue{.type = PrimitiveType::Int,
                                        .elements = {Value{2}, Value{3}}}}}}});
}

TEST_CASE_FIXTURE(InterpreterFixture, "Interpreter evals add expressions") {
    initExpr({}, ExprNode(pos1, AddExpr(ExprNode(pos1, IntLiteral{1}),
                                        ExprNode(pos1, IntLiteral{2}))));

    auto const value = run();

    CHECK(value == Value{3});
}

TEST_CASE_FIXTURE(InterpreterFixture, "Interpreter evals sub expressions") {
    initExpr({}, ExprNode(pos1, SubExpr(ExprNode(pos1, IntLiteral{1}),
                                        ExprNode(pos1, IntLiteral{2}))));

    auto const value = run();

    CHECK(value == Value{-1});
}

TEST_CASE_FIXTURE(InterpreterFixture, "Interpreter evals mul expressions") {
    initExpr({}, ExprNode(pos1, MulExpr(ExprNode(pos1, IntLiteral{1}),
                                        ExprNode(pos1, IntLiteral{2}))));

    auto const value = run();

    CHECK(value == Value{2});
}

TEST_CASE_FIXTURE(InterpreterFixture, "Interpreter evals div expressions") {
    initExpr({}, ExprNode(pos1, DivExpr(ExprNode(pos1, IntLiteral{4}),
                                        ExprNode(pos1, IntLiteral{2}))));

    auto const value = run();

    CHECK(value == Value{2});
}

TEST_CASE_FIXTURE(InterpreterFixture, "Interpreter evals mod expressions") {
    initExpr({}, ExprNode(pos1, ModExpr(ExprNode(pos1, IntLiteral{5}),
                                        ExprNode(pos1, IntLiteral{2}))));

    auto const value = run();

    CHECK(value == Value{1});
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter evals comparison expressions") {
    initExpr({.return_type = PrimitiveType::Bool},
             ExprNode(pos1, LeExpr(ExprNode(pos1, IntLiteral{1}),
                                   ExprNode(pos1, IntLiteral{2}))));

    auto const value = run();

    CHECK(value == Value{true});
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter returns from nested blocks") {
    initMain({}, makeStatements(StatementNode(
                     pos1,
                     BlockNode(pos1, Block{makeStatements(StatementNode(
                                         pos1, ReturnStmt{ExprNode(
                                                   pos1, IntLiteral{1})}))}))));

    auto const value = run();
    CHECK(value == Value{1});
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter throws on break statements outside of loops") {
    initMain({}, makeStatements(StatementNode(pos1, BreakStmt{})));

    auto const value = run();
    CHECK(value == std::unexpected(
                       RuntimeError{.kind = UnexpectedBreak{}, .pos = pos1}));
}

TEST_CASE_FIXTURE(
    InterpreterFixture,
    "Interpreter throws on continue statements outside of loops") {
    initMain({}, makeStatements(StatementNode(pos1, ContinueStmt{})));

    auto const value = run();
    CHECK(value == std::unexpected(RuntimeError{.kind = UnexpectedContinue{},
                                                .pos = pos1}));
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter assigns value to statements") {
    initMain({}, makeStatements(
                     StatementNode(pos1, LetBinding{.identifier = "a",
                                                    .value = ExprNode(
                                                        pos1, IntLiteral{1})}),
                     return_a()));

    auto const value = run();
    CHECK(value == Value{1});
}

TEST_CASE_FIXTURE(
    InterpreterFixture,
    "Interpreter doesn't allow variable redefinition in the same scope") {
    initMain(
        {}, makeStatements(
                StatementNode(
                    pos2, LetBinding{.identifier = "a",
                                     .value = ExprNode(pos1, IntLiteral{1})}),
                StatementNode(
                    pos3, LetBinding{.identifier = "a",
                                     .value = ExprNode(pos1, IntLiteral{1})})));

    auto const value = run();
    CHECK(value == std::unexpected(RuntimeError{
                       .kind = VariableRedefinition{.original_pos = pos2},
                       .pos = pos3}));
}

TEST_CASE_FIXTURE(InterpreterFixture, "Interpreter let binding with type") {
    initMain(
        {}, makeStatements(
                StatementNode(
                    pos2, LetBinding{.identifier = "a",
                                     .type = TypeNode(pos1, PrimitiveType::Int),
                                     .value = ExprNode(pos1, IntLiteral{1})}),
                return_a()));

    auto const value = run();
    CHECK(value == 1);
}

TEST_CASE_FIXTURE(
    InterpreterFixture,
    "Interpreter let binding with type, requires types to match") {
    initMain({},
             makeStatements(StatementNode(
                 pos2, LetBinding{.identifier = "a",
                                  .type = TypeNode(pos1, PrimitiveType::Str),
                                  .value = ExprNode(pos1, IntLiteral{1})})));

    auto const value = run();
    CHECK(value == std::unexpected(RuntimeError{
                       .kind = AssignmentTypeMismatch{}, .pos = pos2}));
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter throws on usage of an undeclared variable") {
    initMain({}, makeStatements(StatementNode(
                     pos1, ReturnStmt{ExprNode(pos2, Identifier{"a"})})));

    auto const value = run();
    CHECK(value == std::unexpected(RuntimeError{.kind = UseOfUnkownIdentifier{},
                                                .pos = pos2}));
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter allows shadowing variables in inner scopes") {
    initMain(
        {}, makeStatements(
                let_a_eq(1),
                StatementNode(
                    pos1, BlockNode(pos1, Block{makeStatements(let_a_eq(2),
                                                               return_a())}))));

    auto const value = run();
    CHECK(value == Value{2});
}

TEST_CASE_FIXTURE(
    InterpreterFixture,
    "Interpreter restores shadowed variable value once block ends") {
    initMain({},
             makeStatements(
                 let_a_eq(1),
                 StatementNode(
                     pos1, BlockNode(pos1, Block{makeStatements(let_a_eq(0))})),
                 return_a()));

    auto const value = run();
    CHECK(value == Value{1});
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter sees variables in scopes above itself") {
    initMain(
        {}, makeStatements(
                let_a_eq(1),
                StatementNode(
                    pos1, BlockNode(pos1, Block{makeStatements(return_a())}))));

    auto const value = run();
    CHECK(value == Value{1});
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter assigns values to variables") {
    initMain({},
             makeStatements(
                 let_mut_a_eq(1),
                 StatementNode(pos1, AssignStmt(LValue{.identifier = "a"},
                                                ExprNode(pos1, IntLiteral{2}))),
                 return_a()));

    auto const value = run();
    CHECK(value == Value{2});
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter doesn't allow assignment to non mut variables") {
    initMain({},
             makeStatements(
                 let_a_eq(1),
                 StatementNode(pos2, AssignStmt(LValue{.identifier = "a"},
                                                ExprNode(pos1, IntLiteral{2}))),
                 return_a()));

    auto const value = run();
    CHECK(value ==
          std::unexpected(RuntimeError{.kind = MutViolation{}, .pos = pos2}));
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter converts to variable type on assignment") {
    initMain({}, makeStatements(
                     let_mut_a_eq(1),
                     StatementNode(
                         pos1, AssignStmt(LValue{.identifier = "a"},
                                          ExprNode(pos1, StringLiteral{"2"}))),
                     return_a()));

    auto const value = run();
    CHECK(value == Value{2});
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter throws error when assignment converion fails") {
    initMain({}, makeStatements(
                     let_mut_a_eq(1),
                     StatementNode(
                         pos1, AssignStmt(LValue{.identifier = "a"},
                                          ExprNode(pos2, StringLiteral{"a"}))),
                     return_a()));

    auto const value = run();
    CHECK(value ==
          std::unexpected(RuntimeError{
              .kind = UnparsableString{.val = "a",
                                       .targetType = PrimitiveType::Int},
              .pos = pos2}));
}

TEST_CASE_FIXTURE(InterpreterFixture, "Interpreter runs while loops") {
    initMain({},
             makeStatements(
                 let_mut_a_eq(0),
                 StatementNode(
                     pos1,
                     WhileLoop{.condition = ExprNode(
                                   pos1, LtExpr(ExprNode(pos1, Identifier{"a"}),
                                                ExprNode(pos1, IntLiteral{5}))),
                               .block = BlockNode(
                                   pos1, Block{.statements = makeStatements(
                                                   a_plus_eq_1())})}),
                 return_a()));

    auto const value = run();
    CHECK(value == Value{5});
}

TEST_CASE_FIXTURE(InterpreterFixture, "Interpreter break ends while lopp") {
    initMain(
        {}, makeStatements(
                let_mut_a_eq(0),
                whileALt5(makeStatements(StatementNode(
                    pos1,
                    IfStmt{
                        .if_cond = ExprNode(
                            pos1, EqExpr(ExprNode(pos1, Identifier{"a"}),
                                         ExprNode(pos1, IntLiteral{3}))),
                        .if_block = BlockNode(
                            pos1, Block{.statements = makeStatements(
                                            StatementNode(pos1, BreakStmt{}))}),
                    }))),
                return_a()));
    CHECK(run() == 3);
}

TEST_CASE_FIXTURE(InterpreterFixture, "Interpreter continue works in while") {
    initMain(
        {}, makeStatements(
                let_mut_a_eq(0),
                StatementNode(
                    pos1,
                    WhileLoop{
                        .condition = ExprNode(
                            pos1, LtExpr(ExprNode(pos1, Identifier{"a"}),
                                         ExprNode(pos1, IntLiteral{3}))),
                        .block = BlockNode(
                            pos1, Block{.statements = makeStatements(
                                            a_plus_eq_1(),
                                            StatementNode(pos1, ContinueStmt{}),
                                            return_val(99))})}),
                return_a()));
    CHECK(run() == 3);
}

TEST_CASE_FIXTURE(InterpreterFixture, "Interpreter runs for loop base case") {
    initMain({.return_type = PrimitiveType::Str},
             makeStatements(
                 let_mut_a_eq(""),
                 StatementNode(
                     pos1, ForLoop{.loop_var = "b",
                                   .mut = false,
                                   .range = range_in_0_5(pos1),
                                   .block = BlockNode(
                                       pos1, Block{.statements = makeStatements(
                                                       a_plus_eq_b())})}),
                 return_a()));
    CHECK(run() == "01234");
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter runs for loop with custom step") {
    initMain(
        {.return_type = PrimitiveType::Str},
        makeStatements(
            let_mut_a_eq(""),
            StatementNode(
                pos1,
                ForLoop{.loop_var = "b",
                        .mut = false,
                        .range = RangeNode(
                            pos1, Range{.start = ExprNode(pos1, IntLiteral{0}),
                                        .inclusive = true,
                                        .end = ExprNode(pos1, IntLiteral{5}),
                                        .step = ExprNode(pos1, IntLiteral{2})}),
                        .block = BlockNode(
                            pos1, Block{.statements =
                                            makeStatements(a_plus_eq_b())})}),
            return_a()));
    CHECK(run() == "024");
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter runs for loop reverse default step") {
    initMain(
        {.return_type = PrimitiveType::Str},
        makeStatements(
            let_mut_a_eq(""),
            StatementNode(
                pos1,
                ForLoop{.loop_var = "b",
                        .mut = false,
                        .range = RangeNode(
                            pos1, Range{.start = ExprNode(pos1, IntLiteral{5}),
                                        .inclusive = false,
                                        .end = ExprNode(pos1, IntLiteral{0}),
                                        .step = std::nullopt}),
                        .block = BlockNode(
                            pos1, Block{.statements =
                                            makeStatements(a_plus_eq_b())})}),
            return_a()));
    CHECK(run() == "54321");
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter runs for loop reverse custom step") {
    initMain(
        {.return_type = PrimitiveType::Str},
        makeStatements(
            let_mut_a_eq(""),
            StatementNode(
                pos1,
                ForLoop{
                    .loop_var = "b",
                    .mut = false,
                    .range = RangeNode(
                        pos1, Range{.start = ExprNode(pos1, IntLiteral{5}),
                                    .inclusive = false,
                                    .end = ExprNode(pos1, IntLiteral{0}),
                                    .step = ExprNode(pos1, IntLiteral{-2})}),
                    .block = BlockNode(pos1, Block{.statements = makeStatements(
                                                       a_plus_eq_b())})}),
            return_a()));
    CHECK(run() == "531");
}

TEST_CASE_FIXTURE(InterpreterFixture, "Interpreter for loop range inclusive") {
    initMain(
        {.return_type = PrimitiveType::Str},
        makeStatements(
            let_mut_a_eq(""),
            StatementNode(
                pos1,
                ForLoop{.loop_var = "b",
                        .mut = false,
                        .range = RangeNode(
                            pos1, Range{.start = ExprNode(pos1, IntLiteral{0}),
                                        .inclusive = true,
                                        .end = ExprNode(pos1, IntLiteral{5}),
                                        .step = std::nullopt}),
                        .block = BlockNode(
                            pos1, Block{.statements =
                                            makeStatements(a_plus_eq_b())})}),
            return_a()));
    CHECK(run() == "012345");
}

TEST_CASE_FIXTURE(InterpreterFixture, "Interpreter runs for loop continue") {
    initMain(
        {.return_type = PrimitiveType::Str},
        makeStatements(
            let_mut_a_eq(""),
            StatementNode(
                pos1,
                ForLoop{.loop_var = "b",
                        .mut = false,
                        .range = range_in_0_5(pos1),
                        .block = BlockNode(
                            pos1, Block{.statements = makeStatements(
                                            a_plus_eq_b(),
                                            StatementNode(pos1, ContinueStmt{}),
                                            return_val(99))})}),
            return_a()));
    auto const value = run();
    CHECK(value == "01234");
}

TEST_CASE_FIXTURE(InterpreterFixture, "Interpreter for loop break") {
    initMain(
        {.return_type = PrimitiveType::Str},
        makeStatements(
            let_mut_a_eq(""),
            StatementNode(
                pos1,
                ForLoop{
                    .loop_var = "b",
                    .mut = false,
                    .range = range_in_0_5(pos1),
                    .block = BlockNode(
                        pos1,
                        Block{
                            .statements = makeStatements(
                                StatementNode(
                                    pos1,
                                    IfStmt{
                                        .if_cond = ExprNode(
                                            pos1,
                                            EqExpr(
                                                ExprNode(pos1, Identifier{"b"}),
                                                ExprNode(pos1, IntLiteral{2}))),
                                        .if_block = BlockNode(
                                            pos1,
                                            Block{.statements = makeStatements(
                                                      StatementNode(
                                                          pos1, BreakStmt{}))}),
                                    }),
                                a_plus_eq_b())})}),
            return_a()));
    auto const value = run();
    CHECK(value == "01");
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter for loop, return exists loop") {
    initMain(
        {.return_type = PrimitiveType::Str},
        makeStatements(
            let_mut_a_eq(""),
            StatementNode(
                pos1,
                ForLoop{.loop_var = "b",
                        .mut = false,
                        .range = range_in_0_5(pos1),
                        .block = BlockNode(
                            pos1, Block{.statements = makeStatements(
                                            a_plus_eq_b(), return_a())})})));
    auto const value = run();
    CHECK(value == "0");
}

TEST_CASE_FIXTURE(
    InterpreterFixture,
    "Interpreter for loop, loop variable requires mut to be modified") {
    initMain(
        {},
        makeStatements(
            let_mut_a_eq(""),
            StatementNode(
                pos1,
                ForLoop{
                    .loop_var = "b",
                    .mut = false,
                    .range = range_in_0_5(pos1),
                    .block = BlockNode(
                        pos1,
                        Block{makeStatements(StatementNode(
                            pos3,
                            AssignStmt(LValue{.identifier = "b"},
                                       ExprNode(pos1, IntLiteral{99}))))})})));
    auto const value = run();
    CHECK(value ==
          std::unexpected(RuntimeError{.kind = MutViolation{}, .pos = pos3}));
}

TEST_CASE_FIXTURE(
    InterpreterFixture,
    "Interpreter for loop, loop variable not modified next iteration") {
    initMain(
        {.return_type = PrimitiveType::Str},
        makeStatements(
            let_mut_a_eq(""),
            StatementNode(
                pos1,
                ForLoop{
                    .loop_var = "b",
                    .mut = true,
                    .range = range_in_0_5(pos1),
                    .block = BlockNode(
                        pos1,
                        Block{.statements = makeStatements(
                                  a_plus_eq_b(),
                                  StatementNode(
                                      pos1,
                                      AssignStmt(
                                          LValue{.identifier = "b"},
                                          ExprNode(pos1, IntLiteral{99}))))})}),
            return_a()));
    auto const value = run();
    CHECK(value == "01234");
}

TEST_CASE_FIXTURE(
    InterpreterFixture,
    "Interpreter for loop invalid range - start > stop, step positive") {
    initMain(
        {},
        makeStatements(
            let_mut_a_eq(0),
            StatementNode(
                pos1,
                ForLoop{.loop_var = "b",
                        .mut = false,
                        .range = RangeNode(
                            pos3, Range{.start = ExprNode(pos1, IntLiteral{4}),
                                        .inclusive = false,
                                        .end = ExprNode(pos1, IntLiteral{0}),
                                        .step = ExprNode(pos1, IntLiteral{1})}),
                        .block = BlockNode(pos1, Block{})})));
    CHECK(run() == std::unexpected(
                       RuntimeError{.kind = InvalidForRange{}, .pos = pos3}));
}

TEST_CASE_FIXTURE(
    InterpreterFixture,
    "Interpreter for loop invalid range - start < stop, step negative") {
    initMain({},
             makeStatements(
                 let_mut_a_eq(0),
                 StatementNode(
                     pos1,
                     ForLoop{.loop_var = "b",
                             .mut = false,
                             .range = RangeNode(
                                 pos3,
                                 Range{.start = ExprNode(pos1, IntLiteral{0}),
                                       .inclusive = false,
                                       .end = ExprNode(pos1, IntLiteral{4}),
                                       .step = ExprNode(pos1, IntLiteral{-1})}),
                             .block = BlockNode(pos1, Block{})})));
    CHECK(run() == std::unexpected(
                       RuntimeError{.kind = InvalidForRange{}, .pos = pos3}));
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter for loop invalid range - step 0") {
    initMain(
        {},
        makeStatements(
            let_mut_a_eq(0),
            StatementNode(
                pos1,
                ForLoop{.loop_var = "b",
                        .mut = false,
                        .range = RangeNode(
                            pos3, Range{.start = ExprNode(pos1, IntLiteral{0}),
                                        .inclusive = false,
                                        .end = ExprNode(pos1, IntLiteral{4}),
                                        .step = ExprNode(pos1, IntLiteral{0})}),
                        .block = BlockNode(pos1, Block{})})));
    CHECK(run() == std::unexpected(
                       RuntimeError{.kind = InvalidForRange{}, .pos = pos3}));
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter for loop invalid range - start and end equal") {
    initMain(
        {},
        makeStatements(
            let_mut_a_eq(0),
            StatementNode(
                pos1,
                ForLoop{.loop_var = "b",
                        .mut = false,
                        .range = RangeNode(
                            pos3, Range{.start = ExprNode(pos1, IntLiteral{4}),
                                        .inclusive = false,
                                        .end = ExprNode(pos1, IntLiteral{4}),
                                        .step = ExprNode(pos1, IntLiteral{1})}),
                        .block = BlockNode(pos1, Block{})})));
    CHECK(run() == std::unexpected(
                       RuntimeError{.kind = InvalidForRange{}, .pos = pos3}));
}

TEST_CASE_FIXTURE(InterpreterFixture, "Interpreter if - true executes block") {
    initMain({},
             makeStatements(
                 StatementNode(pos1,
                               IfStmt{
                                   .if_cond = ExprNode(pos1, BoolLiteral{true}),
                                   .if_block = BlockNode(
                                       pos1, Block{.statements = makeStatements(
                                                       return_val(1))}),
                               }),
                 return_val(2)));
    CHECK(run() == 1);
}

TEST_CASE_FIXTURE(InterpreterFixture, "Interpreter if - false skips block") {
    initMain(
        {}, makeStatements(
                StatementNode(pos1,
                              IfStmt{
                                  .if_cond = ExprNode(pos1, BoolLiteral{false}),
                                  .if_block = BlockNode(
                                      pos1, Block{.statements = makeStatements(
                                                      return_val(1))}),
                              }),
                return_val(2)));
    CHECK(run() == 2);
}

TEST_CASE_FIXTURE(InterpreterFixture, "Interpreter else if - first true") {
    initMain(
        {},
        makeStatements(StatementNode(
            pos1,
            IfStmt{
                .if_cond = ExprNode(pos1, BoolLiteral{true}),
                .if_block = BlockNode(
                    pos1, Block{.statements = makeStatements(return_val(1))}),
                .else_if_branches = makeElseIfs(std::make_pair(
                    ExprNode(pos1, BoolLiteral{true}),
                    BlockNode(pos1, Block{.statements =
                                              makeStatements(return_val(2))}))),
            })));
    CHECK(run() == 1);
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter else if - first false second true") {
    initMain(
        {},
        makeStatements(StatementNode(
            pos1,
            IfStmt{
                .if_cond = ExprNode(pos1, BoolLiteral{false}),
                .if_block = BlockNode(
                    pos1, Block{.statements = makeStatements(return_val(1))}),
                .else_if_branches = makeElseIfs(std::make_pair(
                    ExprNode(pos1, BoolLiteral{true}),
                    BlockNode(pos1, Block{.statements =
                                              makeStatements(return_val(2))}))),
            })));
    CHECK(run() == 2);
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter else if - all false executes else") {
    initMain(
        {},
        makeStatements(StatementNode(
            pos1,
            IfStmt{
                .if_cond = ExprNode(pos1, BoolLiteral{false}),
                .if_block = BlockNode(
                    pos1, Block{.statements = makeStatements(return_val(1))}),
                .else_if_branches = makeElseIfs(std::make_pair(
                    ExprNode(pos1, BoolLiteral{false}),
                    BlockNode(pos1, Block{.statements =
                                              makeStatements(return_val(2))}))),
                .else_block = BlockNode(
                    pos1, Block{.statements = makeStatements(return_val(3))}),
            })));
    CHECK(run() == 3);
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter else if - all false no else") {
    initMain(
        {}, makeStatements(
                StatementNode(
                    pos1,
                    IfStmt{
                        .if_cond = ExprNode(pos1, BoolLiteral{false}),
                        .if_block = BlockNode(
                            pos1,
                            Block{.statements = makeStatements(return_val(1))}),
                        .else_if_branches = makeElseIfs(std::make_pair(
                            ExprNode(pos1, BoolLiteral{false}),
                            BlockNode(pos1, Block{.statements = makeStatements(
                                                      return_val(2))}))),
                    }),
                return_val(4)));
    CHECK(run() == 4);
}

TEST_CASE_FIXTURE(InterpreterFixture, "Interpreter else if - first true") {
    initMain(
        {},
        makeStatements(StatementNode(
            pos1,
            IfStmt{
                .if_cond = ExprNode(pos1, BoolLiteral{true}),
                .if_block = BlockNode(
                    pos1, Block{.statements = makeStatements(return_val(1))}),
                .else_if_branches = makeElseIfs(std::make_pair(
                    ExprNode(pos1, BoolLiteral{true}),
                    BlockNode(pos1, Block{.statements =
                                              makeStatements(return_val(2))}))),
            })));
    CHECK(run() == 1);
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter else if - first false second true") {
    initMain(
        {},
        makeStatements(StatementNode(
            pos1,
            IfStmt{
                .if_cond = ExprNode(pos1, BoolLiteral{false}),
                .if_block = BlockNode(
                    pos1, Block{.statements = makeStatements(return_val(1))}),
                .else_if_branches = makeElseIfs(std::make_pair(
                    ExprNode(pos1, BoolLiteral{true}),
                    BlockNode(pos1, Block{.statements =
                                              makeStatements(return_val(2))}))),
            })));
    CHECK(run() == 2);
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter else if - all false executes else") {
    initMain(
        {},
        makeStatements(StatementNode(
            pos1,
            IfStmt{
                .if_cond = ExprNode(pos1, BoolLiteral{false}),
                .if_block = BlockNode(
                    pos1, Block{.statements = makeStatements(return_val(1))}),
                .else_if_branches = makeElseIfs(std::make_pair(
                    ExprNode(pos1, BoolLiteral{false}),
                    BlockNode(pos1, Block{.statements =
                                              makeStatements(return_val(2))}))),
                .else_block = BlockNode(
                    pos1, Block{.statements = makeStatements(return_val(3))}),
            })));
    CHECK(run() == 3);
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter else if - all false no else") {
    initMain(
        {}, makeStatements(
                StatementNode(
                    pos1,
                    IfStmt{
                        .if_cond = ExprNode(pos1, BoolLiteral{false}),
                        .if_block = BlockNode(
                            pos1,
                            Block{.statements = makeStatements(return_val(1))}),
                        .else_if_branches = makeElseIfs(std::make_pair(
                            ExprNode(pos1, BoolLiteral{false}),
                            BlockNode(pos1, Block{.statements = makeStatements(
                                                      return_val(2))}))),
                    }),
                return_val(4)));
    CHECK(run() == 4);
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter calls empty void functions") {
    initMain(
        {.return_type = std::nullopt},
        makeStatements(StatementNode(pos1, FunctionCall{.identifier = "void"})),
        fun_void());
    auto const value = run();
    CHECK(value == Value{});
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter void type in expression not allowed") {
    initMain(
        {.return_type = std::nullopt},
        makeStatements(StatementNode(
            pos1, LetBinding{.identifier = "a",
                             .value = ExprNode(
                                 pos2, FunctionCall{.identifier = "void"})})),
        fun_void());
    auto const value = run();
    CHECK(value == std::unexpected(RuntimeError{.kind = VoidTypeInExpression{},
                                                .pos = pos2}));
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter calls functions which return a value") {
    initMain({.return_type = PrimitiveType::Int},
             makeStatements(StatementNode(
                 pos1, ReturnStmt{ExprNode(
                           pos1, FunctionCall{.identifier = "return_1"})})),
             fun_return_1());
    auto const value = run();
    CHECK(value == Value{1});
}

TEST_CASE_FIXTURE(
    InterpreterFixture,
    "Interpreter throws when number of arguments and params don't match") {
    initMain({.return_type = PrimitiveType::Int},
             makeStatements(StatementNode(
                 pos1, ReturnStmt{ExprNode(
                           pos3, FunctionCall{.identifier = "return_1",
                                              .args = makeExprs(ExprNode(
                                                  pos1, IntLiteral{1}))})})),
             fun_return_1());
    auto const value = run();
    CHECK(value == std::unexpected(RuntimeError{.kind = ArgumentCountMismatch{},
                                                .pos = pos3}));
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter passes mut variable as const parameter") {
    initMain(
        {},
        makeStatements(
            let_mut_a_eq(0),
            StatementNode(
                pos1, FunctionCall{.identifier = "ref_test",
                                   .args = makeExprs(ExprNode(
                                       pos1, Identifier{.identifier = "a"}))}),
            return_a()),

        FunctionNode(
            pos1,
            Function{
                .identifier = "ref_test",
                .params = makeParams(ParamNode(
                    pos1, Param{.type = TypeNode(pos1, PrimitiveType::Int),
                                .identifier = "a",
                                .mut = false})),
                .return_type = TypeNode(pos1, PrimitiveType::Int),
                .block = BlockNode(pos1, Block{makeStatements(return_a())})}));

    auto const value = run();
    CHECK(value == 0);
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter passes const variable as const parameter") {
    initMain(
        {},
        makeStatements(
            let_a_eq(0),
            StatementNode(
                pos1, FunctionCall{.identifier = "ref_test",
                                   .args = makeExprs(ExprNode(
                                       pos1, Identifier{.identifier = "a"}))}),
            return_a()),

        FunctionNode(
            pos1,
            Function{
                .identifier = "ref_test",
                .params = makeParams(ParamNode(
                    pos1, Param{.type = TypeNode(pos1, PrimitiveType::Int),
                                .identifier = "a",
                                .mut = false})),
                .return_type = TypeNode(pos1, PrimitiveType::Int),
                .block = BlockNode(pos1, Block{makeStatements(return_a())})}));

    auto const value = run();
    CHECK(value == 0);
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter passes mut variable as mut paramater") {
    initMain(
        {},
        makeStatements(
            let_mut_a_eq(0),
            StatementNode(
                pos1, FunctionCall{.identifier = "ref_test",
                                   .args = makeExprs(ExprNode(
                                       pos1, Identifier{.identifier = "a"}))}),
            return_a()),

        FunctionNode(
            pos1,
            Function{.identifier = "ref_test",
                     .params = makeParams(ParamNode(
                         pos1, Param{.type = TypeNode(pos1, PrimitiveType::Int),
                                     .identifier = "a",
                                     .mut = true})),
                     .block = BlockNode(
                         pos1, Block{makeStatements(a_plus_eq_1())})}));

    auto const value = run();
    CHECK(value == 1);
}

TEST_CASE_FIXTURE(
    InterpreterFixture,
    "Interpreter throws on passing a const variable to a mut param") {
    initMain(
        {},
        makeStatements(
            let_a_eq(0),
            StatementNode(
                pos1, FunctionCall{.identifier = "ref_test",
                                   .args = makeExprs(ExprNode(
                                       pos3, Identifier{.identifier = "a"}))})),

        FunctionNode(
            pos1,
            Function{.identifier = "ref_test",
                     .params = makeParams(ParamNode(
                         pos1, Param{.type = TypeNode(pos1, PrimitiveType::Int),
                                     .identifier = "a",
                                     .mut = true})),
                     .block = BlockNode(pos1, Block{})}));

    auto const value = run();
    CHECK(value ==
          std::unexpected(RuntimeError{.kind = MutViolation{}, .pos = pos3}));
}

TEST_CASE_FIXTURE(
    InterpreterFixture,
    "Interpreter throws when passing a expression into a mut parameter") {
    initMain(
        {},
        makeStatements(StatementNode(
            pos1,
            FunctionCall{.identifier = "ref_test",
                         .args = makeExprs(ExprNode(pos3, IntLiteral{1}))})),

        FunctionNode(
            pos1,
            Function{.identifier = "ref_test",
                     .params = makeParams(ParamNode(
                         pos1, Param{.type = TypeNode(pos1, PrimitiveType::Int),
                                     .identifier = "a",
                                     .mut = true})),
                     .block = BlockNode(pos1, Block{})}));

    auto const value = run();
    CHECK(value == std::unexpected(
                       RuntimeError{.kind = MutArgExpression{}, .pos = pos3}));
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter converts return value if possible") {
    initMain(
        {},
        makeStatements(StatementNode(
            pos1, ReturnStmt{ExprNode(
                      pos1, FunctionCall{.identifier = "ret_test"})})),

        FunctionNode(
            pos1, Function{.identifier = "ret_test",
                           .return_type = TypeNode(pos1, PrimitiveType::Int),
                           .block = BlockNode(
                               pos1, Block{makeStatements(return_val("3"))})}));

    auto const value = run();
    CHECK(value == 3);
}

TEST_CASE_FIXTURE(
    InterpreterFixture,
    "Interpreter throws when converting return val is impossible") {
    initMain(
        {},
        makeStatements(StatementNode(
            pos1, ReturnStmt{ExprNode(
                      pos1, FunctionCall{.identifier = "ret_test"})})),

        FunctionNode(
            pos3, Function{.identifier = "ret_test",
                           .return_type = TypeNode(pos1, PrimitiveType::Int),
                           .block = BlockNode(
                               pos1, Block{makeStatements(return_val("a"))})}));

    auto const value = run();
    // Once return signal gets it's position, change the pos here
    CHECK(value ==
          std::unexpected(RuntimeError{
              .kind = UnparsableString{.val = "a",
                                       .targetType = PrimitiveType::Int},
              .pos = pos3}));
}

TEST_CASE_FIXTURE(
    InterpreterFixture,
    "Interpreter throws when a value is not returned but it's expected") {
    initMain(
        {},
        makeStatements(
            StatementNode(pos1, FunctionCall{.identifier = "ret_test"})),

        FunctionNode(
            pos3, Function{.identifier = "ret_test",
                           .return_type = TypeNode(pos1, PrimitiveType::Int),
                           .block = BlockNode(pos1, Block{makeStatements()})}));

    auto const value = run();
    CHECK(value == std::unexpected(RuntimeError{.kind = MissingReturnValue{},
                                                .pos = pos3}));
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter recursion just below depth limit") {
    initMain({.opts = {.max_call_depth = 4}},
             makeStatements(StatementNode(
                 pos1, ReturnStmt{ExprNode(
                           pos1, FunctionCall{.identifier = "count",
                                              .args = makeExprs(ExprNode(
                                                  pos1, IntLiteral{3}))})})),
             makeRecursiveCounter(pos1));
    auto const value = run();
    // Called 3 times, because main also counts
    CHECK(value == 3);
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter errors when call depth is exceeded") {
    initMain({.opts = {.max_call_depth = 4}},
             makeStatements(StatementNode(
                 pos1, ReturnStmt{ExprNode(
                           pos1, FunctionCall{.identifier = "count",
                                              .args = makeExprs(ExprNode(
                                                  pos1, IntLiteral{4}))})})),
             makeRecursiveCounter(pos3));
    auto const value = run();
    CHECK(value == std::unexpected(
                       RuntimeError{.kind = CallDepthExceeded{}, .pos = pos3}));
}
