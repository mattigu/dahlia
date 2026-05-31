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

class InterpreterFixture {
public:
    void init(ProgramNode program) {
        program_ = std::move(program);
        interpreter_.emplace(Interpreter());
    }

    void initMain(std::vector<StatementNode> statements) {
        init(ProgramNode(
            pos0_,
            makeProgram(FunctionNode(
                pos0_, Function{.identifier = "main",
                                .block = BlockNode(
                                    pos2, Block{std::move(statements)})}))));
    }

    void initExpr(ExprNode expr) {
        initMain(
            makeStatements(StatementNode(pos0_, ReturnStmt{std::move(expr)})));
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
    Position pos0_ = Position{.line = 0, .column = 0, .offset = 3};

    std::optional<Interpreter> interpreter_;
    std::optional<ProgramNode> program_;
};

TEST_CASE_FIXTURE(InterpreterFixture, "Interpreter requires a main function") {
    init(ProgramNode(pos2,
                     makeProgram(FunctionNode(
                         pos1, Function{.identifier = "test",
                                        .block = BlockNode(pos1, Block{})}))));

    auto const value = run();

    CHECK(value == std::unexpected(RuntimeError{.kind = MissingMainFunction{},
                                                .pos = pos2}));
}

TEST_CASE_FIXTURE(InterpreterFixture, "Interpreter runs empty main func") {
    init(ProgramNode(pos1,
                     makeProgram(FunctionNode(
                         pos1, Function{.identifier = "main",
                                        .block = BlockNode(pos1, Block{})}))));

    auto const value = run();

    CHECK(value == Value{std::monostate{}});
}

TEST_CASE_FIXTURE(InterpreterFixture, "Interpreter runs empty main func") {
    initMain(makeStatements(
        StatementNode(pos1, ReturnStmt{ExprNode(pos1, IntLiteral{1})})));

    auto const value = run();
    CHECK(value == Value{1});
}

TEST_CASE_FIXTURE(InterpreterFixture, "Interpreter evals int literals") {
    initExpr(ExprNode(pos1, IntLiteral{1}));

    auto const value = run();
    CHECK(value == Value{1});
}

TEST_CASE_FIXTURE(InterpreterFixture, "Interpreter evals float literals") {
    initExpr(ExprNode(pos1, FloatLiteral{1.5}));

    auto const value = run();
    CHECK(value == Value{1.5});
}

TEST_CASE_FIXTURE(InterpreterFixture, "Interpreter evals string literals") {
    initExpr(ExprNode(pos1, StringLiteral{"A"}));

    auto const value = run();
    CHECK(value == Value{"A"});
}

TEST_CASE_FIXTURE(InterpreterFixture, "Interpreter evals bool true literal") {
    initExpr(ExprNode(pos1, BoolLiteral{true}));

    auto const value = run();
    CHECK(value == Value{true});
}

TEST_CASE_FIXTURE(InterpreterFixture, "Interpreter evals bool false literal") {
    initExpr(ExprNode(pos1, BoolLiteral{false}));

    auto const value = run();
    CHECK(value == Value{false});
}

TEST_CASE_FIXTURE(InterpreterFixture, "Interpreter evals simple vec literals") {
    initExpr(ExprNode(pos1, VecLiteral{.elements = makeVec<ExprNode>(
                                           ExprNode(pos1, IntLiteral{1}),
                                           ExprNode(pos1, IntLiteral{2}))}));

    auto const value = run();
    CHECK(value == Value{VecValue{.type = PrimitiveType::Int,
                                  .elements = {Value{1}, Value{2}}}});
}

TEST_CASE_FIXTURE(InterpreterFixture, "Interpreter evals simple vec literals") {
    initExpr(ExprNode(pos1, VecLiteral{.elements = makeVec<ExprNode>(
                                           ExprNode(pos1, IntLiteral{1}),
                                           ExprNode(pos1, IntLiteral{2}))}));

    auto const value = run();
    CHECK(value == Value{VecValue{.type = Type{PrimitiveType::Int},
                                  .elements = {Value{1}, Value{2}}}});
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter doesn't allow different types in vec literals") {
    initExpr(
        ExprNode(pos2, VecLiteral{.elements = makeVec<ExprNode>(
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
    initExpr(ExprNode(
        pos1,
        VecLiteral{
            .elements = makeVec<ExprNode>(
                ExprNode(pos1, VecLiteral{.elements = makeVec<ExprNode>(
                                              ExprNode(pos1, IntLiteral{1}))}),
                ExprNode(pos1,
                         VecLiteral{.elements = makeVec<ExprNode>(
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
    initExpr(ExprNode(pos1, AddExpr(ExprNode(pos1, IntLiteral{1}),
                                    ExprNode(pos1, IntLiteral{2}))));

    auto const value = run();

    CHECK(value == Value{3});
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter evals comparison expressions") {
    initExpr(ExprNode(pos1, LeExpr(ExprNode(pos1, IntLiteral{1}),
                                   ExprNode(pos1, IntLiteral{2}))));

    auto const value = run();

    CHECK(value == Value{true});
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter returns from nested blocks") {
    initMain(makeStatements(StatementNode(
        pos1,
        BlockNode(pos1,
                  Block{makeStatements(StatementNode(
                      pos1, ReturnStmt{ExprNode(pos1, IntLiteral{1})}))}))));

    auto const value = run();
    CHECK(value == Value{1});
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter throws on break statements outside of loops") {
    initMain(makeStatements(StatementNode(pos1, BreakStmt{})));

    auto const value = run();
    CHECK(value == std::unexpected(
                       RuntimeError{.kind = UnexpectedBreak{}, .pos = pos1}));
}

TEST_CASE_FIXTURE(
    InterpreterFixture,
    "Interpreter throws on continue statements outside of loops") {
    initMain(makeStatements(StatementNode(pos1, ContinueStmt{})));

    auto const value = run();
    CHECK(value == std::unexpected(RuntimeError{.kind = UnexpectedContinue{},
                                                .pos = pos1}));
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter assigns value to statements") {
    initMain(makeStatements(
        StatementNode(pos1, LetBinding{.identifier = "a",
                                       .value = ExprNode(pos1, IntLiteral{1})}),
        return_a()));

    auto const value = run();
    CHECK(value == Value{1});
}

TEST_CASE_FIXTURE(
    InterpreterFixture,
    "Interpreter doesn't allow variable redefinition in the same scope") {
    initMain(makeStatements(
        StatementNode(pos2, LetBinding{.identifier = "a",
                                       .value = ExprNode(pos1, IntLiteral{1})}),
        StatementNode(pos3,
                      LetBinding{.identifier = "a",
                                 .value = ExprNode(pos1, IntLiteral{1})})));

    auto const value = run();
    CHECK(value == std::unexpected(RuntimeError{
                       .kind = VariableRedefinition{.original_pos = pos2},
                       .pos = pos3}));
}

TEST_CASE_FIXTURE(InterpreterFixture, "Interpreter let assignment with type") {
    initMain(makeStatements(
        StatementNode(pos2,
                      LetBinding{.identifier = "a",
                                 .type = TypeNode(pos1, PrimitiveType::Int),
                                 .value = ExprNode(pos1, IntLiteral{1})}),
        return_a()));

    auto const value = run();
    CHECK(value == 1);
}

TEST_CASE_FIXTURE(
    InterpreterFixture,
    "Interpreter let assignement with type, requires types to match") {
    initMain(makeStatements(StatementNode(
        pos2, LetBinding{.identifier = "a",
                         .type = TypeNode(pos1, PrimitiveType::Str),
                         .value = ExprNode(pos1, IntLiteral{1})})));

    auto const value = run();
    CHECK(value == std::unexpected(RuntimeError{
                       .kind = AssignmentTypeMismatch{}, .pos = pos2}));
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter throws on usage of an undeclared variable") {
    initMain(makeStatements(
        StatementNode(pos1, ReturnStmt{ExprNode(pos2, Identifier{"a"})})));

    auto const value = run();
    CHECK(value == std::unexpected(RuntimeError{
                       .kind = UseOfUndeclaredVariable{}, .pos = pos2}));
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter allows shadowing variables in inner scopes") {
    initMain(makeStatements(
        let_a_eq(1),
        StatementNode(pos1, BlockNode(pos1, Block{makeStatements(
                                                let_a_eq(2), return_a())}))));

    auto const value = run();
    CHECK(value == Value{2});
}

TEST_CASE_FIXTURE(
    InterpreterFixture,
    "Interpreter restores shadowed variable value once block ends") {
    initMain(makeStatements(
        let_a_eq(1),
        StatementNode(pos1,
                      BlockNode(pos1, Block{makeStatements(let_a_eq(0))})),
        return_a()));

    auto const value = run();
    CHECK(value == Value{1});
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter sees variables in scopes above itself") {
    initMain(makeStatements(
        let_a_eq(1),
        StatementNode(pos1,
                      BlockNode(pos1, Block{makeStatements(return_a())}))));

    auto const value = run();
    CHECK(value == Value{1});
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter assigns values to variables") {
    initMain(makeStatements(
        let_mut_a_eq(1),
        StatementNode(pos1, AssignStmt(LValue{.identifier = "a"},
                                       ExprNode(pos1, IntLiteral{2}))),
        return_a()));

    auto const value = run();
    CHECK(value == Value{2});
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter doesn't allow assignment to non mut variables") {
    initMain(makeStatements(
        let_a_eq(1),
        StatementNode(pos2, AssignStmt(LValue{.identifier = "a"},
                                       ExprNode(pos1, IntLiteral{2}))),
        return_a()));

    auto const value = run();
    CHECK(value ==
          std::unexpected(RuntimeError{
              .kind = ConstAssignment{.identifier = "a"}, .pos = pos2}));
}

TEST_CASE_FIXTURE(InterpreterFixture, "Interpreter runs while loops") {
    initMain(makeStatements(
        let_mut_a_eq(0),
        StatementNode(
            pos1,
            WhileLoop{
                .condition =
                    ExprNode(pos1, LtExpr(ExprNode(pos1, Identifier{"a"}),
                                          ExprNode(pos1, IntLiteral{5}))),
                .block = BlockNode(
                    pos1, Block{.statements = makeStatements(a_plus_eq_1())})}),
        return_a()));

    auto const value = run();
    CHECK(value == Value{5});
}

TEST_CASE_FIXTURE(InterpreterFixture, "Interpreter break ends while lopp") {
    initMain(makeStatements(
        let_mut_a_eq(0),
        whileALt5(makeStatements(StatementNode(
            pos1,
            IfStmt{
                .if_cond =
                    ExprNode(pos1, EqExpr(ExprNode(pos1, Identifier{"a"}),
                                          ExprNode(pos1, IntLiteral{3}))),
                .if_block = BlockNode(
                    pos1, Block{.statements = makeStatements(
                                    StatementNode(pos1, BreakStmt{}))}),
            }))),
        return_a()));
    CHECK(run() == 3);
}

TEST_CASE_FIXTURE(InterpreterFixture, "Interpreter continue works in while") {
    initMain(makeStatements(
        let_mut_a_eq(0),
        StatementNode(
            pos1,
            WhileLoop{.condition =
                          ExprNode(pos1, LtExpr(ExprNode(pos1, Identifier{"a"}),
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
    initMain(makeStatements(
        let_mut_a_eq(""),
        StatementNode(
            pos1,
            ForLoop{.loop_var = "b",
                    .mut = false,
                    .range = range_in_0_5(pos1),
                    .block = BlockNode(pos1, Block{.statements = makeStatements(
                                                       a_plus_eq_b())})}),
        return_a()));
    CHECK(run() == "01234");
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter runs for loop with custom step") {
    initMain(makeStatements(
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
                    .block = BlockNode(pos1, Block{.statements = makeStatements(
                                                       a_plus_eq_b())})}),
        return_a()));
    CHECK(run() == "024");
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter runs for loop reverse default step") {
    initMain(makeStatements(
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
                    .block = BlockNode(pos1, Block{.statements = makeStatements(
                                                       a_plus_eq_b())})}),
        return_a()));
    CHECK(run() == "54321");
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter runs for loop reverse custom step") {
    initMain(makeStatements(
        let_mut_a_eq(""),
        StatementNode(
            pos1,
            ForLoop{.loop_var = "b",
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
    initMain(makeStatements(
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
                    .block = BlockNode(pos1, Block{.statements = makeStatements(
                                                       a_plus_eq_b())})}),
        return_a()));
    CHECK(run() == "012345");
}

TEST_CASE_FIXTURE(InterpreterFixture, "Interpreter runs for loop continue") {
    initMain(makeStatements(
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
    initMain(makeStatements(
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
                                        EqExpr(ExprNode(pos1, Identifier{"b"}),
                                               ExprNode(pos1, IntLiteral{2}))),
                                    .if_block = BlockNode(
                                        pos1,
                                        Block{.statements =
                                                  makeStatements(StatementNode(
                                                      pos1, BreakStmt{}))}),
                                }),
                            a_plus_eq_b())})}),
        return_a()));
    auto const value = run();
    CHECK(value == "01");
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter for loop, return exists loop") {
    initMain(makeStatements(
        let_mut_a_eq(""),
        StatementNode(
            pos1, ForLoop{.loop_var = "b",
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
    initMain(makeStatements(
        let_mut_a_eq(""),
        StatementNode(
            pos1,
            ForLoop{
                .loop_var = "b",
                .mut = false,
                .range = range_in_0_5(pos1),
                .block = BlockNode(
                    pos1, Block{makeStatements(StatementNode(
                              pos3, AssignStmt(
                                        LValue{.identifier = "b"},
                                        ExprNode(pos1, IntLiteral{99}))))})})));
    auto const value = run();
    CHECK(value ==
          std::unexpected(RuntimeError{
              .kind = ConstAssignment{.identifier = "b"}, .pos = pos3}));
}

TEST_CASE_FIXTURE(
    InterpreterFixture,
    "Interpreter for loop, loop variable not modified next iteration") {
    initMain(makeStatements(
        let_mut_a_eq(""),
        StatementNode(
            pos1,
            ForLoop{.loop_var = "b",
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
    initMain(makeStatements(
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
    initMain(makeStatements(
        let_mut_a_eq(0),
        StatementNode(
            pos1,
            ForLoop{.loop_var = "b",
                    .mut = false,
                    .range = RangeNode(
                        pos3, Range{.start = ExprNode(pos1, IntLiteral{0}),
                                    .inclusive = false,
                                    .end = ExprNode(pos1, IntLiteral{4}),
                                    .step = ExprNode(pos1, IntLiteral{-1})}),
                    .block = BlockNode(pos1, Block{})})));
    CHECK(run() == std::unexpected(
                       RuntimeError{.kind = InvalidForRange{}, .pos = pos3}));
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter for loop invalid range - step 0") {
    initMain(makeStatements(
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
    initMain(makeStatements(
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
    initMain(makeStatements(
        StatementNode(
            pos1,
            IfStmt{
                .if_cond = ExprNode(pos1, BoolLiteral{true}),
                .if_block = BlockNode(
                    pos1, Block{.statements = makeStatements(return_val(1))}),
            }),
        return_val(2)));
    CHECK(run() == 1);
}

TEST_CASE_FIXTURE(InterpreterFixture, "Interpreter if - false skips block") {
    initMain(makeStatements(
        StatementNode(
            pos1,
            IfStmt{
                .if_cond = ExprNode(pos1, BoolLiteral{false}),
                .if_block = BlockNode(
                    pos1, Block{.statements = makeStatements(return_val(1))}),
            }),
        return_val(2)));
    CHECK(run() == 2);
}

TEST_CASE_FIXTURE(InterpreterFixture, "Interpreter else if - first true") {
    initMain(makeStatements(StatementNode(
        pos1, IfStmt{
                  .if_cond = ExprNode(pos1, BoolLiteral{true}),
                  .if_block = BlockNode(
                      pos1, Block{.statements = makeStatements(return_val(1))}),
                  .else_if_branches = makeElseIfs(std::make_pair(
                      ExprNode(pos1, BoolLiteral{true}),
                      BlockNode(pos1, Block{.statements = makeStatements(
                                                return_val(2))}))),
              })));
    CHECK(run() == 1);
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter else if - first false second true") {
    initMain(makeStatements(StatementNode(
        pos1, IfStmt{
                  .if_cond = ExprNode(pos1, BoolLiteral{false}),
                  .if_block = BlockNode(
                      pos1, Block{.statements = makeStatements(return_val(1))}),
                  .else_if_branches = makeElseIfs(std::make_pair(
                      ExprNode(pos1, BoolLiteral{true}),
                      BlockNode(pos1, Block{.statements = makeStatements(
                                                return_val(2))}))),
              })));
    CHECK(run() == 2);
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter else if - all false executes else") {
    initMain(makeStatements(StatementNode(
        pos1, IfStmt{
                  .if_cond = ExprNode(pos1, BoolLiteral{false}),
                  .if_block = BlockNode(
                      pos1, Block{.statements = makeStatements(return_val(1))}),
                  .else_if_branches = makeElseIfs(std::make_pair(
                      ExprNode(pos1, BoolLiteral{false}),
                      BlockNode(pos1, Block{.statements = makeStatements(
                                                return_val(2))}))),
                  .else_block = BlockNode(
                      pos1, Block{.statements = makeStatements(return_val(3))}),
              })));
    CHECK(run() == 3);
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter else if - all false no else") {
    initMain(makeStatements(
        StatementNode(
            pos1,
            IfStmt{
                .if_cond = ExprNode(pos1, BoolLiteral{false}),
                .if_block = BlockNode(
                    pos1, Block{.statements = makeStatements(return_val(1))}),
                .else_if_branches = makeElseIfs(std::make_pair(
                    ExprNode(pos1, BoolLiteral{false}),
                    BlockNode(pos1, Block{.statements =
                                              makeStatements(return_val(2))}))),
            }),
        return_val(4)));
    CHECK(run() == 4);
}

TEST_CASE_FIXTURE(InterpreterFixture, "Interpreter else if - first true") {
    initMain(makeStatements(StatementNode(
        pos1, IfStmt{
                  .if_cond = ExprNode(pos1, BoolLiteral{true}),
                  .if_block = BlockNode(
                      pos1, Block{.statements = makeStatements(return_val(1))}),
                  .else_if_branches = makeElseIfs(std::make_pair(
                      ExprNode(pos1, BoolLiteral{true}),
                      BlockNode(pos1, Block{.statements = makeStatements(
                                                return_val(2))}))),
              })));
    CHECK(run() == 1);
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter else if - first false second true") {
    initMain(makeStatements(StatementNode(
        pos1, IfStmt{
                  .if_cond = ExprNode(pos1, BoolLiteral{false}),
                  .if_block = BlockNode(
                      pos1, Block{.statements = makeStatements(return_val(1))}),
                  .else_if_branches = makeElseIfs(std::make_pair(
                      ExprNode(pos1, BoolLiteral{true}),
                      BlockNode(pos1, Block{.statements = makeStatements(
                                                return_val(2))}))),
              })));
    CHECK(run() == 2);
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter else if - all false executes else") {
    initMain(makeStatements(StatementNode(
        pos1, IfStmt{
                  .if_cond = ExprNode(pos1, BoolLiteral{false}),
                  .if_block = BlockNode(
                      pos1, Block{.statements = makeStatements(return_val(1))}),
                  .else_if_branches = makeElseIfs(std::make_pair(
                      ExprNode(pos1, BoolLiteral{false}),
                      BlockNode(pos1, Block{.statements = makeStatements(
                                                return_val(2))}))),
                  .else_block = BlockNode(
                      pos1, Block{.statements = makeStatements(return_val(3))}),
              })));
    CHECK(run() == 3);
}

TEST_CASE_FIXTURE(InterpreterFixture,
                  "Interpreter else if - all false no else") {
    initMain(makeStatements(
        StatementNode(
            pos1,
            IfStmt{
                .if_cond = ExprNode(pos1, BoolLiteral{false}),
                .if_block = BlockNode(
                    pos1, Block{.statements = makeStatements(return_val(1))}),
                .else_if_branches = makeElseIfs(std::make_pair(
                    ExprNode(pos1, BoolLiteral{false}),
                    BlockNode(pos1, Block{.statements =
                                              makeStatements(return_val(2))}))),
            }),
        return_val(4)));
    CHECK(run() == 4);
}
