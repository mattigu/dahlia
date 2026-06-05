#include <sstream>
#include <string>

#include "dahlia_lib/Diagnostics.hpp"
#include "dahlia_lib/Lexer.h"
#include "dahlia_lib/LexerDiagnostics.h"
#include "dahlia_lib/Position.h"
#include "dahlia_lib/Token.h"
#include "doctest.h"

struct LexerFixture {
    void init(std::string const& src, LexerOptions const& options = {}) {
        stream.emplace(src);
        lexer.emplace(*stream, options);
    }

    Token next() { return lexer->next(); }
    Token current() { return lexer->current(); }
    auto const& diagnostics() { return lexer->diagnostics(); }

    std::optional<std::istringstream> stream;
    std::optional<Lexer> lexer;
    static std::string makeNumber(std::size_t int_digits,
                                  std::size_t frac_digits = 0,
                                  char digit = '9');

    static std::vector<LexerDiagnostic> makeDiags(
        std::initializer_list<LexerDiagnostic> diags) {
        return diags;
    }
};

inline std::string subcaseName(char const* src, TokenKind kind) {
    return std::string(src) + " -> " + toString(kind);
}

std::string LexerFixture::makeNumber(std::size_t int_digits,
                                     std::size_t frac_digits, char digit) {
    std::string number;
    number.reserve(int_digits + ((frac_digits != 0U) ? frac_digits + 1 : 0));

    if (int_digits == 0) {
        number.push_back('0');
    } else {
        number.append(int_digits, digit);
    }

    if (frac_digits > 0) {
        number.push_back('.');
        number.append(frac_digits, digit);
    }

    return number;
}

TEST_CASE("Lexer first token is STX") {
    std::istringstream input("1");
    Lexer lexer(input);

    CHECK(
        lexer.current() ==
        Token{TokenKind::STX, Position{.line = 1, .column = 0, .offset = -1}});
}

TEST_CASE("Lexer last token is ETX") {
    std::istringstream input("");
    Lexer lexer(input);

    CHECK(lexer.next() ==
          Token{TokenKind::ETX, Position{.line = 1, .column = 1, .offset = 0}});
}

TEST_CASE("Lexer next() when depleted") {
    std::istringstream input("");
    Lexer lexer(input);

    auto const end_token =
        Token{TokenKind::ETX, Position{.line = 1, .column = 1, .offset = 0}};

    REQUIRE(lexer.next() == end_token);
    REQUIRE(lexer.next() == end_token);
    REQUIRE(lexer.next() == end_token);
}

TEST_CASE_FIXTURE(LexerFixture,
                  "Lexer reports and skips unexpected characters") {
    init("let^;");

    CHECK(next() ==
          Token{TokenKind::Let, Position{.line = 1, .column = 1, .offset = 0}});

    CHECK(next() == Token{TokenKind::ERROR,
                          Position{.line = 1, .column = 4, .offset = 3}});

    CHECK(diagnostics().all() ==
          makeDiags({LexerDiagnostic{
              .kind = UnexpectedChar{.chr = '^'},
              .pos = Position{.line = 1, .column = 4, .offset = 3},
              .severity = Severity::Error}}));

    CHECK(next() == Token{TokenKind::Semicolon,
                          Position{.line = 1, .column = 5, .offset = 4}});

    CHECK(next() ==
          Token{TokenKind::ETX, Position{.line = 1, .column = 6, .offset = 5}});
}

