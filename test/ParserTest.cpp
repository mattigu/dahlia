
#include <optional>
#include <ranges>
#include <sstream>
#include <utility>
#include <variant>
#include <vector>

#include "doctest.h"
#include "src/Ast.h"
#include "src/Diagnostics.hpp"
#include "src/Parser.h"
#include "src/ParserDiagnostic.h"
#include "src/Position.h"
#include "src/Token.h"

struct MockToken {
public:
    MockToken(TokenKind kind, TokenValue val = std::monostate{})
        : kind_(kind), value_(std::move(val)) {};

    TokenKind kind_;
    TokenValue value_;
};

struct MockLexer {
    explicit MockLexer(std::vector<MockToken> tokens)
        : tokens_{std::move(tokens)},
          current_{tokens_.begin()},
          pos_{Position{.line = 1, .column = 1, .offset = 0}} {}

    [[nodiscard]] Token current() const { return makeToken(*current_, pos_); }
    Token next() {
        if (std::next(current_) != tokens_.cend()) {
            ++current_;
            pos_.column += 1;
            pos_.offset += 1;
        }
        return makeToken(*current_, pos_);
    }
    [[nodiscard]] Diagnostics<LexerDiagnosticKind> const& diagnostics() const {
        return diagnostics_;
    }

private:
    std::vector<MockToken> tokens_;
    std::vector<MockToken>::const_iterator current_;
    Diagnostics<LexerDiagnosticKind> diagnostics_;

    Position pos_;

    static Token makeToken(MockToken const& mock, Position pos) {
        return {mock.kind_, pos, mock.value_};
    }
};

using MockParser = ParserTemplate<MockLexer>;

class ParserFixture {
public:
    std::span<Position> init(std::vector<MockToken> tokens) {
        positions_.clear();
        positions_.reserve(tokens.size());
        tokens.emplace_back(TokenKind::ETX);
        for (auto [idx, token] : std::views::enumerate(tokens)) {
            positions_.push_back(Position{.line = 1,
                                          .column = static_cast<int>(idx + 1),
                                          .offset = static_cast<int>(idx)});
        }
        parser_.emplace(MockLexer{std::move(tokens)});
        return positions_;
    }

    std::span<Position> initValidated(std::string const& src,
                                      std::vector<MockToken> tokens) {
        assertTokensMatch(src, tokens);
        return init(std::move(tokens));
    }

    std::pair<TypeNode, std::span<Position const>> parseType(
        std::vector<MockToken> type_tokens) {
        std::vector<MockToken> tokens;

        tokens.append_range(
            std::to_array<MockToken>({{TokenKind::Fn},
                                      {TokenKind::Identifier, std::string{"f"}},
                                      {TokenKind::ParenOpen},
                                      {TokenKind::ParenClose},
                                      {TokenKind::MinusGreater}}));

        auto const offset = tokens.size();

        tokens.append_range(type_tokens);

        tokens.append_range(std::to_array<MockToken>(
            {{TokenKind::BraceOpen}, {TokenKind::BraceClose}}));

        auto const all_pos = init(std::move(tokens));

        auto program = parse();
        REQUIRE(program.has_value());

        auto& prog = **program;
        REQUIRE(prog.functions.contains("f"));

        auto& ret = prog.functions.at("f")->return_type;
        REQUIRE(ret.has_value());

        return {std::move(*ret), all_pos.subspan(offset)};
    }

    std::pair<std::vector<StatementNode>, std::span<Position const>>
    parseStatement(std::vector<MockToken> stmt_tokens) {
        std::vector<MockToken> tokens;

        tokens.append_range(
            std::to_array<MockToken>({{TokenKind::Fn},
                                      {TokenKind::Identifier, std::string{"f"}},
                                      {TokenKind::ParenOpen},
                                      {TokenKind::ParenClose},
                                      {TokenKind::BraceOpen}}));

        auto const offset = tokens.size();

        tokens.append_range(stmt_tokens);

        tokens.append_range(std::to_array<MockToken>({
            {TokenKind::BraceClose},
        }));

        auto const all_pos = init(std::move(tokens));

        auto program = parse();
        REQUIRE(program.has_value());

        auto& prog = **program;
        REQUIRE(prog.functions.contains("f"));

        auto& func = prog.functions.at("f");
        REQUIRE(!func->block->statements.empty());

        return {std::move(func->block->statements), all_pos.subspan(offset)};
    }

    std::pair<ExprNode, std::span<Position const>> parseExpression(
        std::vector<MockToken> expr_tokens) {
        std::vector<MockToken> tokens;

        tokens.append_range(
            std::to_array<MockToken>({{TokenKind::Fn},
                                      {TokenKind::Identifier, std::string{"f"}},
                                      {TokenKind::ParenOpen},
                                      {TokenKind::ParenClose},
                                      {TokenKind::BraceOpen},
                                      {TokenKind::Let},
                                      {TokenKind::Identifier, std::string{"x"}},
                                      {TokenKind::Equal}}));

        auto const offset = tokens.size();

        tokens.append_range(expr_tokens);

        tokens.append_range(std::to_array<MockToken>(
            {{TokenKind::Semicolon}, {TokenKind::BraceClose}}));

        init(std::move(tokens));

        auto program = parse();
        REQUIRE(program.has_value());

        auto& prog = **program;
        REQUIRE(prog.functions.contains("f"));

        auto& func = prog.functions.at("f");

        REQUIRE(!func->block->statements.empty());

        auto& stmt = func->block->statements.front();
        REQUIRE(std::holds_alternative<LetBinding>(*stmt));

        auto& let = std::get<LetBinding>(*stmt);
        return std::make_pair(std::move(let.value),
                              std::span(positions_).subspan(offset));
    }

    static constexpr void assertTokensMatch(
        std::string const& src, std::vector<MockToken> const& tokens) {
        std::istringstream stream{src};
        Lexer lexer{stream};
        lexer.next();
        for (auto const& mock : tokens) {
            auto const token = lexer.current();
            REQUIRE(token.kind() == mock.kind_);
            REQUIRE(token.value() == mock.value_);
            lexer.next();
        }
    }

