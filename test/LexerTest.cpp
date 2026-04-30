#include <sstream>
#include <string>

#include "doctest.h"
#include "src/Lexer.h"
#include "src/LexerDiagnostics.h"
#include "src/Position.h"
#include "src/Token.h"

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
};

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

TEST_CASE_FIXTURE(LexerFixture,
                  "Lexer reports and skips unexpected characters") {
    init("let^;");
    CHECK(next().kind == TokenKind::Let);

    auto token = next();
    CHECK(token.kind == TokenKind::ERROR);

    REQUIRE(!diagnostics().empty());

    CHECK(diagnostics().last().kind ==
          LexerDiagnosticKind{UnexpectedChar{.chr = '^'}});
    CHECK(diagnostics().last().pos ==
          Position{.line = 1, .column = 4, .offset = 3});

    CHECK(next().kind == TokenKind::Semicolon);
    CHECK(next().kind == TokenKind::ETX);
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

TEST_CASE_FIXTURE(LexerFixture, "Lexer tokenizes comments") {
    init("# Hello \n# World");
    auto token = next();
    CHECK(token.kind == TokenKind::Comment);
    CHECK(token.value == TokenValue(" Hello "));

    token = next();
    CHECK(token.kind == TokenKind::Comment);
    CHECK(token.value == TokenValue(" World"));
    CHECK(next().kind == TokenKind::ETX);
}

TEST_CASE_FIXTURE(LexerFixture, "Lexer reports comments too long") {
    init("#1\n#12\n;", {.max_comment_len = 1});
    auto token = next();
    CHECK(token.kind == TokenKind::Comment);

    token = next();
    CHECK(token.kind == TokenKind::Comment);

    REQUIRE(!diagnostics().empty());

    CHECK(diagnostics().last().kind == LexerDiagnosticKind{CommentTooLong{}});
    CHECK(diagnostics().last().pos ==
          Position{.line = 2, .column = 1, .offset = 3});

    token = next();
    CHECK(token.kind == TokenKind::Semicolon);
    CHECK(next().kind == TokenKind::ETX);
}

TEST_CASE_FIXTURE(LexerFixture, "Lexer tokenizes simple strings") {
    init(R"("hello")");
    auto const token = next();
    CHECK(token.kind == TokenKind::StrLiteral);
    CHECK(token.value == TokenValue("hello"));
}

TEST_CASE_FIXTURE(LexerFixture, "Lexer recognizes unterminated strings") {
    init(R"("hello
        ;)");

    auto const token = next();
    CHECK(token.kind == TokenKind::ERROR);

    REQUIRE(!diagnostics().empty());

    CHECK(diagnostics().last().kind ==
          LexerDiagnosticKind{UnterminatedString{}});

    CHECK(diagnostics().last().pos ==
          Position{.line = 1, .column = 1, .offset = 0});

    CHECK(next().kind == TokenKind::Semicolon);
    CHECK(next().kind == TokenKind::ETX);
}

TEST_CASE_FIXTURE(LexerFixture, "Lexer reports strings too long") {
    init(R"("a" "ab" ;)", {.max_string_len = 1});
    auto token = next();
    CHECK(token.kind == TokenKind::StrLiteral);
    CHECK(token.value == TokenValue{"a"});

    token = next();
    CHECK(token.kind == TokenKind::StrLiteral);

    REQUIRE(!diagnostics().empty());

    CHECK(diagnostics().last().kind == LexerDiagnosticKind{StringTooLong{}});

    CHECK(diagnostics().last().pos ==
          Position{.line = 1, .column = 5, .offset = 4});

    CHECK(next().kind == TokenKind::Semicolon);
    CHECK(next().kind == TokenKind::ETX);
}

TEST_CASE_FIXTURE(LexerFixture,
                  "Lexer tokenizes strings with escape sequences") {
    init(R"(" 1 \t 2 \n 3 \" 4 \\ 5 \r 6 ")");
    auto const token = next();
    CHECK(token.kind == TokenKind::StrLiteral);
    CHECK(token.value == TokenValue(" 1 \t 2 \n 3 \" 4 \\ 5 \r 6 "));
}

TEST_CASE_FIXTURE(LexerFixture,
                  "Lexer recognizes and reports invalid escape sequences") {
    init(R"("ab\c" ;)");
    auto const token = next();
    CHECK(token.kind == TokenKind::ERROR);

    REQUIRE(!diagnostics().empty());

    CHECK(diagnostics().last().kind ==
          LexerDiagnosticKind{InvalidEscapeSequence{.chr = 'c'}});
    CHECK(diagnostics().last().pos ==
          Position{.line = 1, .column = 5, .offset = 4});

    CHECK(next().kind == TokenKind::Semicolon);
    CHECK(next().kind == TokenKind::ETX);
}

TEST_CASE_FIXTURE(LexerFixture,
                  "Lexer tokenizes strings with hex escape sequences") {
    init(R"("\x48\x65\x6C\x6C\x6F")");
    auto const token = next();
    CHECK(token.kind == TokenKind::StrLiteral);
    CHECK(token.value == TokenValue("Hello"));
    CHECK(next().kind == TokenKind::ETX);
}

TEST_CASE_FIXTURE(LexerFixture,
                  "Lexer recognizes and reports invalid hex escape sequences") {
    init(R"("\xLc" ;)");
    auto const token = next();

    CHECK(token.kind == TokenKind::ERROR);

    REQUIRE(!diagnostics().empty());

    CHECK(diagnostics().last().kind ==
          LexerDiagnosticKind{InvalidHexEscape{.chr = 'L'}});
    CHECK(diagnostics().last().pos ==
          Position{.line = 1, .column = 4, .offset = 3});

    CHECK(next().kind == TokenKind::Semicolon);
    CHECK(next().kind == TokenKind::ETX);
}

TEST_CASE_FIXTURE(LexerFixture, "Lexer tokenizes simple operators") {
    init("+ - * / % = ! < > :");
    CHECK(next().kind == TokenKind::Plus);
    CHECK(next().kind == TokenKind::Minus);
    CHECK(next().kind == TokenKind::Asterisk);
    CHECK(next().kind == TokenKind::Slash);
    CHECK(next().kind == TokenKind::Percent);
    CHECK(next().kind == TokenKind::Equal);
    CHECK(next().kind == TokenKind::Exclamation);
    CHECK(next().kind == TokenKind::Less);
    CHECK(next().kind == TokenKind::Greater);
    CHECK(next().kind == TokenKind::Colon);
    CHECK(next().kind == TokenKind::ETX);
}

TEST_CASE_FIXTURE(LexerFixture,
                  "Lexer tokenizes compound assignment operators") {
    init("+= -= *= /= %= == != <= >=");
    CHECK(next().kind == TokenKind::PlusEqual);
    CHECK(next().kind == TokenKind::MinusEqual);
    CHECK(next().kind == TokenKind::AsteriskEqual);
    CHECK(next().kind == TokenKind::SlashEqual);
    CHECK(next().kind == TokenKind::PercentEqual);
    CHECK(next().kind == TokenKind::EqualEqual);
    CHECK(next().kind == TokenKind::ExclamationEqual);
    CHECK(next().kind == TokenKind::LessEqual);
    CHECK(next().kind == TokenKind::GreaterEqual);
    CHECK(next().kind == TokenKind::ETX);
}

TEST_CASE_FIXTURE(LexerFixture, "Lexer tokenizes special operators") {
    init(".. ..= :> ><");
    CHECK(next().kind == TokenKind::DotDot);
    CHECK(next().kind == TokenKind::DotDotEq);
    CHECK(next().kind == TokenKind::ColonGreater);
    CHECK(next().kind == TokenKind::GreaterLess);
    CHECK(next().kind == TokenKind::ETX);
}

TEST_CASE_FIXTURE(LexerFixture, "Lexer reports 'Expected' error for ..") {
    init("for x in a.b");

    CHECK(next().kind == TokenKind::For);
    CHECK(next().kind == TokenKind::Identifier);
    CHECK(next().kind == TokenKind::In);
    CHECK(next().kind == TokenKind::Identifier);

    CHECK(next().kind == TokenKind::ERROR);

    REQUIRE(!diagnostics().empty());

    CHECK(diagnostics().last().kind ==
          LexerDiagnosticKind{ExpectedChar{.expected = '.', .got = 'b'}});
    CHECK(diagnostics().last().pos ==
          Position{.line = 1, .column = 12, .offset = 11});

    CHECK(next().kind == TokenKind::Identifier);
    CHECK(next().kind == TokenKind::ETX);
}

TEST_CASE_FIXTURE(LexerFixture, "Lexer tokenizes keywords") {
    init(
        "let mut fn if else for while in return true false int float bool str "
        "vec");

    CHECK(next().kind == TokenKind::Let);
    CHECK(next().kind == TokenKind::Mut);
    CHECK(next().kind == TokenKind::Fn);
    CHECK(next().kind == TokenKind::If);
    CHECK(next().kind == TokenKind::Else);
    CHECK(next().kind == TokenKind::For);
    CHECK(next().kind == TokenKind::While);
    CHECK(next().kind == TokenKind::In);
    CHECK(next().kind == TokenKind::Return);
    CHECK(next().kind == TokenKind::True);
    CHECK(next().kind == TokenKind::False);
    CHECK(next().kind == TokenKind::Int);
    CHECK(next().kind == TokenKind::Float);
    CHECK(next().kind == TokenKind::Bool);
    CHECK(next().kind == TokenKind::Str);
    CHECK(next().kind == TokenKind::Vec);
    CHECK(next().kind == TokenKind::ETX);
}

TEST_CASE_FIXTURE(LexerFixture, "Lexer tokenizes valid identifiers") {
    init("x FOO bar123 camelCase snake_case a1b2c3");

    auto token = next();
    CHECK(token.kind == TokenKind::Identifier);
    CHECK(token.value == TokenValue("x"));

    token = next();
    CHECK(token.kind == TokenKind::Identifier);
    CHECK(token.value == TokenValue("FOO"));

    token = next();
    CHECK(token.kind == TokenKind::Identifier);
    CHECK(token.value == TokenValue("bar123"));

    token = next();
    CHECK(token.kind == TokenKind::Identifier);
    CHECK(token.value == TokenValue("camelCase"));

    token = next();
    CHECK(token.kind == TokenKind::Identifier);
    CHECK(token.value == TokenValue("snake_case"));

    token = next();
    CHECK(token.kind == TokenKind::Identifier);
    CHECK(token.value == TokenValue("a1b2c3"));

    CHECK(next().kind == TokenKind::ETX);
}

TEST_CASE_FIXTURE(LexerFixture,
                  "Lexer distinguishes keywords from identifiers") {
    init("let letter if ifelse for fortune");

    CHECK(next().kind == TokenKind::Let);

    auto token = next();
    CHECK(token.kind == TokenKind::Identifier);
    CHECK(token.value == TokenValue("letter"));

    CHECK(next().kind == TokenKind::If);

    token = next();
    CHECK(token.kind == TokenKind::Identifier);
    CHECK(token.value == TokenValue("ifelse"));

    CHECK(next().kind == TokenKind::For);

    token = next();
    CHECK(token.kind == TokenKind::Identifier);
    CHECK(token.value == TokenValue("fortune"));

    CHECK(next().kind == TokenKind::ETX);
}

TEST_CASE_FIXTURE(LexerFixture, "Lexer reports identifiers too long") {
    init("a ab ;", {.max_identifier_len = 1});
    auto token = next();
    CHECK(token.kind == TokenKind::Identifier);
    CHECK(token.value == TokenValue{"a"});

    token = next();
    CHECK(token.kind == TokenKind::ERROR);

    REQUIRE(!diagnostics().empty());

    CHECK(diagnostics().last().kind ==
          LexerDiagnosticKind{IdentifierTooLong{}});

    CHECK(diagnostics().last().pos ==
          Position{.line = 1, .column = 3, .offset = 2});

    token = next();
    CHECK(token.kind == TokenKind::Semicolon);
    CHECK(next().kind == TokenKind::ETX);
}

TEST_CASE_FIXTURE(LexerFixture, "Lexer tokenizes simple integers") {
    init("617 5 123456789");

    next();
    CHECK(current().kind == TokenKind::IntLiteral);
    CHECK(current().value == TokenValue{617});

    next();
    CHECK(current().kind == TokenKind::IntLiteral);
    CHECK(current().value == TokenValue{5});

    next();
    CHECK(current().kind == TokenKind::IntLiteral);
    CHECK(current().value == TokenValue{123456789});

    CHECK(next().kind == TokenKind::ETX);
}

TEST_CASE_FIXTURE(LexerFixture, "Lexer tokenizes int of value 0") {
    init("0");

    next();
    CHECK(current().kind == TokenKind::IntLiteral);
    CHECK(current().value == TokenValue{0});
}

TEST_CASE_FIXTURE(LexerFixture,
                  "Lexer, leading zeros not allowed on integers") {
    init("003");

    next();
    CHECK(current().kind == TokenKind::ERROR);

    REQUIRE(!diagnostics().empty());

    CHECK(diagnostics().last().kind ==
          LexerDiagnosticKind{InvalidNumericLiteral{}});
    CHECK(diagnostics().last().pos ==
          Position{.line = 1, .column = 1, .offset = 0});

    CHECK(next().kind == TokenKind::ETX);
}

TEST_CASE_FIXTURE(LexerFixture, "Lexer, leading zeros not allowed on floats") {
    init("00.1");

    next();
    CHECK(current().kind == TokenKind::ERROR);

    REQUIRE(!diagnostics().empty());

    CHECK(diagnostics().last().kind ==
          LexerDiagnosticKind{InvalidNumericLiteral{}});
    CHECK(diagnostics().last().pos ==
          Position{.line = 1, .column = 1, .offset = 0});

    CHECK(next().kind == TokenKind::ETX);
}

TEST_CASE_FIXTURE(LexerFixture, "Lexer detects integer overflow") {
    // First one is max int
    init("9223372036854775807 9223372036854775808");

    next();
    CHECK(current().kind == TokenKind::IntLiteral);
    CHECK(current().value == TokenValue{9223372036854775807});

    next();
    CHECK(current().kind == TokenKind::ERROR);

    REQUIRE(!diagnostics().empty());

    CHECK(diagnostics().last().kind == LexerDiagnosticKind{IntegerOverflow{}});

    CHECK(next().kind == TokenKind::ETX);
}

TEST_CASE_FIXTURE(LexerFixture, "Lexer detects double separators") {
    init("1__2");

    auto const token = next();
    CHECK(token.kind == TokenKind::ERROR);

    REQUIRE(!diagnostics().empty());

    CHECK(diagnostics().last().kind ==
          LexerDiagnosticKind{InvalidNumericSeparator{}});

    CHECK(diagnostics().last().pos ==
          Position{.line = 1, .column = 3, .offset = 2});

    CHECK(next().kind == TokenKind::ETX);
}

TEST_CASE_FIXTURE(LexerFixture, "Lexer detects leading separator") {
    init("_123");

    auto const token = next();
    CHECK(token.kind == TokenKind::ERROR);

    REQUIRE(!diagnostics().empty());

    CHECK(diagnostics().last().kind ==
          LexerDiagnosticKind{InvalidNumericSeparator{}});

    CHECK(diagnostics().last().pos ==
          Position{.line = 1, .column = 1, .offset = 0});

    CHECK(next().kind == TokenKind::ETX);
}

TEST_CASE_FIXTURE(LexerFixture, "Lexer detects trailing separator") {
    init("123_");

    auto const token = next();
    CHECK(token.kind == TokenKind::ERROR);

    REQUIRE(!diagnostics().empty());

    CHECK(diagnostics().last().kind ==
          LexerDiagnosticKind{InvalidNumericSeparator{}});

    CHECK(diagnostics().last().pos ==
          Position{.line = 1, .column = 4, .offset = 3});

    CHECK(next().kind == TokenKind::ETX);
}

TEST_CASE_FIXTURE(LexerFixture,
                  "Lexer detects separators around decimal point") {
    init("1_.2");

    auto const token = next();
    CHECK(token.kind == TokenKind::ERROR);

    REQUIRE(!diagnostics().empty());

    CHECK(diagnostics().last().kind ==
          LexerDiagnosticKind{InvalidNumericSeparator{}});

    CHECK(diagnostics().last().pos ==
          Position{.line = 1, .column = 2, .offset = 1});

    CHECK(next().kind == TokenKind::ETX);
}

TEST_CASE_FIXTURE(LexerFixture, "Lexer detects floats with no fraction") {
    init("1.");

    auto const token = next();
    CHECK(token.kind == TokenKind::ERROR);

    REQUIRE(!diagnostics().empty());

    CHECK(diagnostics().last().kind ==
          LexerDiagnosticKind{InvalidNumericLiteral{}});

    CHECK(diagnostics().last().pos ==
          Position{.line = 1, .column = 1, .offset = 0});

    CHECK(next().kind == TokenKind::ETX);
}

TEST_CASE_FIXTURE(LexerFixture, "Lexer tokenizes simple floats") {
    init("0.0 1.0");

    auto token = next();

    CHECK(token.kind == TokenKind::FloatLiteral);
    CHECK(token.value == TokenValue{0.0});

    token = next();

    CHECK(token.kind == TokenKind::FloatLiteral);
    CHECK(token.value == TokenValue{1.0});

    CHECK(next().kind == TokenKind::ETX);
}

TEST_CASE_FIXTURE(LexerFixture, "Lexer parses floats with fractional values") {
    init("123.456");

    auto const token = next();

    CHECK(token.kind == TokenKind::FloatLiteral);
    CHECK(std::get<double>(token.value) == doctest::Approx(123.456));

    CHECK(next().kind == TokenKind::ETX);
}

TEST_CASE_FIXTURE(LexerFixture,
                  "Lexer parses floats with zero leading values") {
    init("0.4");

    auto const token = next();

    CHECK(token.kind == TokenKind::FloatLiteral);
    CHECK(std::get<double>(token.value) == doctest::Approx(0.4));

    CHECK(next().kind == TokenKind::ETX);
}

TEST_CASE_FIXTURE(LexerFixture, "Lexer parses floats with large int parts") {
    init(makeNumber(50, 10, '9'));

    auto const token = next();

    CHECK(token.kind == TokenKind::FloatLiteral);

    CHECK(next().kind == TokenKind::ETX);
}

TEST_CASE_FIXTURE(LexerFixture,
                  "Lexer parses floats with fractional int parts") {
    init(makeNumber(0, 100, '9'));

    auto const token = next();

    CHECK(token.kind == TokenKind::FloatLiteral);
    CHECK(std::get<double>(token.value) == doctest::Approx(1.0));

    CHECK(next().kind == TokenKind::ETX);
}

TEST_CASE_FIXTURE(LexerFixture,
                  "Lexer detects float overflow from large value") {
    init(makeNumber(500, 10, '9'));

    auto const token = next();

    CHECK(token.kind == TokenKind::ERROR);

    REQUIRE(!diagnostics().empty());

    CHECK(diagnostics().last().kind == LexerDiagnosticKind{FloatOutOfRange{}});
    CHECK(next().kind == TokenKind::ETX);
}
