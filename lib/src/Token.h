#pragma once
#include <cstdint>
#include <string>
#include <variant>

#include "Position.h"
#include "magic_enum/magic_enum.hpp"

enum class TokenKind : std::uint8_t {
    // Arithmetic
    Plus,
    Minus,
    Asterisk,
    Slash,
    Percent,
    Equal,

    // Compound assignment
    PlusEqual,
    MinusEqual,
    AsteriskEqual,
    SlashEqual,
    PercentEqual,

    // Comparison
    Greater,
    GreaterEqual,
    Less,
    LessEqual,
    EqualEqual,
    ExclamationEqual,

    // Boolean arithmetic
    Exclamation,
    And,
    Or,

    // Vector operators
    At,            // @
    GreaterLess,   // ><
    ColonGreater,  // :>
    Question,      // ?

    // Parentheses
    ParenOpen,
    ParenClose,
    BracketOpen,
    BracketClose,
    BraceOpen,
    BraceClose,

    // Keywords
    Let,
    Mut,
    For,
    While,
    If,
    Else,
    In,
    Fn,
    True,
    False,
    Return,

    // Type keywords
    Bool,
    Str,
    Int,
    Float,
    Vec,

    // Others
    DotDot,    // 0..3
    DotDotEq,  // 0..=3

    Colon,
    Semicolon,
    Comma,

    // With value
    Identifier,
    Comment,

    StrLiteral,
    IntLiteral,
    FloatLiteral,

    // Special
    STX,
    ETX,
    ERROR,
};

using TokenValue =
    std::variant<std::monostate, std::string, std::int64_t, double>;

class Token {
public:
    Token(TokenKind kind, Position pos, TokenValue val = std::monostate{});

    bool operator==(Token const&) const = default;

    [[nodiscard]] TokenKind kind() const noexcept;
    [[nodiscard]] Position pos() const noexcept;
    [[nodiscard]] TokenValue const& value() const noexcept;

private:
    TokenKind kind_;
    Position pos_;
    TokenValue value_;
};

template <>
struct std::formatter<TokenKind> : std::formatter<std::string_view> {
    auto format(TokenKind kind, std::format_context& ctx) const {
        return std::formatter<std::string_view>::format(
            magic_enum::enum_name(kind), ctx);
    }
};

template <>
struct std::formatter<TokenValue> : std::formatter<std::string> {
    auto format(TokenValue const& value, std::format_context& ctx) const {
        std::string str = std::visit(
            [](auto const& val) -> std::string {
                using T = std::decay_t<decltype(val)>;
                if constexpr (std::is_same_v<T, std::monostate>) {
                    return "none";
                } else if constexpr (std::is_same_v<T, std::string>) {
                    return std::format("\"{}\"", val);
                } else {
                    return std::to_string(val);
                }
            },
            value);
        return std::formatter<std::string>::format(str, ctx);
    }
};

template <>
struct std::formatter<Token> : std::formatter<std::string> {
    auto format(Token const& token, std::format_context& ctx) const {
        return std::formatter<std::string>::format(
            std::format("{{\n  kind={},\n  pos={},\n  value={}\n}}", token.kind(),
                        token.pos(), token.value()),
            ctx);
    }
};
std::ostream& operator<<(std::ostream& oss, Token const& token);
std::ostream& operator<<(std::ostream& oss, TokenKind const& kind);
std::ostream& operator<<(std::ostream& oss, TokenValue const& value);