    template <typename T>
    Node<T> testNode(T value) {
        return Node<T>{Position{}, std::move(value)};
    }

    template <typename T, typename... Args>
    std::vector<T> makeVec(Args&&... args) {
        std::vector<T> vec;
        vec.reserve(sizeof...(args));
        (vec.emplace_back(std::forward<Args>(args)), ...);
        return vec;
    }

    static std::vector<ParserDiagnostic> makeDiags(
        std::initializer_list<ParserDiagnostic> diags) {
        return diags;
    }

    template <typename... Args>
    std::vector<StatementNode> makeStatements(Args&&... args) {
        return makeVec<StatementNode>(std::forward<Args>(args)...);
    }

    std::optional<ProgramNode> parse() { return parser_->parse(); }
    auto const& diagnostics() { return parser_->diagnostics(); }

private:
    std::optional<MockParser> parser_;
    std::vector<Position> positions_;
};

TEST_CASE_FIXTURE(ParserFixture, "Parser parses empty program") {
    auto const pos = initValidated("", {});

    auto const program = parse();

    CHECK(program == ProgramNode(pos[0], Program{}));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses empty function") {
    auto const pos =
        initValidated("fn main() {}", {{TokenKind::Fn},
                                       {TokenKind::Identifier, "main"},
                                       {TokenKind::ParenOpen},
                                       {TokenKind::ParenClose},
                                       {TokenKind::BraceOpen},
                                       {TokenKind::BraceClose}});

    auto const program = parse();

    REQUIRE(program.has_value());
    REQUIRE(program.value()->functions.contains("main"));

    auto const& function_node = program.value()->functions.at("main");

    CHECK(function_node ==
          FunctionNode(pos[0], Function{.identifier = "main",
                                        .params = {},
                                        .block = BlockNode(pos[4], Block{})}));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser skips comments") {
    auto const pos =
        initValidated("fn main() {#abc\n}", {{TokenKind::Fn},
                                             {TokenKind::Identifier, "main"},
                                             {TokenKind::ParenOpen},
                                             {TokenKind::ParenClose},
                                             {TokenKind::BraceOpen},
                                             {TokenKind::Comment, "abc"},
                                             {TokenKind::BraceClose}});

    auto const program = parse();

    REQUIRE(program.has_value());
    REQUIRE(program.value()->functions.contains("main"));

    auto const& function_node = program.value()->functions.at("main");

    CHECK(function_node ==
          FunctionNode(pos[0], Function{.identifier = "main",
                                        .params = {},
                                        .block = BlockNode(pos[4], Block{})}));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser detects redefined functions") {
    auto const pos = initValidated("fn main() {} fn main() {}",
                                   {{TokenKind::Fn},
                                    {TokenKind::Identifier, "main"},
                                    {TokenKind::ParenOpen},
                                    {TokenKind::ParenClose},
                                    {TokenKind::BraceOpen},
                                    {TokenKind::BraceClose},
                                    {TokenKind::Fn},
                                    {TokenKind::Identifier, "main"},
                                    {TokenKind::ParenOpen},
                                    {TokenKind::ParenClose},
                                    {TokenKind::BraceOpen},
                                    {TokenKind::BraceClose}});

    auto const program = parse();

    REQUIRE(!program.has_value());

    CHECK(diagnostics().all() ==
          makeDiags({{.kind = {FunctionRedefined{.identifier = "main",
                                                 .original_pos = pos[0]}},
                      .pos = pos[6],
                      .severity = Severity::Error}}));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses function parameters") {
    auto const pos =
        initValidated("fn main(mut a : int, b : [str]) {}",
                      {{TokenKind::Fn},
                       {TokenKind::Identifier, std::string{"main"}},
                       {TokenKind::ParenOpen},
                       {TokenKind::Mut},
                       {TokenKind::Identifier, std::string{"a"}},
                       {TokenKind::Colon},
                       {TokenKind::Int},
                       {TokenKind::Comma},
                       {TokenKind::Identifier, std::string{"b"}},
                       {TokenKind::Colon},
                       {TokenKind::BracketOpen},
                       {TokenKind::Str},
                       {TokenKind::BracketClose},
                       {TokenKind::ParenClose},
                       {TokenKind::BraceOpen},
                       {TokenKind::BraceClose}});

    auto const program = parse();

    REQUIRE(program.has_value());
    REQUIRE(program.value()->functions.contains("main"));

    auto const& function_node = program.value()->functions.at("main");
    REQUIRE(function_node->params.size() == 2);

    auto const& param_a = function_node->params[0];
    CHECK(param_a ==
          ParamNode{pos[3], Param{.type = {pos[6], PrimitiveType::Int},
                                  .identifier = "a",
                                  .mut = true}});

    auto const& param_b = function_node->params[1];
    CHECK(param_b ==
          ParamNode{pos[8],
                    Param{.type = {pos[10], Type::vec(PrimitiveType::Str)},
                          .identifier = "b",
                          .mut = false}});
}

TEST_CASE_FIXTURE(ParserFixture, "Parser detect duplicated pararemer names") {
    auto const pos =
        initValidated("fn main(a : int, a : int) {}",
                      {{TokenKind::Fn},
                       {TokenKind::Identifier, std::string{"main"}},
                       {TokenKind::ParenOpen},
                       {TokenKind::Identifier, std::string{"a"}},
                       {TokenKind::Colon},
                       {TokenKind::Int},
                       {TokenKind::Comma},
                       {TokenKind::Identifier, std::string{"a"}},
                       {TokenKind::Colon},
                       {TokenKind::Int},
                       {TokenKind::ParenClose},
                       {TokenKind::BraceOpen},
                       {TokenKind::BraceClose}});

    auto const program = parse();

    REQUIRE(!program.has_value());
    CHECK(diagnostics().all() ==
          makeDiags({{.kind = ParameterRedefined{.identifier = "a",
                                                 .original_pos = pos[3]},
                      .pos = pos[7],
                      .severity = Severity::Error}}));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses return type") {
    auto const pos =
        initValidated("fn main() -> int {}", {{TokenKind::Fn},
                                              {TokenKind::Identifier, "main"},
                                              {TokenKind::ParenOpen},
                                              {TokenKind::ParenClose},
                                              {TokenKind::MinusGreater},
                                              {TokenKind::Int},
                                              {TokenKind::BraceOpen},
                                              {TokenKind::BraceClose}});

    auto const program = parse();

    REQUIRE(program.has_value());
    REQUIRE(program.value()->functions.contains("main"));

    auto const& return_type =
        program.value()->functions.at("main")->return_type;

    CHECK(return_type == TypeNode(pos[5], PrimitiveType::Int));
}

