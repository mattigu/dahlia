#include "Lexer.h"

#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string_view>

#include "CharReader.h"
#include "LexerDiagnostics.h"
#include "Position.h"
#include "Token.h"

static constexpr auto SINGLE_CHAR_TOKENS =
    std::to_array<std::pair<char, TokenKind>>({
        {'(', TokenKind::ParenOpen},
        {')', TokenKind::ParenClose},
        {'[', TokenKind::BracketOpen},
        {']', TokenKind::BracketClose},
        {'{', TokenKind::BraceOpen},
        {'}', TokenKind::BraceClose},
        {';', TokenKind::Semicolon},
        {',', TokenKind::Comma},
        {'@', TokenKind::At},
        {'?', TokenKind::Question},
        {ETX, TokenKind::ETX},
    });

static constexpr auto KEYWORDS =
    std::to_array<std::pair<std::string_view, TokenKind>>({
        {"let", TokenKind::Let},
        {"mut", TokenKind::Mut},
        {"fn", TokenKind::Fn},
        {"if", TokenKind::If},
        {"else", TokenKind::Else},
        {"for", TokenKind::For},
        {"while", TokenKind::While},
        {"in", TokenKind::In},
        {"return", TokenKind::Return},
        {"true", TokenKind::True},
        {"false", TokenKind::False},
        {"int", TokenKind::Int},
        {"float", TokenKind::Float},
        {"bool", TokenKind::Bool},
        {"str", TokenKind::Str},
        {"vec", TokenKind::Vec},
    });

Lexer::Lexer(std::istream& src, LexerOptions const& options) noexcept
    : src_{src}, current_{TokenKind::STX, src_.position()}, options_(options) {}

Token Lexer::current() const noexcept { return current_; }

[[nodiscard]] Diagnostics<LexerDiagnosticKind> const& Lexer::diagnostics()
    const noexcept {
    return diagnostics_;
}

Token Lexer::next() {
    current_ = tryBuildToken();
    return current_;
}

void Lexer::pushDiag(LexerDiagnosticKind const& kind, Position const& pos) {
    diagnostics_.push({.kind = kind, .pos = pos});
}

Token Lexer::tryBuildToken() {
    skipWhile([](char chr) { return std::isspace(chr) || chr == STX; });

    if (current_.kind() == TokenKind::ETX) {
        return current_;
    }

    auto const token_pos = src_.position();

    auto result =
        tryBuildSingleCharToken()
            .or_else([this]() { return tryBuildComment(); })
            .or_else([this]() { return tryBuildNumber(); })
            .or_else([&]() {
                return tryBuildOperator().transform(
                    [&](TokenKind kind) { return Token(kind, token_pos); });
            })
            .or_else([this]() { return tryBuildString(); })
            .or_else([this]() { return tryBuildIdentifierOrKeyword(); });

    if (result.has_value()) {
        return *result;
    }

    pushDiag(UnexpectedChar{.chr = src_.current()}, src_.position());
    src_.next();

    return {TokenKind::ERROR, token_pos};
}

std::optional<Token> Lexer::tryBuildSingleCharToken() {
    auto const* const token = std::ranges::find(
        SINGLE_CHAR_TOKENS, src_.current(), &std::pair<char, TokenKind>::first);

    auto const pos = src_.position();

    if (token != SINGLE_CHAR_TOKENS.cend()) {
        src_.next();
        return Token(token->second, pos);
    }

    return std::nullopt;
}

std::optional<Token> Lexer::tryBuildComment() {
    if (src_.current() != '#') {
        return std::nullopt;
    }
    auto const start_pos = src_.position();
    auto constexpr comment_char = [](char chr) { return chr != NEWLINE; };

    src_.next();
    auto const [comment, too_long] =
        buildTextWhile(options_.max_comment_len, comment_char);

    if (too_long) {
        skipWhile(comment_char);
        pushDiag(CommentTooLong{}, start_pos);
        return Token(TokenKind::Comment, start_pos, comment);
    }

    return Token(TokenKind::Comment, start_pos, comment);
}

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
            pushDiag(ExpectedChar{.expected = '.', .got = src_.current()},
                     src_.position());
            return TokenKind::ERROR;

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
}

TokenKind Lexer::extendOperator(TokenKind kind, char extend_char,
                                TokenKind extended_kind) {
    if (src_.next() == extend_char) {
        src_.next();
        return extended_kind;
    }
    return kind;
}

