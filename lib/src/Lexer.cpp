#include "Lexer.h"

#include <cctype>
#include <cmath>
#include <optional>

#include "CharReader.h"
#include "LexerDiagnostics.h"
#include "Position.h"
#include "Token.h"

static std::unordered_map<char, TokenKind> const SINGLE_CHAR_TOKENS = {
    {'(', TokenKind::ParenOpen},   {')', TokenKind::ParenClose},
    {'[', TokenKind::BracketOpen}, {']', TokenKind::BracketClose},
    {'{', TokenKind::BraceOpen},   {'}', TokenKind::BraceClose},
    {';', TokenKind::Semicolon},   {',', TokenKind::Comma},
    {'@', TokenKind::At},          {'?', TokenKind::Question},
    {ETX, TokenKind::ETX}};

static std::unordered_map<std::string, TokenKind> const KEYWORDS = {
    {"let", TokenKind::Let},       {"mut", TokenKind::Mut},
    {"fn", TokenKind::Fn},         {"if", TokenKind::If},
    {"else", TokenKind::Else},     {"for", TokenKind::For},
    {"while", TokenKind::While},   {"in", TokenKind::In},
    {"return", TokenKind::Return}, {"true", TokenKind::True},
    {"false", TokenKind::False},   {"int", TokenKind::Int},
    {"float", TokenKind::Float},   {"bool", TokenKind::Bool},
    {"str", TokenKind::Str},       {"vec", TokenKind::Vec},
};

Lexer::Lexer(std::istream& src, std::ostream& diagnostics) noexcept
    : src_{src}, current_{.kind = TokenKind::STX, .pos = src_.position()} {}

Token Lexer::current() const noexcept { return current_; }

[[nodiscard]] Diagnostics<LexerDiagnosticKind> const& Lexer::diagnostics()
    const noexcept {
    return diagnostics_;
};

Token Lexer::next() {
    while (true) {
        auto const token = tryBuildToken();
        if (token.has_value()) {
            current_ = token.value();
            break;
        }
    }
    return current_;
}

std::optional<Token> Lexer::tryBuildToken() {
    skipWhile([](char chr) { return std::isspace(chr) || chr == STX; });

    if (current_.kind == TokenKind::ETX) {
        return current_;
    }

    auto const token_pos = src_.position();

    auto result =
        tryBuildSingleCharToken()
            .or_else([this]() { return tryBuildComment(); })
            .or_else([this]() { return tryBuildNumber(); })
            .or_else([this]() {
                return tryBuildOperator().transform(
                    [](TokenKind kind) { return Token{.kind = kind}; });
            })
            .or_else([this]() { return tryBuildString(); })
            .or_else([this]() { return tryBuildIdentifierOrKeyword(); });

    if (result.has_value()) {
        result->pos = token_pos;
        return result;
    }
    diagnostics_.push({.kind = UnexpectedChar{.chr = src_.current()},
                       .pos = src_.position()});
    src_.next();

    return std::nullopt;
};

std::optional<Token> Lexer::tryBuildSingleCharToken() {
    auto const token = SINGLE_CHAR_TOKENS.find(src_.current());
    if (token != SINGLE_CHAR_TOKENS.cend()) {
        src_.next();
        return Token{.kind = token->second};
    }
    return std::nullopt;
};

std::optional<Token> Lexer::tryBuildComment() {
    if (src_.current() != '#') {
        return std::nullopt;
    }

    src_.next();
    std::string const comment =
        buildTextWhile([](char chr) { return chr != NEWLINE && chr != ETX; });

    return Token{.kind = TokenKind::Comment, .value = comment};
};