TEST_CASE_FIXTURE(ParserFixture,
                  "Parser errors on missing function identifier") {
    auto const pos = initValidated("fn () {}", {{TokenKind::Fn},
                                                {TokenKind::ParenOpen},
                                                {TokenKind::ParenClose},
                                                {TokenKind::BraceOpen},
                                                {TokenKind::BraceClose}});

    auto const program = parse();

    REQUIRE(!program.has_value());

    CHECK(diagnostics().all() == makeDiags({{.kind = {ExpectedIdentifier{}},
                                             .pos = pos[1],
                                             .severity = Severity::Fatal}}));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser errors on missing param type") {
    auto const pos =
        initValidated("fn main(a : ) {}", {{TokenKind::Fn},
                                           {TokenKind::Identifier, "main"},
                                           {TokenKind::ParenOpen},
                                           {TokenKind::Identifier, "a"},
                                           {TokenKind::Colon},
                                           {TokenKind::ParenClose},
                                           {TokenKind::BraceOpen},
                                           {TokenKind::BraceClose}});

    auto const program = parse();
    REQUIRE(!program.has_value());

    CHECK(diagnostics().all() == makeDiags({{.kind = {ExpectedType{}},
                                             .pos = pos[5],
                                             .severity = Severity::Fatal}}));
}

TEST_CASE_FIXTURE(ParserFixture,
                  "Parser errors on incorrect function return type") {
    auto const pos =
        initValidated("fn main() -> ; {}", {{TokenKind::Fn},
                                            {TokenKind::Identifier, "main"},
                                            {TokenKind::ParenOpen},
                                            {TokenKind::ParenClose},
                                            {TokenKind::MinusGreater},
                                            {TokenKind::Semicolon},
                                            {TokenKind::BraceOpen},
                                            {TokenKind::BraceClose}});

    auto const program = parse();

    REQUIRE(!program.has_value());

    CHECK(diagnostics().all() == makeDiags({{.kind = {ExpectedType{}},
                                             .pos = pos[5],
                                             .severity = Severity::Fatal}}));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser errors on missing function block") {
    auto const pos =
        initValidated("fn main () ;", {{TokenKind::Fn},
                                       {TokenKind::Identifier, "main"},
                                       {TokenKind::ParenOpen},
                                       {TokenKind::ParenClose},
                                       {TokenKind::Semicolon}});

    auto const program = parse();

    REQUIRE(!program.has_value());

    CHECK(diagnostics().all() == makeDiags({{.kind = {ExpectedBlock{}},
                                             .pos = pos[4],
                                             .severity = Severity::Fatal}}));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses int type") {
    auto const [type, pos] = parseType({{TokenKind::Int}});
    CHECK(type == TypeNode(pos[0], PrimitiveType::Int));
}
TEST_CASE_FIXTURE(ParserFixture, "Parser parses float type") {
    auto const [type, pos] = parseType({{TokenKind::Float}});
    CHECK(type == TypeNode(pos[0], PrimitiveType::Float));
}
TEST_CASE_FIXTURE(ParserFixture, "Parser parses bool type") {
    auto const [type, pos] = parseType({{TokenKind::Bool}});
    CHECK(type == TypeNode(pos[0], PrimitiveType::Bool));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses str type") {
    auto const [type, pos] = parseType({{TokenKind::Str}});
    CHECK(type == TypeNode(pos[0], PrimitiveType::Str));
}

TEST_CASE_FIXTURE(ParserFixture,
                  "Parser parses vectors containing primitives") {
    auto const [type, pos] = parseType({{TokenKind::BracketOpen},
                                        {TokenKind::Int},
                                        {TokenKind::BracketClose}});
    CHECK(type == TypeNode(pos[0], Type::vec(PrimitiveType::Int)));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses vectors containing vectors") {
    auto const [type, pos] = parseType({{TokenKind::BracketOpen},
                                        {TokenKind::BracketOpen},
                                        {TokenKind::Int},
                                        {TokenKind::BracketClose},
                                        {TokenKind::BracketClose}});
    CHECK(type == TypeNode(pos[0], Type::vec(Type::vec(PrimitiveType::Int))));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses int literal") {
    auto const [int_val, pos] = parseExpression({{TokenKind::IntLiteral, 1}});
    CHECK(int_val == ExprNode(pos[0], IntLiteral{.value = 1}));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses string literal") {
    auto const [str_val, pos] = parseExpression({{TokenKind::StrLiteral, "a"}});
    CHECK(str_val == ExprNode(pos[0], StringLiteral{.value = "a"}));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses float literal") {
    auto const [float_val, pos] =
        parseExpression({{TokenKind::FloatLiteral, 1.5}});
    CHECK(float_val == ExprNode(pos[0], FloatLiteral{.value = 1.5}));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses bool false literal") {
    auto const [bool_val, pos] = parseExpression({{TokenKind::False}});
    CHECK(bool_val == ExprNode(pos[0], BoolLiteral{.value = false}));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses bool true literal") {
    auto const [bool_val, pos] = parseExpression({{TokenKind::True}});
    CHECK(bool_val == ExprNode(pos[0], BoolLiteral{.value = true}));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses empty vec literal") {
    auto const [bool_val, pos] =
        parseExpression({{TokenKind::BracketOpen}, {TokenKind::BracketClose}});
    CHECK(bool_val == ExprNode(pos[0], VecLiteral{.elements = {}}));
}

// TODO! Test comma in empty vec once error handling finalized.

