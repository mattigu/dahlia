#include "Token.h"

inline bool matches(TokenKind kind, TokenValue const& val) {
    switch (kind) {
        case TokenKind::IntLiteral:
            return std::holds_alternative<std::int64_t>(val);

        case TokenKind::FloatLiteral:
            return std::holds_alternative<double>(val);

        case TokenKind::StrLiteral:
        case TokenKind::Identifier:
        case TokenKind::Comment:
            return std::holds_alternative<std::string>(val);

        default:
            return std::holds_alternative<std::monostate>(val);
    }
}

Token::Token(TokenKind kind, Position pos, TokenValue val)
    : kind_(kind), pos_(pos), value_(std::move(val)) {
    assert(matches(kind, val) && "TokenKind and TokenValue mismatch");
}

TokenKind Token::kind() const noexcept { return kind_; }
Position Token::pos() const noexcept { return pos_; }
TokenValue const& Token::value() const noexcept { return value_; }

std::ostream& operator<<(std::ostream& oss, Token const& token) {
    return oss << std::format("{}", token);
}

std::ostream& operator<<(std::ostream& oss, TokenKind const& kind) {
    return oss << std::format("{}", kind);
}

std::ostream& operator<<(std::ostream& oss, TokenValue const& value) {
    return oss << std::format("{}", value);
}