std::optional<Token> Lexer::tryBuildString() {
    if (src_.current() != '"') {
        return std::nullopt;
    }
    auto const start_pos = src_.position();

    src_.next();
    auto constexpr string_end = [](char chr) {
        return chr == '"' || chr == NEWLINE || chr == ETX;
    };

    std::string string;
    while (!string_end(src_.current()) &&
           string.length() < options_.max_string_len) {
        if (src_.current() == '\\') {
            if (auto const escape = tryBuildEscapeSequence()) {
                string += escape.value();
            } else {
                skipWhile([&](char chr) { return !string_end(chr); });
                if (src_.current() == '"') {
                    src_.next();
                }
                return Token(TokenKind::ERROR, start_pos);
            }
            continue;
        }
        string += src_.current();
        src_.next();
    }

    if (string.length() == options_.max_string_len &&
        !string_end(src_.current())) {
        skipWhile([&](char chr) { return !string_end(chr); });
        pushDiag(StringTooLong{}, start_pos);
        if (src_.current() == '"') {
            src_.next();
        }
        return Token(TokenKind::StrLiteral, start_pos, string);
    }

    if (src_.current() == '"') {
        src_.next();
        return Token(TokenKind::StrLiteral, start_pos, string);
    }
    pushDiag(UnterminatedString{}, start_pos);
    return Token(TokenKind::ERROR, start_pos);
}

std::optional<char> Lexer::tryBuildEscapeSequence() {
    if (src_.current() != '\\') {
        return std::nullopt;
    }
    src_.next();

    switch (src_.current()) {
        case '"':
            src_.next();
            return '"';
        case '\\':
            src_.next();
            return '\\';
        case 'n':
            src_.next();
            return '\n';
        case 'r':
            src_.next();
            return '\r';
        case 't':
            src_.next();
            return '\t';
        case 'x':
            return tryBuildHexEscape();
        default:
            pushDiag(InvalidEscapeSequence{.chr = src_.current()},
                     src_.position());
            return std::nullopt;
    }
}
std::optional<char> Lexer::tryBuildHexEscape() {
    if (src_.current() != 'x') {
        return std::nullopt;
    }

    char const high = src_.next();
    if (std::isxdigit(high) == 0) {
        pushDiag(InvalidHexEscape{.chr = high}, src_.position());
        return std::nullopt;
    }
    char const low = src_.next();
    if (std::isxdigit(low) == 0) {
        pushDiag(InvalidHexEscape{.chr = low}, src_.position());
        return std::nullopt;
    }
    src_.next();

    return static_cast<char>(std::stoi(std::string{high, low}, nullptr, 16));
}

std::optional<BuiltDigits> Lexer::tryBuildDigits(DigitMode mode) {
    auto constexpr is_num_char = [](char chr) {
        return std::isdigit(chr) != 0 || chr == '_';
    };

    if (src_.current() == '_') {
        pushDiag(InvalidNumericSeparator{}, src_.position());
        skipWhile(is_num_char);
        return std::nullopt;
    }

    BuiltDigits result{};
    char prev = 0;
    auto prev_pos = src_.position();

    double frac_scale = 0.1;
    if (mode == DigitMode::Fraction) {
        result.value = 0.0;
    }

    while (is_num_char(src_.current())) {
        char curr = src_.current();

        if (curr == '_' && prev == '_') {
            pushDiag(InvalidNumericSeparator{}, src_.position());
            skipWhile(is_num_char);
            return std::nullopt;
        }

        if (curr != '_') {
            auto const digit = static_cast<uint8_t>(curr - '0');

            if (mode == DigitMode::Integer) {
                appendIntegerDigit(result, digit);
                if (result.overflowed) {
                    skipWhile(is_num_char);
                    return result;
                }
            } else {
                appendFractionDigit(result, digit, frac_scale);
                frac_scale *= 0.1;
            }
        }

        prev = curr;
        prev_pos = src_.position();
        src_.next();
    }

    if (prev == '_') {
        pushDiag(InvalidNumericSeparator{}, prev_pos);
        skipWhile(is_num_char);
        return std::nullopt;
    }

    return result;
}