TEST_CASE_FIXTURE(ParserFixture,
                  "Parser parses simple vec literal with one element") {
    auto const [vec_val, pos] = parseExpression({{TokenKind::BracketOpen},
                                                 {TokenKind::IntLiteral, 1},
                                                 {TokenKind::BracketClose}});
    CHECK(vec_val ==
          ExprNode(pos[0], VecLiteral{.elements = makeVec<ExprNode>(ExprNode(
                                          pos[1], IntLiteral{.value = 1}))}));
}

TEST_CASE_FIXTURE(ParserFixture,
                  "Parser parses simple vec with trailing comma") {
    auto [vec_val, pos] = parseExpression({{TokenKind::BracketOpen},
                                           {TokenKind::IntLiteral, 1},
                                           {TokenKind::Comma},
                                           {TokenKind::BracketClose}});
    CHECK(vec_val ==
          ExprNode(pos[0], VecLiteral{.elements = makeVec<ExprNode>(ExprNode(
                                          pos[1], IntLiteral{.value = 1}))}));
}

TEST_CASE_FIXTURE(ParserFixture,
                  "Parser parses simple vec literal with multiple elements") {
    auto const [vec_val, pos] = parseExpression({{TokenKind::BracketOpen},
                                                 {TokenKind::IntLiteral, 1},
                                                 {TokenKind::Comma},
                                                 {TokenKind::IntLiteral, 2},
                                                 {TokenKind::BracketClose}});
    CHECK(vec_val ==
          ExprNode(pos[0],
                   VecLiteral{.elements = makeVec<ExprNode>(
                                  ExprNode(pos[1], IntLiteral{.value = 1}),
                                  ExprNode(pos[3], IntLiteral{.value = 2}))}));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses vec literal with nested vecs") {
    auto const [vec_val, pos] = parseExpression({{TokenKind::BracketOpen},
                                                 {TokenKind::BracketOpen},
                                                 {TokenKind::IntLiteral, 1},
                                                 {TokenKind::BracketClose},
                                                 {TokenKind::BracketClose}});
    CHECK(
        vec_val ==
        ExprNode(pos[0],
                 VecLiteral{
                     .elements = makeVec<ExprNode>(ExprNode(
                         pos[1],
                         VecLiteral{.elements = makeVec<ExprNode>(ExprNode(
                                        pos[2], IntLiteral{.value = 1}))}))}));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses let statement without type") {
    auto const [statements, pos] = parseStatement({{TokenKind::Let},
                                                   {TokenKind::Mut},
                                                   {TokenKind::Identifier, "a"},
                                                   {TokenKind::Equal},
                                                   {TokenKind::IntLiteral, 1},
                                                   {TokenKind::Semicolon}});

    REQUIRE(statements.size() == 1);
    auto const& statement = statements[0];
    CHECK(statement ==
          StatementNode(
              pos[0],
              LetBinding{.identifier = "a",
                         .mut = true,
                         .type = std::nullopt,
                         .value = ExprNode(pos[4], IntLiteral{.value = 1})}));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses let statement with type") {
    auto const [statements, pos] = parseStatement({{TokenKind::Let},
                                                   {TokenKind::Mut},
                                                   {TokenKind::Identifier, "a"},
                                                   {TokenKind::Colon},
                                                   {TokenKind::Int},
                                                   {TokenKind::Equal},
                                                   {TokenKind::IntLiteral, 1},
                                                   {TokenKind::Semicolon}});

    REQUIRE(statements.size() == 1);
    auto const& statement = statements[0];
    CHECK(statement ==
          StatementNode(
              pos[0],
              LetBinding{.identifier = "a",
                         .mut = true,
                         .type = TypeNode(pos[4], Type(PrimitiveType::Int)),
                         .value = ExprNode(pos[6], IntLiteral{.value = 1})}));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses simple assignment") {
    auto const [stmts, pos] = parseStatement({{TokenKind::Identifier, "a"},
                                              {TokenKind::Equal},
                                              {TokenKind::IntLiteral, 1},
                                              {TokenKind::Semicolon}});
    CHECK(stmts == makeStatements(StatementNode(
                       pos[0], AssignStmt{LValue{.identifier = "a"},
                                          ExprNode(pos[2], IntLiteral{1})})));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses add assignment") {
    auto const [stmts, pos] = parseStatement({{TokenKind::Identifier, "a"},
                                              {TokenKind::PlusEqual},
                                              {TokenKind::IntLiteral, 1},
                                              {TokenKind::Semicolon}});
    CHECK(stmts ==
          makeStatements(StatementNode(
              pos[0], AddAssignStmt{LValue{.identifier = "a"},
                                    ExprNode(pos[2], IntLiteral{1})})));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses sub assignment") {
    auto const [stmts, pos] = parseStatement({{TokenKind::Identifier, "a"},
                                              {TokenKind::MinusEqual},
                                              {TokenKind::IntLiteral, 1},
                                              {TokenKind::Semicolon}});
    CHECK(stmts ==
          makeStatements(StatementNode(
              pos[0], SubAssignStmt{LValue{.identifier = "a"},
                                    ExprNode(pos[2], IntLiteral{1})})));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses mul assignment") {
    auto const [stmts, pos] = parseStatement({{TokenKind::Identifier, "a"},
                                              {TokenKind::AsteriskEqual},
                                              {TokenKind::IntLiteral, 1},
                                              {TokenKind::Semicolon}});

    CHECK(stmts ==
          makeStatements(StatementNode(
              pos[0], MulAssignStmt{LValue{.identifier = "a"},
                                    ExprNode(pos[2], IntLiteral{1})})));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses div assignment") {
    auto const [stmts, pos] = parseStatement({{TokenKind::Identifier, "a"},
                                              {TokenKind::SlashEqual},
                                              {TokenKind::IntLiteral, 1},
                                              {TokenKind::Semicolon}});
    CHECK(stmts ==
          makeStatements(StatementNode(
              pos[0], DivAssignStmt{LValue{.identifier = "a"},
                                    ExprNode(pos[2], IntLiteral{1})})));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses mod assignment") {
    auto const [stmts, pos] = parseStatement({{TokenKind::Identifier, "a"},
                                              {TokenKind::PercentEqual},
                                              {TokenKind::IntLiteral, 1},
                                              {TokenKind::Semicolon}});
    CHECK(stmts ==
          makeStatements(StatementNode(
              pos[0], ModAssignStmt{LValue{.identifier = "a"},
                                    ExprNode(pos[2], IntLiteral{1})})));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses indexed assignment") {
    auto const [stmts, pos] = parseStatement({{TokenKind::Identifier, "a"},
                                              {TokenKind::BracketOpen},
                                              {TokenKind::IntLiteral, 0},
                                              {TokenKind::BracketClose},
                                              {TokenKind::Equal},
                                              {TokenKind::IntLiteral, 1},
                                              {TokenKind::Semicolon}});
    CHECK(stmts ==
          makeStatements(StatementNode(
              pos[0], AssignStmt{LValue{.identifier = "a",
                                        .indices = makeVec<ExprNode>(
                                            ExprNode(pos[2], IntLiteral{0}))},
                                 ExprNode(pos[5], IntLiteral{1})})));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses double indexed assignment") {
    auto const [stmts, pos] = parseStatement({{TokenKind::Identifier, "a"},
                                              {TokenKind::BracketOpen},
                                              {TokenKind::IntLiteral, 0},
                                              {TokenKind::BracketClose},
                                              {TokenKind::BracketOpen},
                                              {TokenKind::IntLiteral, 1},
                                              {TokenKind::BracketClose},
                                              {TokenKind::Equal},
                                              {TokenKind::IntLiteral, 2},
                                              {TokenKind::Semicolon}});
    CHECK(stmts ==
          makeStatements(StatementNode(
              pos[0], AssignStmt{LValue{.identifier = "a",
                                        .indices = makeVec<ExprNode>(
                                            ExprNode(pos[2], IntLiteral{0}),
                                            ExprNode(pos[5], IntLiteral{1}))},
                                 ExprNode(pos[8], IntLiteral{2})})));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses function call statement") {
    auto const [stmts, pos] = parseStatement({{TokenKind::Identifier, "fun"},
                                              {TokenKind::ParenOpen},
                                              {TokenKind::ParenClose},
                                              {TokenKind::Semicolon}});
    CHECK(stmts == makeStatements(StatementNode(
                       pos[0], FunctionCall{.identifier = "fun", .args = {}})));
}