TEST_CASE_FIXTURE(LexerFixture, "Lexer tokenizes single char tokens") {
    struct Case {
        char const* src;
        TokenKind kind;
    };

    constexpr auto cases = std::to_array<Case>({
        {.src = "(", .kind = TokenKind::ParenOpen},
        {.src = ")", .kind = TokenKind::ParenClose},
        {.src = "[", .kind = TokenKind::BracketOpen},
        {.src = "]", .kind = TokenKind::BracketClose},
        {.src = "{", .kind = TokenKind::BraceOpen},
        {.src = "}", .kind = TokenKind::BraceClose},
        {.src = ";", .kind = TokenKind::Semicolon},
        {.src = ",", .kind = TokenKind::Comma},
        {.src = "@", .kind = TokenKind::At},
        {.src = "?", .kind = TokenKind::Question},
    });

    for (auto const& test : cases) {
        SUBCASE(subcaseName(test.src, test.kind)) {
            init(test.src);
            CHECK(next() ==
                  Token{test.kind,
                        Position{.line = 1, .column = 1, .offset = 0}});
            CHECK(next() ==
                  Token{TokenKind::ETX,
                        Position{.line = 1, .column = 2, .offset = 1}});
        }
    }
}

TEST_CASE_FIXTURE(LexerFixture, "Lexer tokenizes comments") {
    init("# Hello \n# World");

    CHECK(next() == Token{TokenKind::Comment,
                          Position{.line = 1, .column = 1, .offset = 0},
                          TokenValue(" Hello ")});

    CHECK(next() == Token{TokenKind::Comment,
                          Position{.line = 2, .column = 1, .offset = 9},
                          TokenValue(" World")});

    CHECK(next() == Token{TokenKind::ETX,
                          Position{.line = 2, .column = 8, .offset = 16}});
}

TEST_CASE_FIXTURE(LexerFixture, "Lexer reports comments too long") {
    init("#1\n#12\n;", {.max_comment_len = 1});

    CHECK(next() == Token{TokenKind::Comment,
                          Position{.line = 1, .column = 1, .offset = 0},
                          TokenValue("1")});

    CHECK(next() == Token{TokenKind::Comment,
                          Position{.line = 2, .column = 1, .offset = 3},
                          TokenValue("1")});

    CHECK(diagnostics().all() ==
          makeDiags({LexerDiagnostic{
              .kind = CommentTooLong{},
              .pos = Position{.line = 2, .column = 1, .offset = 3},
              .severity = Severity::Warning}}));

    CHECK(next() == Token{TokenKind::Semicolon,
                          Position{.line = 3, .column = 1, .offset = 7}});

    CHECK(next() ==
          Token{TokenKind::ETX, Position{.line = 3, .column = 2, .offset = 8}});
}

TEST_CASE_FIXTURE(LexerFixture, "Lexer tokenizes simple operators") {
    struct Case {
        char const* src;
        TokenKind kind;
    };

    constexpr auto cases = std::to_array<Case>({
        {.src = "+", .kind = TokenKind::Plus},
        {.src = "-", .kind = TokenKind::Minus},
        {.src = "*", .kind = TokenKind::Asterisk},
        {.src = "/", .kind = TokenKind::Slash},
        {.src = "%", .kind = TokenKind::Percent},
        {.src = "=", .kind = TokenKind::Equal},
        {.src = "!", .kind = TokenKind::Exclamation},
        {.src = "<", .kind = TokenKind::Less},
        {.src = ">", .kind = TokenKind::Greater},
        {.src = ":", .kind = TokenKind::Colon},
    });

    for (auto const& test : cases) {
        SUBCASE(subcaseName(test.src, test.kind)) {
            init(test.src);
            CHECK(next() ==
                  Token{test.kind,
                        Position{.line = 1, .column = 1, .offset = 0}});
            CHECK(next().kind() == TokenKind::ETX);
        }
    }
}

