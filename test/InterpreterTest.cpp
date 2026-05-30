#include <expected>
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

    static Value makeVecValue(Type element_type, std::vector<Value> elements) {
        return Value{VecValue{.type = std::move(element_type),
                              .elements = std::move(elements)}};
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

    StatementNode let_a_eq(std::int64_t x) {  // NOLINT
        return StatementNode(
            pos0_, LetBinding{.identifier = "a",
                              .mut = false,
                              .value = ExprNode(pos0_, IntLiteral{x})});
    }
    StatementNode let_mut_a_eq(std::int64_t x) {  // NOLINT
        return StatementNode(
            pos0_, LetBinding{.identifier = "a",
                              .mut = true,
                              .value = ExprNode(pos0_, IntLiteral{x})});
    }

    StatementNode return_val(Value val) {
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
        return StatementNode(
            pos0_, ReturnStmt{.value = ExprNode(pos0_, std::move(expr))});
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
                    pos1,
                    Block{.statements = makeStatements(StatementNode(
                              pos1,
                              AssignStmt(
                                  LValue{.identifier = "a", .indices = {}},
                                  ExprNode(
                                      pos1,
                                      AddExpr(ExprNode(pos1, Identifier{"a"}),
                                              ExprNode(pos1,
                                                       IntLiteral{1}))))))})}),
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
            WhileLoop{
                .condition =
                    ExprNode(pos1, LtExpr(ExprNode(pos1, Identifier{"a"}),
                                          ExprNode(pos1, IntLiteral{3}))),
                .block = BlockNode(
                    pos1,
                    Block{.statements = makeStatements(
                              StatementNode(
                                  pos1,
                                  AssignStmt(
                                      LValue{.identifier = "a", .indices = {}},
                                      ExprNode(
                                          pos1,
                                          AddExpr(
                                              ExprNode(pos1, Identifier{"a"}),
                                              ExprNode(pos1, IntLiteral{1}))))),
                              StatementNode(pos1, ContinueStmt{}),
                              return_val(99))})}),
        return_a()));
    CHECK(run() == 3);
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
