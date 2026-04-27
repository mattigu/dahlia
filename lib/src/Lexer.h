#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <istream>
#include <limits>
#include <optional>
#include <utility>

#include "CharReader.h"
#include "Diagnostics.hpp"
#include "LexerDiagnostics.h"
#include "Position.h"
#include "Token.h"

struct LexerOptions {
    std::int64_t int_range = std::numeric_limits<std::int64_t>::max();

    std::size_t max_string_len = 128;
    std::size_t max_comment_len = 128;
    std::size_t max_identifier_len = 128;
};

class Lexer {
public:
    explicit Lexer(std::istream& src, LexerOptions const& options = {}) noexcept;

    Token next();
    [[nodiscard]] Token current() const noexcept;

    [[nodiscard]] Diagnostics<LexerDiagnosticKind> const& diagnostics()
        const noexcept;

private:
    LexerOptions options_;

    CharReader src_;

    Token current_;

    Diagnostics<LexerDiagnosticKind> diagnostics_;

    void pushDiag(LexerDiagnosticKind const& kind);
    void pushDiag(LexerDiagnosticKind const& kind, Position const& pos);

    std::optional<Token> tryBuildToken();

    std::optional<Token> tryBuildSingleCharToken();

    std::optional<Token> tryBuildComment();

    std::optional<TokenKind> tryBuildOperator();
    TokenKind extendOperator(TokenKind kind, char extend_char,
                             TokenKind extended_kind);

    std::optional<Token> tryBuildIdentifierOrKeyword();

    std::optional<Token> tryBuildString();
    std::optional<char> tryBuildEscapeSequence();
    std::optional<char> tryBuildHexEscape();

    std::optional<Token> tryBuildNumber();
    std::optional<std::string> tryBuildDigits();

    std::optional<std::int64_t> tryBuildIntValue(std::string_view integer,
                                                 Position const& pos);
    std::optional<double> tryBuildFloatValue(std::string_view integer,
                                             std::string_view fraction,
                                             Position const& pos);

    void skipWhile(std::function<bool(char)> const& predicate);
    std::pair<std::string, bool> buildTextWhile(
        std::size_t max_size, std::function<bool(char)> const& predicate);
};