TEST_CASE_FIXTURE(LexerFixture,
                  "Lexer tokenizes compound assignment operators") {
    struct Case {
        char const* src;
        TokenKind kind;
    };

    constexpr auto cases = std::to_array<Case>({
        {.src = "+=", .kind = TokenKind::PlusEqual},
        {.src = "-=", .kind = TokenKind::MinusEqual},
        {.src = "*=", .kind = TokenKind::AsteriskEqual},
        {.src = "/=", .kind = TokenKind::SlashEqual},
        {.src = "%=", .kind = TokenKind::PercentEqual},
        {.src = "==", .kind = TokenKind::EqualEqual},
        {.src = "!=", .kind = TokenKind::ExclamationEqual},
        {.src = "<=", .kind = TokenKind::LessEqual},
        {.src = ">=", .kind = TokenKind::GreaterEqual},
    });

    for (auto const& test : cases) {
        SUBCASE(subcaseName(test.src, test.kind)) {
            init(test.src);
            CHECK(next() ==
                  Token{test.kind,
                        Position{.line = 1, .column = 1, .offset = 0}});
            CHECK(next().kind() == TokenKind::ETX);
        }
    }
}

TEST_CASE_FIXTURE(LexerFixture, "Lexer tokenizes special operators") {
    struct Case {
        char const* src;
        TokenKind kind;
    };

    constexpr auto cases = std::to_array<Case>({
        {.src = "..", .kind = TokenKind::DotDot},
        {.src = "..=", .kind = TokenKind::DotDotEq},
        {.src = ":>", .kind = TokenKind::ColonGreater},
        {.src = "><", .kind = TokenKind::GreaterLess},
        {.src = "->", .kind = TokenKind::MinusGreater},
    });

    for (auto const& test : cases) {
        SUBCASE(subcaseName(test.src, test.kind)) {
            init(test.src);
            CHECK(next() ==
                  Token{test.kind,
                        Position{.line = 1, .column = 1, .offset = 0}});
            CHECK(next().kind() == TokenKind::ETX);
        }
    }
}

TEST_CASE_FIXTURE(LexerFixture, "Lexer reports 'Expected' error for ..") {
    init("for x in a.b");

    CHECK(next().kind() == TokenKind::For);
    CHECK(next().kind() == TokenKind::Identifier);
    CHECK(next().kind() == TokenKind::In);
    CHECK(next().kind() == TokenKind::Identifier);

    CHECK(next().kind() == TokenKind::ERROR);

    CHECK(diagnostics().all() ==
          makeDiags({LexerDiagnostic{
              .kind = ExpectedChar{.expected = '.', .got = 'b'},
              .pos = Position{.line = 1, .column = 12, .offset = 11},
              .severity = Severity::Error}}));

    CHECK(next().kind() == TokenKind::Identifier);
    CHECK(next().kind() == TokenKind::ETX);
}

TEST_CASE_FIXTURE(LexerFixture, "Lexer tokenizes keywords") {
    struct Case {
        char const* src;
        TokenKind expected;
    };

    constexpr auto cases = std::to_array<Case>({
        {.src = "let", .expected = TokenKind::Let},
        {.src = "mut", .expected = TokenKind::Mut},
        {.src = "fn", .expected = TokenKind::Fn},
        {.src = "and", .expected = TokenKind::And},
        {.src = "or", .expected = TokenKind::Or},
        {.src = "if", .expected = TokenKind::If},
        {.src = "else", .expected = TokenKind::Else},
        {.src = "for", .expected = TokenKind::For},
        {.src = "while", .expected = TokenKind::While},
        {.src = "in", .expected = TokenKind::In},
        {.src = "return", .expected = TokenKind::Return},
        {.src = "true", .expected = TokenKind::True},
        {.src = "break", .expected = TokenKind::Break},
        {.src = "continue", .expected = TokenKind::Continue},
        {.src = "false", .expected = TokenKind::False},
        {.src = "int", .expected = TokenKind::Int},
        {.src = "float", .expected = TokenKind::Float},
        {.src = "bool", .expected = TokenKind::Bool},
        {.src = "str", .expected = TokenKind::Str},
    });

    for (auto const& test : cases) {
        SUBCASE(subcaseName(test.src, test.expected)) {
            init(test.src);

            CHECK(next() ==
                  Token{test.expected,
                        Position{.line = 1, .column = 1, .offset = 0}});

            CHECK(next().kind() == TokenKind::ETX);
        }
    }
}

