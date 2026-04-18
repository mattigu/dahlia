#include "src/Lexer.h"

#include <sstream>

#include "doctest.h"
#include "src/Position.h"
#include "src/Token.h"

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

TEST_CASE("Lexer tokenizes single char tokens") {
    std::istringstream input("()[]{}:;,@?");
    Lexer lexer(input);

    CHECK(lexer.next().kind == TokenKind::ParenOpen);
    CHECK(lexer.next().kind == TokenKind::ParenClose);
    CHECK(lexer.next().kind == TokenKind::BracketOpen);
    CHECK(lexer.next().kind == TokenKind::BracketClose);
    CHECK(lexer.next().kind == TokenKind::BraceOpen);
    CHECK(lexer.next().kind == TokenKind::BraceClose);
    CHECK(lexer.next().kind == TokenKind::Colon);
    CHECK(lexer.next().kind == TokenKind::Semicolon);
    CHECK(lexer.next().kind == TokenKind::Comma);
    CHECK(lexer.next().kind == TokenKind::At);
    CHECK(lexer.next().kind == TokenKind::Question);
    CHECK(lexer.next().kind == TokenKind::ETX);
}

TEST_CASE("Lexer token positions(single char)") {
    std::istringstream input("()\n[");
    Lexer lexer(input);

    REQUIRE(lexer.next().pos == Position{.line = 1, .column = 1, .offset = 0});
    REQUIRE(lexer.next().pos == Position{.line = 1, .column = 2, .offset = 1});
    REQUIRE(lexer.next().pos == Position{.line = 2, .column = 1, .offset = 3});
    REQUIRE(lexer.next().pos == Position{.line = 2, .column = 2, .offset = 4});
}
