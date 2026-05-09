
#include <optional>
#include <ranges>
#include <sstream>
#include <utility>
#include <variant>
#include <vector>

#include "doctest.h"
#include "src/Ast.h"
#include "src/CharReader.h"
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

        tokens.append_range(std::to_array<MockToken>({{TokenKind::BraceOpen},
                                                      {TokenKind::BraceClose},
                                                      {TokenKind::ETX}}));

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

        tokens.append_range(std::to_array<MockToken>(
            {{TokenKind::BraceClose}, {TokenKind::ETX}}));

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

        tokens.append_range(std::to_array<MockToken>({{TokenKind::Semicolon},
                                                      {TokenKind::BraceClose},
                                                      {TokenKind::ETX}}));

        auto const all_pos = init(std::move(tokens));

        auto program = parse();
        REQUIRE(program.has_value());

        auto& prog = **program;
        REQUIRE(prog.functions.contains("f"));

        auto& func = prog.functions.at("f");

        REQUIRE(!func->block->statements.empty());

        auto const& stmt = func->block->statements.front();
        REQUIRE(std::holds_alternative<LetBinding>(*stmt));

        auto const& let = std::get<LetBinding>(*stmt);

        return {let.value, all_pos.subspan(offset)};
    }

    static constexpr void assertTokensMatch(
        std::string const& src, std::vector<MockToken> const& tokens) {
        std::istringstream stream{src};
        Lexer lexer{stream};
        lexer.next();
        for (auto const& mock : tokens) {
            auto token = lexer.current();
            REQUIRE(token.kind() == mock.kind_);
            REQUIRE(token.value() == mock.value_);
            lexer.next();
        }
    }

    template <typename T>
    Node<T> testNode(T value) {
        return Node<T>{Position{}, std::move(value)};
    }

    std::optional<ProgramNode> parse() { return parser_->parse(); }
    auto const& diagnostics() { return parser_->diagnostics(); }

private:
    std::optional<MockParser> parser_;
    std::vector<Position> positions_;
};

TEST_CASE_FIXTURE(ParserFixture, "Parser parses empty function") {
    auto const pos =
        initValidated("fn main() {}", {
                                          {TokenKind::Fn},
                                          {TokenKind::Identifier, "main"},
                                          {TokenKind::ParenOpen},
                                          {TokenKind::ParenClose},
                                          {TokenKind::BraceOpen},
                                          {TokenKind::BraceClose},
                                          {TokenKind::ETX},
                                      });

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
                                    {TokenKind::BraceClose},
                                    {TokenKind::ETX}});

    auto const program = parse();

    REQUIRE(!program.has_value());
    REQUIRE(!diagnostics().empty());

    REQUIRE(diagnostics().last().kind ==
            ParserDiagnosticKind{FunctionRedefined{.identifier = "main",
                                                   .original_pos = pos[0]}});
    REQUIRE(diagnostics().last().pos == pos[6]);
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
                       {TokenKind::BraceClose},
                       {TokenKind::ETX}});

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

TEST_CASE_FIXTURE(ParserFixture, "Parser parses return type") {
    auto const pos =
        initValidated("fn main() -> int {}", {{TokenKind::Fn},
                                              {TokenKind::Identifier, "main"},
                                              {TokenKind::ParenOpen},
                                              {TokenKind::ParenClose},
                                              {TokenKind::MinusGreater},
                                              {TokenKind::Int},
                                              {TokenKind::BraceOpen},
                                              {TokenKind::BraceClose},
                                              {TokenKind::ETX}});

    auto const program = parse();

    REQUIRE(program.has_value());
    REQUIRE(program.value()->functions.contains("main"));

    auto const& return_type =
        program.value()->functions.at("main")->return_type;

    CHECK(return_type == TypeNode(pos[5], PrimitiveType::Int));
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

    CHECK(vec_val == ExprNode(pos[0], VecLiteral{
                                          .elements = {ExprNode(
                                              pos[1], IntLiteral{.value = 1})},
                                      }));
}

TEST_CASE_FIXTURE(ParserFixture,
                  "Parser parses simple vec with trailing comma") {
    auto const [vec_val, pos] = parseExpression({{TokenKind::BracketOpen},
                                                 {TokenKind::IntLiteral, 1},
                                                 {TokenKind::Comma},
                                                 {TokenKind::BracketClose}});

    CHECK(vec_val == ExprNode(pos[0], VecLiteral{
                                          .elements = {ExprNode(
                                              pos[1], IntLiteral{.value = 1})},
                                      }));
}

TEST_CASE_FIXTURE(ParserFixture,
                  "Parser parses simple vec literal with multiple elements") {
    auto const [vec_val, pos] = parseExpression({{TokenKind::BracketOpen},
                                                 {TokenKind::IntLiteral, 1},
                                                 {TokenKind::Comma},
                                                 {TokenKind::IntLiteral, 2},
                                                 {TokenKind::BracketClose}});
    CHECK(
        vec_val ==
        ExprNode(pos[0],
                 VecLiteral{.elements = {
                                {ExprNode(pos[1], IntLiteral{.value = 1})},
                                {ExprNode(pos[3], IntLiteral{.value = 2})}}}));
}

TEST_CASE_FIXTURE(ParserFixture, "Parser parses vec literal with nested vecs") {
    auto const [vec_val, pos] = parseExpression({{TokenKind::BracketOpen},
                                                 {TokenKind::BracketOpen},
                                                 {TokenKind::IntLiteral, 1},
                                                 {TokenKind::BracketClose},
                                                 {TokenKind::BracketClose}});

    auto const expected = ExprNode(
        pos[0],
        VecLiteral{
            .elements = {ExprNode(
                pos[1], VecLiteral{.elements = {ExprNode(
                                       pos[2], IntLiteral{.value = 1})}})}});
    CHECK(vec_val == expected);
};

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
          ExprNode(
              pos[0],
              FunctionCall{.identifier = "a",
                           .args = {ExprNode(pos[2], Identifier{"param1"}),
                                    ExprNode(pos[4], Identifier{"param2"})}}));
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