TEST_CASE_FIXTURE(LexerFixture, "Lexer tokenizes simple strings") {
    init(R"("hello")");

    CHECK(next() == Token{TokenKind::StrLiteral,
                          Position{.line = 1, .column = 1, .offset = 0},
                          TokenValue("hello")});

    CHECK(next() ==
          Token{TokenKind::ETX, Position{.line = 1, .column = 8, .offset = 7}});
}

TEST_CASE_FIXTURE(LexerFixture,
                  "Lexer tokenizes strings with escape sequences") {
    init(R"(" 1 \t 2 \n 3 \" 4 \\ 5 \r 6 ")");

    CHECK(next() == Token{TokenKind::StrLiteral,
                          Position{.line = 1, .column = 1, .offset = 0},
                          TokenValue{" 1 \t 2 \n 3 \" 4 \\ 5 \r 6 "}});

    CHECK(next() == Token{TokenKind::ETX,
                          Position{.line = 1, .column = 31, .offset = 30}});
}

TEST_CASE_FIXTURE(LexerFixture,
                  "Lexer recognizes and reports invalid escape sequences") {
    init(R"("ab\c" )");

    CHECK(next() == Token{TokenKind::ERROR,
                          Position{.line = 1, .column = 1, .offset = 0}});

    CHECK(diagnostics().all() ==
          makeDiags({LexerDiagnostic{
              .kind = InvalidEscapeSequence{.chr = 'c'},
              .pos = Position{.line = 1, .column = 5, .offset = 4},
              .severity = Severity::Error}}));

    CHECK(next() ==
          Token{TokenKind::ETX, Position{.line = 1, .column = 8, .offset = 7}});
}

TEST_CASE_FIXTURE(LexerFixture,
                  "Lexer tokenizes strings with hex escape sequences") {
    init(R"("\x48\x65\x6C\x6C\x6F")");

    CHECK(next() == Token{TokenKind::StrLiteral,
                          Position{.line = 1, .column = 1, .offset = 0},
                          TokenValue{"Hello"}});

    CHECK(next() == Token{TokenKind::ETX,
                          Position{.line = 1, .column = 23, .offset = 22}});
}

TEST_CASE_FIXTURE(LexerFixture,
                  "Lexer recognizes and reports invalid hex escape sequences") {
    init(R"("\xLc" ;)");

    CHECK(next() == Token{TokenKind::ERROR,
                          Position{.line = 1, .column = 1, .offset = 0}});

    CHECK(diagnostics().all() ==
          makeDiags({LexerDiagnostic{
              .kind = InvalidHexEscape{.chr = 'L'},
              .pos = Position{.line = 1, .column = 4, .offset = 3},
              .severity = Severity::Error}}));

    CHECK(next() == Token{TokenKind::Semicolon,
                          Position{.line = 1, .column = 8, .offset = 7}});

    CHECK(next() ==
          Token{TokenKind::ETX, Position{.line = 1, .column = 9, .offset = 8}});
}

TEST_CASE_FIXTURE(LexerFixture, "Lexer reports strings too long") {
    init(R"("a" "ab" ;)", {.max_string_len = 1});

    CHECK(next() == Token{TokenKind::StrLiteral,
                          Position{.line = 1, .column = 1, .offset = 0},
                          TokenValue{"a"}});

    CHECK(next() == Token{TokenKind::ERROR,
                          Position{.line = 1, .column = 5, .offset = 4}});

    CHECK(diagnostics().all() ==
          makeDiags({LexerDiagnostic{
              .kind = StringTooLong{},
              .pos = Position{.line = 1, .column = 5, .offset = 4},
              .severity = Severity::Error}}));

    CHECK(next() == Token{TokenKind::Semicolon,
                          Position{.line = 1, .column = 10, .offset = 9}});

    CHECK(next() == Token{TokenKind::ETX,
                          Position{.line = 1, .column = 11, .offset = 10}});
}