TEST_CASE_FIXTURE(ParserFixture,
                  "Parser parses function call statement with arguments") {
    auto const [stmts, pos] = parseStatement({{TokenKind::Identifier, "fun"},
                                              {TokenKind::ParenOpen},
                                              {TokenKind::Identifier, "a"},
                                              {TokenKind::Comma},
                                              {TokenKind::Identifier, "b"},
                                              {TokenKind::ParenClose},
                                              {TokenKind::Semicolon}});
    CHECK(stmts ==
          makeStatements(StatementNode(
              pos[0],
              FunctionCall{
                  .identifier = "fun",
                  .args = makeVec<ExprNode>(
                      ExprNode(pos[2], Identifier{.identifier = "a"}),
                      ExprNode(pos[4], Identifier{.identifier = "b"}))})));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses break statement") {
    auto const [stmts, pos] =
        parseStatement({{TokenKind::Break}, {TokenKind::Semicolon}});
    CHECK(stmts == makeStatements(StatementNode(pos[0], BreakStmt{})));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses continue statement") {
    auto const [stmts, pos] =
        parseStatement({{TokenKind::Continue}, {TokenKind::Semicolon}});
    CHECK(stmts == makeStatements(StatementNode(pos[0], ContinueStmt{})));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses empty return statement") {
    auto const [stmts, pos] =
        parseStatement({{TokenKind::Return}, {TokenKind::Semicolon}});
    CHECK(stmts == makeStatements(StatementNode(
                       pos[0], ReturnStmt{.value = std::nullopt})));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses return statement with value") {
    auto const [stmts, pos] = parseStatement({{TokenKind::Return},
                                              {TokenKind::IntLiteral, 1},
                                              {TokenKind::Semicolon}});
    CHECK(stmts ==
          makeStatements(StatementNode(
              pos[0], ReturnStmt{.value = ExprNode(pos[1], IntLiteral{1})})));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses nested blocks") {
    auto const [stmts, pos] = parseStatement({{TokenKind::BraceOpen},
                                              {TokenKind::BraceOpen},
                                              {TokenKind::Break},
                                              {TokenKind::Semicolon},
                                              {TokenKind::BraceClose},
                                              {TokenKind::BraceClose}});

    CHECK(
        stmts ==
        makeStatements(StatementNode(
            pos[0],
            Block{.statements = makeStatements(StatementNode(
                      pos[1], Block{.statements = makeStatements(StatementNode(
                                        pos[2], BreakStmt{}))}))})));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses simple if statement") {
    auto const [stmts, pos] = parseStatement({{TokenKind::If},
                                              {TokenKind::True},
                                              {TokenKind::BraceOpen},
                                              {TokenKind::BraceClose}});
    CHECK(stmts ==
          makeStatements(StatementNode(
              pos[0], IfStmt(ExprNode(pos[1], BoolLiteral{true}),
                             BlockNode(pos[2], Block{}), {}, std::nullopt))));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses if else statement") {
    auto const [stmts, pos] = parseStatement({{TokenKind::If},
                                              {TokenKind::True},
                                              {TokenKind::BraceOpen},
                                              {TokenKind::BraceClose},
                                              {TokenKind::Else},
                                              {TokenKind::BraceOpen},
                                              {TokenKind::BraceClose}});
    CHECK(stmts == makeStatements(StatementNode(
                       pos[0], IfStmt(ExprNode(pos[1], BoolLiteral{true}),
                                      BlockNode(pos[2], Block{}), {},
                                      BlockNode(pos[5], Block{})))));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses if with multiple else ifs") {
    auto const [stmts, pos] = parseStatement({{TokenKind::If},
                                              {TokenKind::True},
                                              {TokenKind::BraceOpen},
                                              {TokenKind::BraceClose},
                                              {TokenKind::Else},
                                              {TokenKind::If},
                                              {TokenKind::False},
                                              {TokenKind::BraceOpen},
                                              {TokenKind::BraceClose},
                                              {TokenKind::Else},
                                              {TokenKind::If},
                                              {TokenKind::True},
                                              {TokenKind::BraceOpen},
                                              {TokenKind::BraceClose}});
    CHECK(stmts ==
          makeStatements(StatementNode(
              pos[0], IfStmt(ExprNode(pos[1], BoolLiteral{true}),
                             BlockNode(pos[2], Block{}),
                             makeVec<std::pair<ExprNode, BlockNode>>(
                                 std::pair{ExprNode(pos[6], BoolLiteral{false}),
                                           BlockNode(pos[7], Block{})},
                                 std::pair{ExprNode(pos[11], BoolLiteral{true}),
                                           BlockNode(pos[12], Block{})}),
                             std::nullopt))));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses if with else ifs and else") {
    auto const [stmts, pos] = parseStatement({{TokenKind::If},
                                              {TokenKind::True},
                                              {TokenKind::BraceOpen},
                                              {TokenKind::BraceClose},
                                              {TokenKind::Else},
                                              {TokenKind::If},
                                              {TokenKind::False},
                                              {TokenKind::BraceOpen},
                                              {TokenKind::BraceClose},
                                              {TokenKind::Else},
                                              {TokenKind::BraceOpen},
                                              {TokenKind::BraceClose}});
    CHECK(stmts ==
          makeStatements(StatementNode(
              pos[0], IfStmt(ExprNode(pos[1], BoolLiteral{true}),
                             BlockNode(pos[2], Block{}),
                             makeVec<std::pair<ExprNode, BlockNode>>(
                                 std::pair{ExprNode(pos[6], BoolLiteral{false}),
                                           BlockNode(pos[7], Block{})}),
                             BlockNode(pos[10], Block{})))));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses while loop") {
    auto const [stmts, pos] = parseStatement({{TokenKind::While},
                                              {TokenKind::True},
                                              {TokenKind::BraceOpen},
                                              {TokenKind::BraceClose}});
    CHECK(
        stmts ==
        makeStatements(StatementNode(
            pos[0], WhileLoop{.condition = ExprNode(pos[1], BoolLiteral{true}),
                              .block = BlockNode(pos[2], Block{})})));
}

