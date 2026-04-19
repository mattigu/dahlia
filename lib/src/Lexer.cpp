#include "Lexer.h"

#include <cctype>
#include <optional>

#include "CharReader.h"
#include "Position.h"
#include "Token.h"

static std::unordered_map<char, TokenKind> const SINGLE_CHAR_TOKENS = {
    {'(', TokenKind::ParenOpen},   {')', TokenKind::ParenClose},
    {'[', TokenKind::BracketOpen}, {']', TokenKind::BracketClose},
    {'{', TokenKind::BraceOpen},   {'}', TokenKind::BraceClose},
    {':', TokenKind::Colon},       {';', TokenKind::Semicolon},
    {',', TokenKind::Comma},       {'@', TokenKind::At},
    {'?', TokenKind::Question},    {ETX, TokenKind::ETX}};

Lexer::Lexer(std::istream& src, std::ostream& diagnostics) noexcept
    : src_{src}, current_{.kind = TokenKind::STX, .pos = src_.position()} {}

Token Lexer::current() const noexcept { return current_; }

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

    auto result = tryBuildSingleCharToken()
                      .or_else([this]() { return tryBuildComment(); })
                      .or_else([this]() { return tryBuildString(); });
    if (result.has_value()) {
        result->pos = token_pos;
        return result;
    }

    // Log error

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
std::optional<Token> Lexer::tryBuildString() {
    if (src_.current() != '"') {
        return std::nullopt;
    }
    src_.next();
    std::string text;
    auto const string_end = [](char chr) {
        return chr == '"' || chr == NEWLINE || chr == ETX;
    };

    while (!string_end(src_.current())) {
        text += buildTextWhile(
            [&](char chr) { return chr != '\\' && !string_end(chr); });

        if (auto const escape = tryBuildEscapeSequence()) {
            text += escape.value();
        }
    }
    if (src_.current() == '"') {
        return Token{.kind = TokenKind::StrLiteral, .value = text};
    }
    // TODO! Handle unterminated string
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
        case 't':
            value = '\t';
            break;
        // TODO! In error cases the lexer should consume until end of string or
        // line
        case 'x':
            return tryBuildHexEscape();
        default:
            return std::nullopt;
            // Log unrecognized escape sequence
    }
    src_.next();
    return value;
}
std::optional<char> Lexer::tryBuildHexEscape() {
    if (src_.current() != 'x') {
        return std::nullopt;
    }

    char const high = src_.next();
    if (std::isxdigit(high) != 1) {
        // Handle error
    }
    char const low = src_.next();
    if (std::isxdigit(low) != 1) {
        // Handle error
    }
    src_.next();

    auto constexpr base16 = 16;
    return static_cast<char>(
        std::stoi(std::string{high, low}, nullptr, base16));
}

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
