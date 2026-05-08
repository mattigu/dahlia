
#include <ranges>
#include <sstream>
#include <utility>
#include <variant>
#include <vector>

#include "doctest.h"
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
        tokens.emplace_back(TokenKind::Fn);
        tokens.emplace_back(TokenKind::Identifier, std::string{"f"});
        tokens.emplace_back(TokenKind::ParenOpen);
        tokens.emplace_back(TokenKind::ParenClose);
        tokens.emplace_back(TokenKind::MinusGreater);
        auto const offset = tokens.size();
        tokens.append_range(type_tokens);
        tokens.emplace_back(TokenKind::BraceOpen);
        tokens.emplace_back(TokenKind::BraceClose);
        tokens.emplace_back(TokenKind::ETX);
        auto const all_pos = init(std::move(tokens));
        auto program = parse();
        REQUIRE(program.has_value());
        auto& prog = **program;
        REQUIRE(prog.functions.contains("f"));
        auto& ret = prog.functions.at("f")->return_type;
        REQUIRE(ret.has_value());
        return {std::move(*ret), all_pos.subspan(offset)};
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
    auto const pos = initValidated(
        "fn main() {}", {
                            {TokenKind::Fn},
                            {TokenKind::Identifier, std::string{"main"}},
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
    auto const pos =
        initValidated("fn main() {} fn main() {}",
                      {{TokenKind::Fn},
                       {TokenKind::Identifier, std::string{"main"}},
                       {TokenKind::ParenOpen},
                       {TokenKind::ParenClose},
                       {TokenKind::BraceOpen},
                       {TokenKind::BraceClose},
                       {TokenKind::Fn},
                       {TokenKind::Identifier, std::string{"main"}},
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
    auto const pos = initValidated(
        "fn main() -> int {}", {{TokenKind::Fn},
                                {TokenKind::Identifier, std::string{"main"}},
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