std::optional<TokenKind> Lexer::tryBuildOperator() {
    switch (src_.current()) {
        case '+':
            return extendOperator(TokenKind::Plus, '=', TokenKind::PlusEqual);
        case '-':
            return extendOperator(TokenKind::Minus, '=', TokenKind::MinusEqual);
        case '*':
            return extendOperator(TokenKind::Asterisk, '=',
                                  TokenKind::AsteriskEqual);
        case '/':
            return extendOperator(TokenKind::Slash, '=', TokenKind::SlashEqual);
        case '%':
            return extendOperator(TokenKind::Percent, '=',
                                  TokenKind::PercentEqual);
        case '=':
            return extendOperator(TokenKind::Equal, '=', TokenKind::EqualEqual);
        case '!':
            return extendOperator(TokenKind::Exclamation, '=',
                                  TokenKind::ExclamationEqual);
        case '<':
            return extendOperator(TokenKind::Less, '=', TokenKind::LessEqual);

        case '.':
            if (src_.next() == '.') {
                if (src_.next() == '=') {
                    src_.next();
                    return TokenKind::DotDotEq;
                }
                return TokenKind::DotDot;
            }
            diagnostics_.push({.kind = LexerDiagnosticKind{ExpectedChar{
                                   .expected = '.', .got = src_.current()}},
                               .pos = src_.position()});
            return TokenKind::DotDot;

        case ':':
            if (src_.next() == '>') {
                src_.next();
                return TokenKind::ColonGreater;
            }
            return TokenKind::Colon;
        case '>': {
            auto const curr = src_.next();
            if (curr == '<') {
                src_.next();
                return TokenKind::GreaterLess;
            }
            if (curr == '=') {
                src_.next();
                return TokenKind::GreaterEqual;
            }
            return TokenKind::Greater;
        }
        default:
            return std::nullopt;
    }
};

TokenKind Lexer::extendOperator(TokenKind kind, char extend_char,
                                TokenKind extended_kind) {
    if (src_.next() == extend_char) {
        src_.next();
        return extended_kind;
    }
    return kind;
};

std::optional<Token> Lexer::tryBuildString() {
    if (src_.current() != '"') {
        return std::nullopt;
    }
    src_.next();
    std::string text;
    auto const string_end = [](char chr) {
        return chr == '"' || chr == NEWLINE || chr == ETX;
    };

    auto const start_pos = src_.position();

    while (!string_end(src_.current())) {
        text += buildTextWhile(
            [&](char chr) { return chr != '\\' && !string_end(chr); });

        if (auto const escape = tryBuildEscapeSequence()) {
            text += escape.value();
        }
    }
    if (src_.current() == '"') {
        src_.next();
        return Token{.kind = TokenKind::StrLiteral, .value = text};
    }
    diagnostics_.push({.kind = UnterminatedString{}, .pos = start_pos});
    return Token{.kind = TokenKind::StrLiteral, .value = text};
};

std::optional<char> Lexer::tryBuildEscapeSequence() {
    if (src_.current() != '\\') {
        return std::nullopt;
    }
    src_.next();

    char value = 0;

    switch (src_.current()) {
        case '"':
            value = '"';
            break;
        case '\\':
            value = '\\';
            break;
        case 'n':
            value = '\n';
            break;
        case 'r':
            value = '\r';
            break;
        case 't':
            value = '\t';
            break;
        case 'x':
            return tryBuildHexEscape();
        default:
            diagnostics_.push(
                {.kind = InvalidEscapeSequence{.chr = src_.current()},
                 .pos = src_.position()});
            return std::nullopt;
    }
    src_.next();
    return value;
}
std::optional<char> Lexer::tryBuildHexEscape() {
    if (src_.current() != 'x') {
        return std::nullopt;
    }

    char const high = src_.next();
    if (std::isxdigit(high) == 0) {
        diagnostics_.push(
            {.kind = InvalidHexEscape{.chr = high}, .pos = src_.position()});
        return std::nullopt;
    }
    char const low = src_.next();
    if (std::isxdigit(low) == 0) {
        diagnostics_.push(
            {.kind = InvalidHexEscape{.chr = low}, .pos = src_.position()});
        return std::nullopt;
    }
    src_.next();

    auto constexpr base16 = 16;
    return static_cast<char>(
        std::stoi(std::string{high, low}, nullptr, base16));
}

