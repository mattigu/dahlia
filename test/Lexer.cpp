#include "src/Lexer.h"

#include <sstream>
#include <string>

#include "doctest.h"
#include "src/Position.h"
#include "src/Token.h"

struct LexerFixture {
    void init(std::string const& src) {
        stream.emplace(src);
        lexer.emplace(*stream);
    }

    Token next() { return lexer->next(); }
    Token current() { return lexer->current(); }

    std::optional<std::istringstream> stream;
    std::optional<Lexer> lexer;
};

TEST_CASE("Lexer first token is STX") {
    std::istringstream input("1");
    Lexer lexer(input);
    CHECK(lexer.current().kind == TokenKind::STX);
}

TEST_CASE("Lexer last token is ETX") {
    std::istringstream input("");
    Lexer lexer(input);
    CHECK(lexer.next().kind == TokenKind::ETX);
}

TEST_CASE("Lexer next() when depleted") {
    std::istringstream input("");
    Lexer lexer(input);
    auto const end_token =
        Token{.kind = TokenKind::ETX,
              .pos = Position{.line = 1, .column = 1, .offset = 0}};
    REQUIRE(lexer.next() == end_token);
    REQUIRE(lexer.next() == end_token);
    REQUIRE(lexer.next() == end_token);
}

TEST_CASE_FIXTURE(LexerFixture, "Lexer tokenizes single char tokens") {
    init("()[]{};,@?");
    CHECK(next().kind == TokenKind::ParenOpen);
    CHECK(next().kind == TokenKind::ParenClose);
    CHECK(next().kind == TokenKind::BracketOpen);
    CHECK(next().kind == TokenKind::BracketClose);
    CHECK(next().kind == TokenKind::BraceOpen);
    CHECK(next().kind == TokenKind::BraceClose);
    CHECK(next().kind == TokenKind::Semicolon);
    CHECK(next().kind == TokenKind::Comma);
    CHECK(next().kind == TokenKind::At);
    CHECK(next().kind == TokenKind::Question);
    CHECK(next().kind == TokenKind::ETX);
}

TEST_CASE_FIXTURE(LexerFixture, "Lexer token positions single char") {
    init("()\n[");
    REQUIRE(next().pos == Position{.line = 1, .column = 1, .offset = 0});
    REQUIRE(next().pos == Position{.line = 1, .column = 2, .offset = 1});
    REQUIRE(next().pos == Position{.line = 2, .column = 1, .offset = 3});
    REQUIRE(next().pos == Position{.line = 2, .column = 2, .offset = 4});
}

TEST_CASE_FIXTURE(LexerFixture, "Lexer tokenizes simple strings") {
    init(R"("hello")");
    auto token = next();
    CHECK(token.kind == TokenKind::StrLiteral);
    CHECK(token.value == TokenValue("hello"));
}

TEST_CASE_FIXTURE(LexerFixture,
                  "Lexer tokenizes strings with escape sequences") {
    init(R"(" 1 \t 2 \n 3 \" 4 \\ 5 ")");
    auto token = next();
    CHECK(token.kind == TokenKind::StrLiteral);
    CHECK(token.value == TokenValue(" 1 \t 2 \n 3 \" 4 \\ 5 "));
}

TEST_CASE_FIXTURE(LexerFixture,
                  "Lexer tokenizes strings with hex escape sequences") {
    init(R"("\x48\x65\x6C\x6C\x6F")");
    auto token = next();
    CHECK(token.kind == TokenKind::StrLiteral);
    CHECK(token.value == TokenValue("Hello"));
}