TEST_CASE_FIXTURE(LexerFixture, "Lexer recognizes unterminated strings") {
    init(R"("hello
;)");

    CHECK(next() == Token{TokenKind::ERROR,
                          Position{.line = 1, .column = 1, .offset = 0}});

    CHECK(diagnostics().all() ==
          makeDiags({LexerDiagnostic{
              .kind = UnterminatedString{},
              .pos = Position{.line = 1, .column = 1, .offset = 0},
              .severity = Severity::Error}}));

    CHECK(next() == Token{TokenKind::Semicolon,
                          Position{.line = 2, .column = 1, .offset = 7}});

    CHECK(next() ==
          Token{TokenKind::ETX, Position{.line = 2, .column = 2, .offset = 8}});
}

TEST_CASE_FIXTURE(LexerFixture, "Lexer tokenizes valid identifiers") {
    init("x FOO bar123 camelCase snake_case a1b2c3");

    CHECK(next() == Token{TokenKind::Identifier,
                          Position{.line = 1, .column = 1, .offset = 0},
                          TokenValue("x")});

    CHECK(next() == Token{TokenKind::Identifier,
                          Position{.line = 1, .column = 3, .offset = 2},
                          TokenValue("FOO")});

    CHECK(next() == Token{TokenKind::Identifier,
                          Position{.line = 1, .column = 7, .offset = 6},
                          TokenValue("bar123")});

    CHECK(next() == Token{TokenKind::Identifier,
                          Position{.line = 1, .column = 14, .offset = 13},
                          TokenValue("camelCase")});

    CHECK(next() == Token{TokenKind::Identifier,
                          Position{.line = 1, .column = 24, .offset = 23},
                          TokenValue("snake_case")});

    CHECK(next() == Token{TokenKind::Identifier,
                          Position{.line = 1, .column = 35, .offset = 34},
                          TokenValue("a1b2c3")});

    CHECK(next() == Token{TokenKind::ETX,
                          Position{.line = 1, .column = 41, .offset = 40}});
}

TEST_CASE_FIXTURE(LexerFixture, "Lexer reports identifiers too long") {
    init("a ab ;", {.max_identifier_len = 1});

    CHECK(next() == Token{TokenKind::Identifier,
                          Position{.line = 1, .column = 1, .offset = 0},
                          TokenValue{"a"}});

    CHECK(next() == Token{TokenKind::ERROR,
                          Position{.line = 1, .column = 3, .offset = 2}});

    CHECK(diagnostics().all() ==
          makeDiags({LexerDiagnostic{
              .kind = IdentifierTooLong{},
              .pos = Position{.line = 1, .column = 3, .offset = 2},
              .severity = Severity::Error}}));

    CHECK(next() == Token{TokenKind::Semicolon,
                          Position{.line = 1, .column = 6, .offset = 5}});

    CHECK(next() ==
          Token{TokenKind::ETX, Position{.line = 1, .column = 7, .offset = 6}});
}

TEST_CASE_FIXTURE(LexerFixture, "Lexer tokenizes simple integers") {
    init("617 5 123456789");

    CHECK(next() == Token{TokenKind::IntLiteral,
                          Position{.line = 1, .column = 1, .offset = 0},
                          TokenValue{617}});

    CHECK(next() == Token{TokenKind::IntLiteral,
                          Position{.line = 1, .column = 5, .offset = 4},
                          TokenValue{5}});

    CHECK(next() == Token{TokenKind::IntLiteral,
                          Position{.line = 1, .column = 7, .offset = 6},
                          TokenValue{123456789}});

    CHECK(next() == Token{TokenKind::ETX,
                          Position{.line = 1, .column = 16, .offset = 15}});
}

