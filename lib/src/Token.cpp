#include "Token.h"

std::ostream& operator<<(std::ostream& oss, Token const& token) {
    return oss << std::format("{}", token);
}

std::ostream& operator<<(std::ostream& oss, TokenKind const& kind) {
    return oss << std::format("{}", kind);
}

std::ostream& operator<<(std::ostream& oss, TokenValue const& value) {
    return oss << std::format("{}", value);
}
