#pragma once

#include <functional>
#include <iostream>
#include <istream>
#include <optional>

#include "CharReader.h"
#include "Diagnostics.hpp"
#include "LexerDiagnostics.h"
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

    void skipWhile(std::function<bool(char)> const& predicate);
    std::string buildTextWhile(std::function<bool(char)> const& predicate);
};