std::optional<Token> Lexer::tryBuildNumber() {
    auto constexpr is_num_char = [](char chr) {
        return std::isdigit(chr) != 0 || chr == '_';
    };

    auto constexpr as_double = [](auto const& value) {
        if (auto num = std::get_if<uint64_t>(&value)) {
            return static_cast<double>(*num);
        }
        return std::get<double>(value);
    };

    if (!is_num_char(src_.current())) {
        return std::nullopt;
    }

    auto const start_pos = src_.position();
    auto const int_part = tryBuildDigits(DigitMode::Integer);

    // Float
    if (src_.current() == '.' && src_.peek() != '.') {
        src_.next();
        if (!int_part.has_value()) {
            skipWhile(is_num_char);
            return Token(TokenKind::ERROR, start_pos);
        }
        if (int_part->overflowed) {
            skipWhile(is_num_char);
            pushDiag(FloatOutOfRange{}, start_pos);
            return Token(TokenKind::ERROR, start_pos);
        }

        auto const fraction_part = tryBuildDigits(DigitMode::Fraction);
        if (!fraction_part.has_value()) {
            return Token(TokenKind::ERROR, start_pos);
        }

        double value =
            as_double(int_part->value) + as_double(fraction_part->value);

        if (!std::isfinite(value)) {
            pushDiag(FloatOutOfRange{}, start_pos);
            return Token(TokenKind::ERROR, start_pos);
        }
        return Token(TokenKind::FloatLiteral, start_pos, value);
    }

    // Int
    if (!int_part) {
        return Token(TokenKind::ERROR, start_pos);
    }

    if (std::holds_alternative<double>(int_part->value)) {
        pushDiag(IntegerOverflow{}, start_pos);
        return Token(TokenKind::ERROR, start_pos);
    }

    uint64_t value = std::get<uint64_t>(int_part->value);

    if (value > static_cast<uint64_t>(options_.int_range)) {
        pushDiag(IntegerOverflow{}, start_pos);
        return Token(TokenKind::ERROR, start_pos);
    }

    return Token(TokenKind::IntLiteral, start_pos, static_cast<int64_t>(value));
}

void Lexer::appendIntegerDigit(BuiltDigits& result, uint8_t digit) {
    ++result.count;

    if (auto* value = std::get_if<uint64_t>(&result.value)) {
        if (*value <= (std::numeric_limits<uint64_t>::max() - digit) / 10) {
            *value = (*value * 10) + digit;
            return;
        }

        result.value = (static_cast<double>(*value) * 10.0) + digit;

        return;
    }

    auto& value = std::get<double>(result.value);
    value = (value * 10.0) + digit;

    if (!std::isfinite(value)) {
        result.overflowed = true;
    }
}
void Lexer::appendFractionDigit(BuiltDigits& result, uint8_t digit,
                                double scale) {
    auto& value = std::get<double>(result.value);
    value += digit * scale;
}

std::optional<Token> Lexer::tryBuildIdentifierOrKeyword() {
    auto constexpr key_or_ident_char = [](char chr) {
        return std::isalnum(chr) || chr == '_';
    };

    if (std::isalpha(src_.current()) == 0) {
        return std::nullopt;
    }
    auto const start_pos = src_.position();
    std::string ident_or_key{src_.current()};
    src_.next();

    auto const [word, too_long] =
        buildTextWhile(options_.max_identifier_len - 1, key_or_ident_char);

    if (too_long) {
        skipWhile(key_or_ident_char);
        pushDiag(IdentifierTooLong{}, start_pos);
        return Token(TokenKind::ERROR, start_pos);
    }

    ident_or_key += word;

    auto const* const keyword = std::ranges::find(
        KEYWORDS, ident_or_key, &std::pair<std::string_view, TokenKind>::first);

    if (keyword != KEYWORDS.end()) {
        return Token(keyword->second, start_pos);
    }
    return Token(TokenKind::Identifier, start_pos, ident_or_key);
}

void Lexer::skipWhile(std::function<bool(char)> const& predicate) {
    while (src_.current() != ETX && predicate(src_.current())) {
        src_.next();
    }
}

std::pair<std::string, bool> Lexer::buildTextWhile(
    std::size_t max_size, std::function<bool(char)> const& predicate) {
    std::string text;

    while (src_.current() != ETX && predicate(src_.current()) &&
           text.length() < max_size) {
        text += src_.current();
        src_.next();
    }
    bool const too_long = text.size() == max_size && predicate(src_.current());
    return {text, too_long};
}
