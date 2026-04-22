#pragma once

#include <cstdint>
#include <functional>
#include <iostream>
#include <istream>
#include <limits>
#include <optional>

#include "CharReader.h"
#include "Diagnostics.hpp"
#include "LexerDiagnostics.h"
#include "Position.h"
#include "Token.h"

class Lexer {
public:
    Lexer(std::istream& src, std::ostream& diagnostics = std::cerr) noexcept;

    Token next();
    [[nodiscard]] Token current() const noexcept;

    [[nodiscard]] Diagnostics<LexerDiagnosticKind> const& diagnostics()
        const noexcept;

private:
    CharReader src_;

    Token current_;

    Diagnostics<LexerDiagnosticKind> diagnostics_;

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
    std::string buildTextWhile(std::function<bool(char)> const& predicate);

    auto static constexpr INT_RANGE = std::numeric_limits<std::int64_t>::max();
};