std::optional<Token> Lexer::tryBuildNumber() {
    if (std::isdigit(src_.current()) == 0 && src_.current() != '_') {
        return std::nullopt;
    }

    auto const start_pos = src_.position();

    auto const int_part = tryBuildDigits();

    // Float
    if (src_.current() == '.' && src_.peek() != '.') {
        src_.next();

        if (!int_part.has_value()) {
            return Token{.kind = TokenKind::FloatLiteral};
        }

        auto const fraction_part = tryBuildDigits();

        if (!fraction_part.has_value()) {
            return Token{.kind = TokenKind::FloatLiteral};
        }

        if (fraction_part->empty()) {
            diagnostics_.push(
                {.kind = InvalidNumericLiteral{}, .pos = start_pos});
            return Token{.kind = TokenKind::FloatLiteral};
        }
        if (int_part->size() > 1 && (*int_part)[0] == '0') {
            diagnostics_.push(
                {.kind = InvalidNumericLiteral{}, .pos = start_pos});
            return Token{.kind = TokenKind::FloatLiteral};
        }

        auto const float_value =
            tryBuildFloatValue(*int_part, *fraction_part, start_pos);

        if (float_value.has_value()) {
            return Token{.kind = TokenKind::FloatLiteral,
                         .value = *float_value};
        }
        return Token{.kind = TokenKind::FloatLiteral};
    }
    // Int
    if (!int_part.has_value()) {
        return Token{.kind = TokenKind::IntLiteral};
    }

    if (int_part->size() > 1 && (*int_part)[0] == '0') {
        diagnostics_.push({.kind = InvalidNumericLiteral{}, .pos = start_pos});
        return Token{.kind = TokenKind::IntLiteral};
    }
    auto const int_value = tryBuildIntValue(*int_part, start_pos);

    if (int_value.has_value()) {
        return Token{.kind = TokenKind::IntLiteral, .value = *int_value};
    }
    return Token{.kind = TokenKind::IntLiteral};
}

std::optional<std::string> Lexer::tryBuildDigits() {
    auto constexpr is_num_char = [](char chr) {
        return std::isdigit(chr) != 0 || chr == '_';
    };

    if (src_.current() == '_') {
        diagnostics_.push(
            {.kind = InvalidNumericSeparator{}, .pos = src_.position()});
        skipWhile(is_num_char);
        return std::nullopt;
    }

    if (!is_num_char(src_.current())) {
        return std::string{};
    }

    std::string digits;
    char prev = 0;
    auto prev_pos = src_.position();

    while (is_num_char(src_.current())) {
        char curr = src_.current();

        if (curr == '_' && prev == '_') {
            diagnostics_.push(
                {.kind = InvalidNumericSeparator{}, .pos = src_.position()});
            skipWhile(is_num_char);
            return std::nullopt;
        }

        if (curr != '_') {
            digits += curr;
        }

        prev = curr;
        prev_pos = src_.position();
        src_.next();
    }

    if (prev == '_') {
        diagnostics_.push({.kind = InvalidNumericSeparator{}, .pos = prev_pos});
        return std::nullopt;
    }

    return digits;
}

std::optional<std::int64_t> Lexer::tryBuildIntValue(std::string_view integer,
                                                    Position const& pos) {
    int64_t value = 0;

    for (char chr : integer) {
        auto const digit = chr - '0';

        if (value > (INT_RANGE - digit) / 10) {
            diagnostics_.push(
                {.kind = LexerDiagnosticKind{IntegerOverflow{}}, .pos = pos});
            return std::nullopt;
        }

        value = (value * 10) + digit;
    }
    return value;
};

std::optional<double> Lexer::tryBuildFloatValue(
    std::string_view integer,  // NOLINT
    std::string_view fraction, Position const& pos) {
    double value = 0.0;

    for (char chr : integer) {
        value = (value * 10.0) + (chr - '0');
    }

    double place = 0.1;

    for (char chr : fraction) {
        value += (chr - '0') * place;
        place *= 0.1;
    }

    if (!std::isfinite(value)) {
        diagnostics_.push({.kind = FloatOutOfRange{}, .pos = pos});
        return std::nullopt;
    }

    return value;
};

std::optional<Token> Lexer::tryBuildIdentifierOrKeyword() {
    if (std::isalpha(src_.current()) == 0) {
        return std::nullopt;
    }

    std::string const word = buildTextWhile(
        [](char chr) { return std::isalnum(chr) || chr == '_'; });

    auto const keyword = KEYWORDS.find(word);
    if (keyword != KEYWORDS.end()) {
        return Token{.kind = keyword->second};
    }
    return Token{.kind = TokenKind::Identifier, .value = word};
};

void Lexer::skipWhile(std::function<bool(char)> const& predicate) {
    while (src_.current() != ETX && predicate(src_.current())) {
        src_.next();
    }
}

std::string Lexer::buildTextWhile(std::function<bool(char)> const& predicate) {
    std::string text;
    while (src_.current() != ETX && predicate(src_.current())) {
        text += src_.current();
        src_.next();
    }
    return text;
}
