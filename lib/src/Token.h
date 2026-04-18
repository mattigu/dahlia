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
    Star,
    Slash,
    Percent,

    // Compound assignment
    PlusEqual,
    MinusEqual,
    StarEqual,
    SlashEqual,
    PercentEqual,

    // Comparison
    Greater,
    GreaterEqual,
    Less,
    LessEqual,
    Equal,
    NotEqual,

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
};

using TokenValue =
    std::variant<std::monostate, std::string, std::int64_t, double>;

struct Token {
    TokenKind kind;
    Position pos;
    TokenValue value;

    bool operator==(Token const&) const = default;
};

template <>
struct std::formatter<Token> : std::formatter<std::string> {
    auto format(Token const& token, std::format_context& ctx) const {
        std::string value_str;
        std::visit(
            [&value_str](auto const& val) {
                using T = std::decay_t<decltype(val)>;
                if constexpr (std::is_same_v<T, std::monostate>) {
                    value_str = "none";
                } else if constexpr (std::is_same_v<T, std::string>) {
                    value_str = val;
                } else {
                    value_str = std::to_string(val);
                }
            },
            token.value);

        return std::formatter<std::string>::format(
            std::format("{{\n  kind={},\n  pos={},\n  value={}\n}}",
                        magic_enum::enum_name(token.kind), token.pos,
                        value_str),
            ctx);
    }
};

std::ostream& operator<<(std::ostream& oss, Token const& pos);