TEST_CASE_FIXTURE(ParserFixture,
                  "Parser parses for loop with inclusive range") {
    auto const [stmts, pos] = parseStatement({{TokenKind::For},
                                              {TokenKind::Identifier, "i"},
                                              {TokenKind::In},
                                              {TokenKind::IntLiteral, 0},
                                              {TokenKind::DotDotEq},
                                              {TokenKind::IntLiteral, 2},
                                              {TokenKind::BraceOpen},
                                              {TokenKind::BraceClose}});

    CHECK(stmts ==
          makeStatements(StatementNode(
              pos[0], ForLoop{.loop_var = "i",
                              .mut = false,
                              .range = RangeNode(
                                  pos[3],
                                  Range{
                                      .start = ExprNode(pos[3], IntLiteral{0}),
                                      .inclusive = true,
                                      .end = ExprNode(pos[5], IntLiteral{2}),
                                  }),
                              .block = BlockNode(pos[6], Block{})})));
}

TEST_CASE_FIXTURE(ParserFixture,
                  "Parser parses for loop with non inclusive range") {
    auto const [stmts, pos] = parseStatement({{TokenKind::For},
                                              {TokenKind::Identifier, "i"},
                                              {TokenKind::In},
                                              {TokenKind::IntLiteral, 0},
                                              {TokenKind::DotDot},
                                              {TokenKind::IntLiteral, 2},
                                              {TokenKind::BraceOpen},
                                              {TokenKind::BraceClose}});

    CHECK(stmts ==
          makeStatements(StatementNode(
              pos[0], ForLoop{.loop_var = "i",
                              .mut = false,
                              .range = RangeNode(
                                  pos[3],
                                  Range{
                                      .start = ExprNode(pos[3], IntLiteral{0}),
                                      .inclusive = false,
                                      .end = ExprNode(pos[5], IntLiteral{2}),
                                  }),
                              .block = BlockNode(pos[6], Block{})})));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses for loop with mut loop var") {
    auto const [stmts, pos] = parseStatement({{TokenKind::For},
                                              {TokenKind::Mut},
                                              {TokenKind::Identifier, "i"},
                                              {TokenKind::In},
                                              {TokenKind::IntLiteral, 0},
                                              {TokenKind::DotDot},
                                              {TokenKind::IntLiteral, 2},
                                              {TokenKind::BraceOpen},
                                              {TokenKind::BraceClose}});

    CHECK(stmts ==
          makeStatements(StatementNode(
              pos[0], ForLoop{.loop_var = "i",
                              .mut = true,
                              .range = RangeNode(
                                  pos[4],
                                  Range{
                                      .start = ExprNode(pos[4], IntLiteral{0}),
                                      .inclusive = false,
                                      .end = ExprNode(pos[6], IntLiteral{2}),
                                  }),
                              .block = BlockNode(pos[7], Block{})})));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses for loop with step") {
    auto const [stmts, pos] = parseStatement({{TokenKind::For},
                                              {TokenKind::Identifier, "i"},
                                              {TokenKind::In},
                                              {TokenKind::IntLiteral, 0},
                                              {TokenKind::DotDot},
                                              {TokenKind::IntLiteral, 2},
                                              {TokenKind::DotDot},
                                              {TokenKind::IntLiteral, 6},
                                              {TokenKind::BraceOpen},
                                              {TokenKind::BraceClose}});

    CHECK(stmts ==
          makeStatements(StatementNode(
              pos[0], ForLoop{.loop_var = "i",
                              .mut = false,
                              .range = RangeNode(
                                  pos[3],
                                  Range{
                                      .start = ExprNode(pos[3], IntLiteral{0}),
                                      .inclusive = false,
                                      .end = ExprNode(pos[5], IntLiteral{2}),
                                      .step = ExprNode(pos[7], IntLiteral{6}),

                                  }),
                              .block = BlockNode(pos[8], Block{})})));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses simple indexing expression") {
    auto const [expr, pos] = parseExpression({{TokenKind::Identifier, "a"},
                                              {TokenKind::BracketOpen},
                                              {TokenKind::IntLiteral, 1},
                                              {TokenKind::BracketClose}});
    CHECK(expr ==
          ExprNode(pos[0], IndexExpr{.object = std::make_unique<ExprNode>(
                                         pos[0], Identifier{.identifier = "a"}),
                                     .index = std::make_unique<ExprNode>(
                                         pos[2], IntLiteral{.value = 1})}));
}