TEST_CASE_FIXTURE(LexerFixture, "Lexer tokenizes int of value 0") {
    init("0");

    CHECK(next() == Token{TokenKind::IntLiteral,
                          Position{.line = 1, .column = 1, .offset = 0},
                          TokenValue{0}});

    CHECK(next() ==
          Token{TokenKind::ETX, Position{.line = 1, .column = 2, .offset = 1}});
}

TEST_CASE_FIXTURE(LexerFixture,
                  "Lexer, leading zeros do not affect integer value") {
    init("003");

    CHECK(next() == Token{TokenKind::IntLiteral,
                          Position{.line = 1, .column = 1, .offset = 0},
                          TokenValue{3}});

    CHECK(next() ==
          Token{TokenKind::ETX, Position{.line = 1, .column = 4, .offset = 3}});
}

TEST_CASE_FIXTURE(LexerFixture,
                  "Lexer, leading zeros do not affect float value") {
    init("000.1");

    CHECK(next() == Token{TokenKind::FloatLiteral,
                          Position{.line = 1, .column = 1, .offset = 0},
                          TokenValue{0.1}});

    CHECK(next() ==
          Token{TokenKind::ETX, Position{.line = 1, .column = 6, .offset = 5}});
}

TEST_CASE_FIXTURE(LexerFixture, "Lexer detects integer overflow") {
    init("9223372036854775807 9223372036854775808");

    CHECK(next() == Token{TokenKind::IntLiteral,
                          Position{.line = 1, .column = 1, .offset = 0},
                          TokenValue{9223372036854775807LL}});

    CHECK(next() == Token{TokenKind::ERROR,
                          Position{.line = 1, .column = 21, .offset = 20}});

    CHECK(diagnostics().all() ==
          makeDiags({LexerDiagnostic{
              .kind = IntegerOverflow{},
              .pos = Position{.line = 1, .column = 21, .offset = 20},
              .severity = Severity::Error}}));

    CHECK(next() == Token{TokenKind::ETX,
                          Position{.line = 1, .column = 40, .offset = 39}});
}

TEST_CASE_FIXTURE(LexerFixture, "Lexer detects double separators") {
    init("1__2");

    CHECK(next() == Token{TokenKind::ERROR,
                          Position{.line = 1, .column = 1, .offset = 0}});

    CHECK(diagnostics().all() ==
          makeDiags({LexerDiagnostic{
              .kind = InvalidNumericSeparator{},
              .pos = Position{.line = 1, .column = 3, .offset = 2},
              .severity = Severity::Error}}));

    CHECK(next() ==
          Token{TokenKind::ETX, Position{.line = 1, .column = 5, .offset = 4}});
}

TEST_CASE_FIXTURE(LexerFixture, "Lexer detects leading separator") {
    init("_123");

    CHECK(next() == Token{TokenKind::ERROR,
                          Position{.line = 1, .column = 1, .offset = 0}});

    CHECK(diagnostics().all() ==
          makeDiags({LexerDiagnostic{
              .kind = InvalidNumericSeparator{},
              .pos = Position{.line = 1, .column = 1, .offset = 0},
              .severity = Severity::Error}}));

    CHECK(next() ==
          Token{TokenKind::ETX, Position{.line = 1, .column = 5, .offset = 4}});
}

TEST_CASE_FIXTURE(LexerFixture, "Lexer detects trailing separator") {
    init("123_");

    CHECK(next() == Token{TokenKind::ERROR,
                          Position{.line = 1, .column = 1, .offset = 0}});

    CHECK(diagnostics().all() ==
          makeDiags({LexerDiagnostic{
              .kind = InvalidNumericSeparator{},
              .pos = Position{.line = 1, .column = 4, .offset = 3},
              .severity = Severity::Error}}));

    CHECK(next() ==
          Token{TokenKind::ETX, Position{.line = 1, .column = 5, .offset = 4}});
}