TEST_CASE_FIXTURE(ParserFixture,
                  "Parser parses indexing expression with multiple indexes") {
    auto const [expr, pos] = parseExpression({{TokenKind::Identifier, "a"},
                                              {TokenKind::BracketOpen},
                                              {TokenKind::IntLiteral, 1},
                                              {TokenKind::BracketClose},
                                              {TokenKind::BracketOpen},
                                              {TokenKind::IntLiteral, 2},
                                              {TokenKind::BracketClose}});
    CHECK(expr ==
          ExprNode(pos[0],
                   IndexExpr{
                       .object = std::make_unique<ExprNode>(
                           pos[0],
                           IndexExpr{.object = std::make_unique<ExprNode>(
                                         pos[0], Identifier{.identifier = "a"}),
                                     .index = std::make_unique<ExprNode>(
                                         pos[2], IntLiteral{.value = 1})}),
                       .index = std::make_unique<ExprNode>(
                           pos[5], IntLiteral{.value = 2})}));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses identifier in expression") {
    auto const [expr, pos] = parseExpression({{TokenKind::Identifier, "a"}});

    CHECK(expr == ExprNode(pos[0], Identifier{.identifier = "a"}));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses function call in expression") {
    auto const [expr, pos] = parseExpression({{TokenKind::Identifier, "a"},
                                              {TokenKind::ParenOpen},
                                              {TokenKind::ParenClose}});

    CHECK(expr ==
          ExprNode(pos[0], FunctionCall{.identifier = "a", .args = {}}));
}

TEST_CASE_FIXTURE(ParserFixture,
                  "Parser parses function call with params in expression") {
    auto const [expr, pos] = parseExpression({{TokenKind::Identifier, "a"},
                                              {TokenKind::ParenOpen},
                                              {TokenKind::Identifier, "param1"},
                                              {TokenKind::Comma},
                                              {TokenKind::Identifier, "param2"},
                                              {TokenKind::ParenClose}});
    CHECK(expr ==
          ExprNode(pos[0],
                   FunctionCall{.identifier = "a",
                                .args = makeVec<ExprNode>(
                                    ExprNode(pos[2], Identifier{"param1"}),
                                    ExprNode(pos[4], Identifier{"param2"}))}));
}

TEST_CASE_FIXTURE(ParserFixture,
                  "Parser parses expression in parenthesis - (expression)") {
    auto const [expr, pos] = parseExpression({{TokenKind::ParenOpen},
                                              {TokenKind::IntLiteral, 1},
                                              {TokenKind::ParenClose}});

    CHECK(expr == ExprNode(pos[1], IntLiteral{1}));
}

TEST_CASE_FIXTURE(
    ParserFixture,
    "Parser parses expression in multiple parenthesis - (expression)") {
    auto const [expr, pos] = parseExpression({{TokenKind::ParenOpen},
                                              {TokenKind::ParenOpen},
                                              {TokenKind::IntLiteral, 1},
                                              {TokenKind::ParenClose},
                                              {TokenKind::ParenClose}});

    CHECK(expr == ExprNode(pos[2], IntLiteral{1}));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses negation expression") {
    auto const [expr, pos] =
        parseExpression({{TokenKind::Minus}, {TokenKind::IntLiteral, 1}});
    CHECK(expr ==
          ExprNode(pos[0], NegExpr(ExprNode(pos[1], IntLiteral{.value = 1}))));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses negation expression") {
    auto const [expr, pos] =
        parseExpression({{TokenKind::At}, {TokenKind::IntLiteral, 1}});
    CHECK(expr == ExprNode(pos[0], LengthExpr(ExprNode(
                                       pos[1], IntLiteral{.value = 1}))));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses negation expression") {
    auto const [expr, pos] =
        parseExpression({{TokenKind::Exclamation}, {TokenKind::IntLiteral, 1}});
    CHECK(expr ==
          ExprNode(pos[0], NotExpr(ExprNode(pos[1], IntLiteral{.value = 1}))));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses multiplication expression") {
    auto const [expr, pos] = parseExpression({{TokenKind::IntLiteral, 2},
                                              {TokenKind::Asterisk},
                                              {TokenKind::IntLiteral, 3}});
    CHECK(expr ==
          ExprNode(pos[1], MulExpr(ExprNode(pos[0], IntLiteral{.value = 2}),
                                   ExprNode(pos[2], IntLiteral{.value = 3}))));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses division expression") {
    auto const [expr, pos] = parseExpression({{TokenKind::IntLiteral, 6},
                                              {TokenKind::Slash},
                                              {TokenKind::IntLiteral, 2}});
    CHECK(expr ==
          ExprNode(pos[1], DivExpr(ExprNode(pos[0], IntLiteral{.value = 6}),
                                   ExprNode(pos[2], IntLiteral{.value = 2}))));
}