TEST_CASE_FIXTURE(LexerFixture,
                  "Lexer detects separators around decimal point") {
    init("1_.2");

    CHECK(next() == Token{TokenKind::ERROR,
                          Position{.line = 1, .column = 1, .offset = 0}});

    CHECK(diagnostics().all() ==
          makeDiags({LexerDiagnostic{
              .kind = InvalidNumericSeparator{},
              .pos = Position{.line = 1, .column = 2, .offset = 1},
              .severity = Severity::Error}}));

    CHECK(next() ==
          Token{TokenKind::ETX, Position{.line = 1, .column = 5, .offset = 4}});
}

TEST_CASE_FIXTURE(LexerFixture,
                  "Lexer tokenizes floats with no fraction part") {
    init("1.");

    CHECK(next() == Token{TokenKind::FloatLiteral,
                          Position{.line = 1, .column = 1, .offset = 0},
                          TokenValue{1.0}});

    CHECK(next() ==
          Token{TokenKind::ETX, Position{.line = 1, .column = 3, .offset = 2}});
}

TEST_CASE_FIXTURE(LexerFixture, "Lexer tokenizes simple floats") {
    init("0.0 1.0");

    CHECK(next() == Token{TokenKind::FloatLiteral,
                          Position{.line = 1, .column = 1, .offset = 0},
                          TokenValue{0.0}});

    CHECK(next() == Token{TokenKind::FloatLiteral,
                          Position{.line = 1, .column = 5, .offset = 4},
                          TokenValue{1.0}});

    CHECK(next() ==
          Token{TokenKind::ETX, Position{.line = 1, .column = 8, .offset = 7}});
}

TEST_CASE_FIXTURE(LexerFixture, "Lexer parses floats with fractional values") {
    init("123.456");

    CHECK(next() == Token{TokenKind::FloatLiteral,
                          Position{.line = 1, .column = 1, .offset = 0},
                          TokenValue{123.456}});

    CHECK(next() ==
          Token{TokenKind::ETX, Position{.line = 1, .column = 8, .offset = 7}});
}

TEST_CASE_FIXTURE(LexerFixture,
                  "Lexer parses floats with zero leading values") {
    init("0.4");

    CHECK(next() == Token{TokenKind::FloatLiteral,
                          Position{.line = 1, .column = 1, .offset = 0},
                          TokenValue{0.4}});

    CHECK(next() ==
          Token{TokenKind::ETX, Position{.line = 1, .column = 4, .offset = 3}});
}

TEST_CASE_FIXTURE(LexerFixture, "Lexer parses floats with large int parts") {
    init(makeNumber(50, 10, '9'));

    CHECK(next().kind() == TokenKind::FloatLiteral);

    CHECK(next() == Token{TokenKind::ETX, Position{
                                              .line = 1,
                                              .column = 62,
                                              .offset = 61,
                                          }});
}

TEST_CASE_FIXTURE(LexerFixture,
                  "Lexer parses floats with fractional int parts") {
    init(makeNumber(0, 100, '9'));

    CHECK(next() == Token{TokenKind::FloatLiteral,
                          Position{.line = 1, .column = 1, .offset = 0},
                          TokenValue{1.0}});

    CHECK(next() == Token{TokenKind::ETX, Position{
                                              .line = 1,
                                              .column = 103,
                                              .offset = 102,
                                          }});
}

TEST_CASE_FIXTURE(LexerFixture,
                  "Lexer detects float overflow from large value") {
    init(makeNumber(500, 10, '9'));

    CHECK(next() == Token{TokenKind::ERROR,
                          Position{.line = 1, .column = 1, .offset = 0}});

    CHECK(diagnostics().all() ==
          makeDiags({LexerDiagnostic{
              .kind = FloatOutOfRange{},
              .pos = Position{.line = 1, .column = 1, .offset = 0},
              .severity = Severity::Error}}));

    CHECK(next() == Token{TokenKind::ETX, Position{
                                              .line = 1,
                                              .column = 512,
                                              .offset = 511,
                                          }});
}