TEST_CASE_FIXTURE(ParserFixture,
                  "Parser parses multiplicative left associativity") {
    auto const [expr, pos] = parseExpression({{TokenKind::IntLiteral, 2},
                                              {TokenKind::Asterisk},
                                              {TokenKind::IntLiteral, 3},
                                              {TokenKind::Asterisk},
                                              {TokenKind::IntLiteral, 4}});
    CHECK(
        expr ==
        ExprNode(
            pos[3],
            MulExpr(ExprNode(pos[1],
                             MulExpr(ExprNode(pos[0], IntLiteral{.value = 2}),
                                     ExprNode(pos[2], IntLiteral{.value = 3}))),
                    ExprNode(pos[4], IntLiteral{.value = 4}))));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses addition") {
    auto const [expr, pos] = parseExpression({{TokenKind::IntLiteral, 1},
                                              {TokenKind::Plus},
                                              {TokenKind::IntLiteral, 2}});
    CHECK(expr == ExprNode(pos[1], AddExpr(ExprNode(pos[0], IntLiteral{1}),
                                           ExprNode(pos[2], IntLiteral{2}))));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses subtraction") {
    auto const [expr, pos] = parseExpression({{TokenKind::IntLiteral, 1},
                                              {TokenKind::Minus},
                                              {TokenKind::IntLiteral, 2}});
    CHECK(expr == ExprNode(pos[1], SubExpr(ExprNode(pos[0], IntLiteral{1}),
                                           ExprNode(pos[2], IntLiteral{2}))));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses additive left associativity") {
    auto const [expr, pos] = parseExpression({{TokenKind::IntLiteral, 1},
                                              {TokenKind::Plus},
                                              {TokenKind::IntLiteral, 2},
                                              {TokenKind::Plus},
                                              {TokenKind::IntLiteral, 3}});
    CHECK(expr ==
          ExprNode(pos[3],
                   AddExpr(ExprNode(pos[1],
                                    AddExpr(ExprNode(pos[0], IntLiteral{1}),
                                            ExprNode(pos[2], IntLiteral{2}))),
                           ExprNode(pos[4], IntLiteral{3}))));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses intersection") {
    auto const [expr, pos] = parseExpression({{TokenKind::Identifier, "a"},
                                              {TokenKind::GreaterLess},
                                              {TokenKind::Identifier, "b"}});
    CHECK(expr ==
          ExprNode(pos[1], IntersectExpr(ExprNode(pos[0], Identifier{"a"}),
                                         ExprNode(pos[2], Identifier{"b"}))));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses in expression") {
    auto const [expr, pos] = parseExpression({{TokenKind::Identifier, "a"},
                                              {TokenKind::In},
                                              {TokenKind::Identifier, "b"}});
    CHECK(expr == ExprNode(pos[1], InExpr(ExprNode(pos[0], Identifier{"a"}),
                                          ExprNode(pos[2], Identifier{"b"}))));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses equal") {
    auto const [expr, pos] = parseExpression({{TokenKind::IntLiteral, 1},
                                              {TokenKind::EqualEqual},
                                              {TokenKind::IntLiteral, 1}});
    CHECK(expr == ExprNode(pos[1], EqExpr(ExprNode(pos[0], IntLiteral{1}),
                                          ExprNode(pos[2], IntLiteral{1}))));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses not equal") {
    auto const [expr, pos] = parseExpression({{TokenKind::IntLiteral, 1},
                                              {TokenKind::ExclamationEqual},
                                              {TokenKind::IntLiteral, 2}});
    CHECK(expr == ExprNode(pos[1], NeqExpr(ExprNode(pos[0], IntLiteral{1}),
                                           ExprNode(pos[2], IntLiteral{2}))));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses less than") {
    auto const [expr, pos] = parseExpression({{TokenKind::IntLiteral, 1},
                                              {TokenKind::Less},
                                              {TokenKind::IntLiteral, 2}});
    CHECK(expr == ExprNode(pos[1], LtExpr(ExprNode(pos[0], IntLiteral{1}),
                                          ExprNode(pos[2], IntLiteral{2}))));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses greater than") {
    auto const [expr, pos] = parseExpression({{TokenKind::IntLiteral, 2},
                                              {TokenKind::Greater},
                                              {TokenKind::IntLiteral, 1}});
    CHECK(expr == ExprNode(pos[1], GtExpr(ExprNode(pos[0], IntLiteral{2}),
                                          ExprNode(pos[2], IntLiteral{1}))));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses less than or equal") {
    auto const [expr, pos] = parseExpression({{TokenKind::IntLiteral, 1},
                                              {TokenKind::LessEqual},
                                              {TokenKind::IntLiteral, 2}});
    CHECK(expr == ExprNode(pos[1], LeExpr(ExprNode(pos[0], IntLiteral{1}),
                                          ExprNode(pos[2], IntLiteral{2}))));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses greater than or equal") {
    auto const [expr, pos] = parseExpression({{TokenKind::IntLiteral, 2},
                                              {TokenKind::GreaterEqual},
                                              {TokenKind::IntLiteral, 1}});
    CHECK(expr == ExprNode(pos[1], GeExpr(ExprNode(pos[0], IntLiteral{2}),
                                          ExprNode(pos[2], IntLiteral{1}))));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses and") {
    auto const [expr, pos] = parseExpression(
        {{TokenKind::True}, {TokenKind::And}, {TokenKind::False}});
    CHECK(expr ==
          ExprNode(pos[1], AndExpr(ExprNode(pos[0], BoolLiteral{true}),
                                   ExprNode(pos[2], BoolLiteral{false}))));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses or") {
    auto const [expr, pos] = parseExpression(
        {{TokenKind::True}, {TokenKind::Or}, {TokenKind::False}});
    CHECK(expr ==
          ExprNode(pos[1], OrExpr(ExprNode(pos[0], BoolLiteral{true}),
                                  ExprNode(pos[2], BoolLiteral{false}))));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses filter expression") {
    auto const [expr, pos] = parseExpression({{TokenKind::Identifier, "a"},
                                              {TokenKind::Question},
                                              {TokenKind::Identifier, "b"}});
    CHECK(expr ==
          ExprNode(pos[1], FilterExpr(ExprNode(pos[0], Identifier{"a"}),
                                      ExprNode(pos[2], Identifier{"b"}))));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses map expression") {
    auto const [expr, pos] = parseExpression({{TokenKind::Identifier, "a"},
                                              {TokenKind::ColonGreater},
                                              {TokenKind::Identifier, "b"}});
    CHECK(expr == ExprNode(pos[1], MapExpr(ExprNode(pos[0], Identifier{"a"}),
                                           ExprNode(pos[2], Identifier{"b"}))));
}
